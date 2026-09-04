#pragma once

#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <stack>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/borrowed_graph.hpp"
#include "melon/detail/consumable_view.hpp"
#include "melon/detail/fill.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace melon {

struct depth_first_search_roles {
    struct reached {};
    struct pred_vertex {};
    struct pred_arc {};
    struct depth {};
};

template <typename Traits>
concept depth_first_search_traits = requires {
    { Traits::store_pred_vertices } -> std::convertible_to<bool>;
    { Traits::store_pred_arcs } -> std::convertible_to<bool>;
    { Traits::store_depth } -> std::convertible_to<bool>;
};

struct depth_first_search_default_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    // Depth in the DFS tree, not a graph distance: it counts the arcs from the
    // source along the path DFS happened to take, so on the same graph it
    // changes with the order the out-arcs come in. Deliberately not called
    // store_distances -- breadth_first_search has a flag of that name whose
    // dist() is a genuine shortest-hop distance, and the two are not
    // interchangeable.
    static constexpr bool store_depth = false;
};

template <graph_view Graph,
          depth_first_search_traits Traits = depth_first_search_default_traits>
    requires outward_adjacency_graph<Graph> && has_vertex_map<Graph>
class depth_first_search
    : public algorithm_view_interface<depth_first_search<Graph, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    static_assert(!Traits::store_pred_arcs || outward_incidence_graph<Graph>,
                  "storing predecessor arcs requires outward_incidence_graph.");

    using stack_range =
        std::conditional_t<Traits::store_pred_arcs, out_arcs_range_t<Graph>,
                           out_neighbors_range_t<Graph>>;
    using stack_cursor = detail::consumable_input_view<stack_range>;

    // Whether relocating this object requires re-aiming the cached cursors at
    // the relocated _graph. Borrowed graphs hand out ranges independent of the
    // graph object, so nothing to do. Otherwise the ranges may capture the
    // stored graph's address (views::subgraph's do), and each frame must be
    // re-asked for -- which the primary detail::consumable_input_view's
    // _consumed counter makes possible. The remaining combination -- a
    // non-borrowed graph whose incidence ranges are std-borrowed (iterators
    // into storage the graph merely owns) -- has no counter to reseek with, so
    // it gets only what the defaulted members provide: moves (storage
    // transfers), no copies.
    static constexpr bool frames_need_rebase =
        !borrowed_graph<Graph> && !std::ranges::borrowed_range<stack_range>;

private:
    Graph _graph;
    std::vector<std::pair<vertex, stack_cursor>> _stack;
    vertex_map_t<Graph, bool, depth_first_search_roles::reached> _reached_map;

    [[no_unique_address]]
    detail::vertex_map_if<Traits::store_pred_vertices, Graph, vertex,
                          depth_first_search_roles::pred_vertex>
        _pred_vertices_map;
    [[no_unique_address]]
    detail::vertex_map_if<Traits::store_pred_arcs, Graph, arc,
                          depth_first_search_roles::pred_arc> _pred_arcs_map;
    [[no_unique_address]]
    detail::vertex_map_if<Traits::store_depth, Graph, int,
                          depth_first_search_roles::depth> _depth_map;

public:
    // ---- Construction -------------------------------------------------------

    template <typename G>
        requires detail::not_self<G, depth_first_search> && graph_for<G, Graph>
    constexpr explicit depth_first_search(G && g)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _stack()
        , _reached_map(
              create_vertex_map<bool, depth_first_search_roles::reached>(_graph,
                                                                         false))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _depth_map(_graph) {
        if constexpr(has_num_vertices<Graph>) {
            _stack.reserve(num_vertices(_graph));
        }
    }

    template <typename G>
        requires detail::not_self<G, depth_first_search> && graph_for<G, Graph>
    constexpr depth_first_search(G && g, const vertex & s)
        : depth_first_search(std::forward<G>(g)) {
        add_source(s);
    }

    template <typename... Args>
        requires std::constructible_from<depth_first_search, Args...>
    constexpr depth_first_search(Traits, Args &&... args)
        : depth_first_search(std::forward<Args>(args)...) {}

    // The relocation policy, stated once: every cached cursor is keyed by the
    // vertex sitting right next to it in the frame, so after the members
    // relocate, _rebase_stack() re-asks the *new* _graph for each frame's range
    // and the _consumed counter puts the cursor back where it was. For borrowed
    // graphs (and std-borrowed incidence ranges) that loop compiles away
    // entirely, leaving exactly a memberwise move.
    //
    // Move-only; see the melon::traversal_algorithm concept for the ruling. The
    // move cannot be defaulted: a memberwise move leaves the cached ranges
    // pointing at the moved-from object's _graph member, a heap-use-after-free
    // on the next advance().
    constexpr depth_first_search(const depth_first_search &) = delete;
    constexpr depth_first_search(depth_first_search && o) noexcept(
        !frames_need_rebase && std::is_nothrow_move_constructible_v<Graph> &&
        std::is_nothrow_move_constructible_v<decltype(_reached_map)>)
        : _graph(std::move(o._graph))
        , _stack(std::move(o._stack))
        , _reached_map(std::move(o._reached_map))
        , _pred_vertices_map(std::move(o._pred_vertices_map))
        , _pred_arcs_map(std::move(o._pred_arcs_map))
        , _depth_map(std::move(o._depth_map)) {
        _rebase_stack();
    }

    constexpr depth_first_search & operator=(const depth_first_search &) =
        delete;
    constexpr depth_first_search & operator=(depth_first_search && o) noexcept(
        !frames_need_rebase && std::is_nothrow_move_assignable_v<Graph> &&
        std::is_nothrow_move_assignable_v<decltype(_reached_map)>) {
        if(this == std::addressof(o)) return *this;
        _graph = std::move(o._graph);
        _stack = std::move(o._stack);
        _reached_map = std::move(o._reached_map);
        _pred_vertices_map = std::move(o._pred_vertices_map);
        _pred_arcs_map = std::move(o._pred_arcs_map);
        _depth_map = std::move(o._depth_map);
        _rebase_stack();
        return *this;
    }

private:
    constexpr void _rebase_stack() {
        if constexpr(frames_need_rebase) {
            for(auto & [v, cursor] : _stack) {
                if constexpr(Traits::store_pred_arcs)
                    cursor.rebase(out_arcs(_graph, v));
                else
                    cursor.rebase(out_neighbors(_graph, v));
            }
        }
    }

public:
    // ---- Base access --------------------------------------------------------

    [[nodiscard]] constexpr Graph & base() & noexcept { return _graph; }
    [[nodiscard]] constexpr const Graph & base() const & noexcept {
        return _graph;
    }
    [[nodiscard]] constexpr Graph && base() && noexcept {
        return std::move(_graph);
    }
    [[nodiscard]] constexpr const Graph && base() const && noexcept {
        return std::move(_graph);
    }

    // ---- Setup --------------------------------------------------------------

    constexpr depth_first_search & reset() {
        _stack.resize(0);
        detail::fill(_reached_map, vertices(_graph), false);
        return *this;
    }
    // Strict precondition: the vertex must not have been reached. Re-seeding
    // one gives it a second stack frame, so it is traversed and emitted twice.
    constexpr depth_first_search & add_source(const vertex & s) {
        assert(!_reached_map[s]);
        if constexpr(Traits::store_pred_arcs)
            _stack.emplace_back(s, out_arcs(_graph, s));
        else
            _stack.emplace_back(s, out_neighbors(_graph, s));
        _reached_map[s] = true;
        if constexpr(Traits::store_pred_vertices) _pred_vertices_map[s] = s;
        if constexpr(Traits::store_depth) _depth_map[s] = 0;
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_stack.empty())) {
        return _stack.empty();
    }

    // The noexcept measures the copy the by-value return performs, not just
    // reaching the element.
    [[nodiscard]] constexpr vertex current() const
        noexcept(noexcept(vertex(_stack.back().first))) {
        assert(!finished());
        return _stack.back().first;
    }

    constexpr void advance() {
        assert(!finished());
        do {
            if constexpr(Traits::store_pred_arcs) {
                for(auto & remaining_arcs = _stack.back().second;
                    !remaining_arcs.empty(); remaining_arcs.advance()) {
                    auto a = remaining_arcs.current();
                    auto w = arc_target(_graph, a);
                    if(_reached_map[w]) continue;
                    _reached_map[w] = true;
                    _pred_arcs_map[w] = a;
                    if constexpr(Traits::store_pred_vertices)
                        _pred_vertices_map[w] = _stack.back().first;
                    if constexpr(Traits::store_depth)
                        _depth_map[w] = _depth_map[_stack.back().first] + 1;
                    remaining_arcs.advance();
                    _stack.emplace_back(w, out_arcs(_graph, w));
                    return;
                }
            } else {
                for(auto & remaining_neighbors = _stack.back().second;
                    !remaining_neighbors.empty();
                    remaining_neighbors.advance()) {
                    auto w = remaining_neighbors.current();
                    if(_reached_map[w]) continue;
                    _reached_map[w] = true;
                    if constexpr(Traits::store_pred_vertices)
                        _pred_vertices_map[w] = _stack.back().first;
                    if constexpr(Traits::store_depth)
                        _depth_map[w] = _depth_map[_stack.back().first] + 1;
                    remaining_neighbors.advance();
                    _stack.emplace_back(w, out_neighbors(_graph, w));
                    return;
                }
            }
            _stack.pop_back();
        } while(!_stack.empty());
    }

    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] constexpr bool reached(const vertex & u) const
        noexcept(noexcept(_reached_map[u])) {
        return _reached_map[u];
    }
    // Refers into the algorithm, like every melon map view: valid while this
    // object lives and stays put, mapping_ref_view's contract.
    [[nodiscard]] constexpr auto reached_map() const & noexcept(
        noexcept(maps::mapping_all(_reached_map))) {
        return maps::mapping_all(_reached_map);
    }
    // Terminal, like std::move(alg).base() -- the member left behind is valid
    // but empty, so no other member may be called afterwards. Same for the
    // trait-gated expiring overloads below.
    [[nodiscard]] constexpr auto reached_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_reached_map)))) {
        return maps::mapping_all(std::move(_reached_map));
    }
    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const
        noexcept(noexcept(_pred_vertices_map[u]))
        requires(Traits::store_pred_vertices)
    {
        assert(reached(u));
        return _pred_vertices_map[u];
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const
        noexcept(noexcept(_pred_arcs_map[u]))
        requires(Traits::store_pred_arcs)
    {
        assert(reached(u));
        return _pred_arcs_map[u];
    }
    // Number of arcs from the source to u along the DFS tree -- equivalently,
    // the length of the pred_vertex chain up to the source. Not a shortest-path
    // distance: two runs over the same graph disagree if the out-arcs come in a
    // different order.
    [[nodiscard]] constexpr int depth(const vertex & u) const
        noexcept(noexcept(_depth_map[u]))
        requires(Traits::store_depth)
    {
        assert(reached(u));
        return _depth_map[u];
    }
    // Views of the stored maps, reached_map()'s contract: valid while this
    // object lives and stays put. Unlike the per-vertex accessors above they
    // cannot assert per read, so unreached vertices still hold indeterminate
    // values -- read them once the vertices of interest are out.
    [[nodiscard]] constexpr auto pred_vertices_map() const & noexcept(
        noexcept(maps::mapping_all(_pred_vertices_map._map)))
        requires(Traits::store_pred_vertices)
    {
        return maps::mapping_all(_pred_vertices_map._map);
    }
    [[nodiscard]] constexpr auto pred_arcs_map() const & noexcept(
        noexcept(maps::mapping_all(_pred_arcs_map._map)))
        requires(Traits::store_pred_arcs)
    {
        return maps::mapping_all(_pred_arcs_map._map);
    }
    [[nodiscard]] constexpr auto depths_map() const & noexcept(
        noexcept(maps::mapping_all(_depth_map._map)))
        requires(Traits::store_depth)
    {
        return maps::mapping_all(_depth_map._map);
    }
    [[nodiscard]] constexpr auto pred_vertices_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_pred_vertices_map._map))))
        requires(Traits::store_pred_vertices)
    {
        return maps::mapping_all(std::move(_pred_vertices_map._map));
    }
    [[nodiscard]] constexpr auto pred_arcs_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_pred_arcs_map._map))))
        requires(Traits::store_pred_arcs)
    {
        return maps::mapping_all(std::move(_pred_arcs_map._map));
    }
    [[nodiscard]] constexpr auto depths_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_depth_map._map))))
        requires(Traits::store_depth)
    {
        return maps::mapping_all(std::move(_depth_map._map));
    }
};

template <typename Graph, typename Traits = depth_first_search_default_traits>
depth_first_search(Graph &&)
    -> depth_first_search<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits = depth_first_search_default_traits>
depth_first_search(Graph &&, const vertex_t<Graph> &)
    -> depth_first_search<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits>
depth_first_search(Traits, Graph &&)
    -> depth_first_search<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits>
depth_first_search(Traits, Graph &&, const vertex_t<Graph> &)
    -> depth_first_search<views::graph_all_t<Graph>, Traits>;

}  // namespace melon
