#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/detail/fill.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace melon {

template <typename Traits>
concept breadth_first_search_traits = requires {
    { Traits::store_pred_vertices } -> std::convertible_to<bool>;
    { Traits::store_pred_arcs } -> std::convertible_to<bool>;
    { Traits::store_distances } -> std::convertible_to<bool>;
    { Traits::store_traversal_range } -> std::convertible_to<bool>;
};

struct breadth_first_search_default_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
    static constexpr bool store_traversal_range = false;
};

namespace detail {
template <typename Graph, typename Traits>
concept enable_branchless_bfs =
    has_num_vertices<Graph> && std::is_trivially_copyable_v<vertex_t<Graph>> &&
    (!Traits::store_pred_vertices && !Traits::store_pred_arcs &&
     !Traits::store_distances);
}  // namespace detail

template <graph_view Graph, breadth_first_search_traits Traits =
                                breadth_first_search_default_traits>
class breadth_first_search;

template <graph_view Graph, breadth_first_search_traits Traits>
    requires outward_adjacency_graph<Graph> && has_vertex_map<Graph> &&
             (!detail::enable_branchless_bfs<Graph, Traits>)
class breadth_first_search<Graph, Traits>
    : public algorithm_view_interface<breadth_first_search<Graph, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    static_assert(!Traits::store_pred_arcs || outward_incidence_graph<Graph>,
                  "storing predecessor arcs requires outward_incidence_graph.");

    // An index, not an iterator into _queue: begin() of the empty queue is
    // the insertion point of the first push_back, which formally invalidates
    // it ([vector.modifiers]) even when a reserve prevents reallocation --
    // exactly what checked-iterator builds flag. The index survives growth,
    // and against a reserved vector it compiles to the same walk.
    using cursor = typename std::vector<vertex>::size_type;

private:
    Graph _graph;
    std::vector<vertex> _queue;
    [[no_unique_address]]
    std::conditional_t<Traits::store_traversal_range, cursor, std::monostate>
        _queue_traversal_begin;
    cursor _queue_current;
    vertex_map_t<Graph, bool> _reached_map;

    [[no_unique_address]]
    detail::vertex_map_if<Traits::store_pred_vertices, Graph, vertex>
        _pred_vertices_map;
    [[no_unique_address]]
    detail::vertex_map_if<Traits::store_pred_arcs, Graph, arc> _pred_arcs_map;
    [[no_unique_address]]
    detail::vertex_map_if<Traits::store_distances, Graph, int> _dist_map;

public:
    // ---- Construction -------------------------------------------------------

    template <typename G>
        requires detail::not_self<G, breadth_first_search> &&
                     graph_for<G, Graph>
    constexpr explicit breadth_first_search(G && g)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _queue()
        , _reached_map(create_vertex_map<bool>(_graph, false))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _dist_map(_graph) {
        if constexpr(has_num_vertices<Graph>) {
            _queue.reserve(num_vertices(_graph));
        }
        _queue_current = 0;
    }

    template <typename G>
        requires detail::not_self<G, breadth_first_search> &&
                 graph_for<G, Graph>
    constexpr breadth_first_search(G && g, const vertex & s)
        : breadth_first_search(std::forward<G>(g)) {
        add_source(s);
    }

    template <typename... Args>
        requires std::constructible_from<breadth_first_search, Args...>
    constexpr breadth_first_search(Traits, Args &&... args)
        : breadth_first_search(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    constexpr breadth_first_search(const breadth_first_search &) = delete;
    constexpr breadth_first_search(breadth_first_search &&) = default;

    constexpr breadth_first_search & operator=(const breadth_first_search &) =
        delete;
    constexpr breadth_first_search & operator=(breadth_first_search &&) =
        default;

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

    constexpr breadth_first_search & reset() {
        _queue.resize(0);
        _queue_current = 0;
        detail::fill(_reached_map, vertices(_graph), false);
        return *this;
    }
    // Strict precondition: the vertex must not have been reached. Re-seeding
    // one queues it twice, overrunning the reserve that advance()'s
    // by-reference vertex binding depends on.
    constexpr breadth_first_search & add_source(const vertex & s) {
        assert(!_reached_map[s]);
        _queue.push_back(s);
        _reached_map[s] = true;
        if constexpr(Traits::store_pred_vertices) _pred_vertices_map[s] = s;
        if constexpr(Traits::store_distances) _dist_map[s] = 0;
        if constexpr(Traits::store_traversal_range)
            _queue_traversal_begin = _queue_current;
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _queue_current == _queue.size();
    }

private:
    // What advance() walks with, kept separate from current() so that the
    // public accessor returning by value costs the hot loop nothing.
    [[nodiscard]] constexpr const vertex & _current_ref() const noexcept {
        return _queue[_queue_current];
    }

public:
    // ---- Execution ----------------------------------------------------------

    // By value: a reference into _queue would name storage the next advance()
    // writes into. The noexcept measures the copy the by-value return performs,
    // not just reaching the element.
    [[nodiscard]] constexpr vertex current() const
        noexcept(noexcept(vertex(_current_ref()))) {
        assert(!finished());
        return _current_ref();
    }
    constexpr void advance() {
        assert(!finished());
        // By reference only where _queue is reserved to num_vertices and so
        // cannot reallocate. Without has_num_vertices there is no reserve, and
        // the push_backs below would leave this reference -- read on every
        // iteration through _pred_vertices_map / _dist_map -- dangling.
        std::conditional_t<has_num_vertices<Graph>, const vertex &, vertex> u =
            _current_ref();
        ++_queue_current;
        if constexpr(Traits::store_pred_arcs) {
            for(auto && a : out_arcs(_graph, u)) {
                const vertex & w = arc_target(_graph, a);
                if(_reached_map[w]) continue;
                _queue.push_back(w);
                _reached_map[w] = true;
                _pred_arcs_map[w] = a;
                if constexpr(Traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(Traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        } else {
            for(auto && w : out_neighbors(_graph, u)) {
                if(_reached_map[w]) continue;
                _queue.push_back(w);
                _reached_map[w] = true;
                if constexpr(Traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(Traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        }
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
    [[nodiscard]] constexpr int dist(const vertex & u) const
        noexcept(noexcept(_dist_map[u]))
        requires(Traits::store_distances)
    {
        assert(reached(u));
        return _dist_map[u];
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
    [[nodiscard]] constexpr auto dists_map() const & noexcept(
        noexcept(maps::mapping_all(_dist_map._map)))
        requires(Traits::store_distances)
    {
        return maps::mapping_all(_dist_map._map);
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
    [[nodiscard]] constexpr auto dists_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_dist_map._map))))
        requires(Traits::store_distances)
    {
        return maps::mapping_all(std::move(_dist_map._map));
    }
    // std::span<const vertex> in both specialisations: _queue is contiguous, so
    // one return type spells both cursor shapes, and the window stays read-only
    // -- the next advance() writes into it.
    [[nodiscard]] constexpr std::span<const vertex> traversal() const noexcept
        requires(Traits::store_traversal_range)
    {
        return std::span<const vertex>(
            _queue.data() + _queue_traversal_begin,
            static_cast<std::size_t>(_queue_current - _queue_traversal_begin));
    }
};

template <graph_view Graph, breadth_first_search_traits Traits>
    requires outward_adjacency_graph<Graph> && has_vertex_map<Graph> &&
             detail::enable_branchless_bfs<Graph, Traits>
class breadth_first_search<Graph, Traits>
    : public algorithm_view_interface<breadth_first_search<Graph, Traits>> {
private:
    using vertex = vertex_t<Graph>;

    Graph _graph;
    std::unique_ptr<vertex[]> _queue;
    vertex * _queue_traversal_begin;
    vertex * _queue_traversal_end;
    vertex * _queue_current;
    vertex_map_t<Graph, bool> _reached_map;

public:
    // ---- Construction -------------------------------------------------------

    template <typename G>
        requires detail::not_self<G, breadth_first_search> &&
                     graph_for<G, Graph>
    constexpr explicit breadth_first_search(G && g)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _queue(std::make_unique_for_overwrite<vertex[]>(num_vertices(_graph) +
                                                          1))
        , _queue_traversal_begin(_queue.get())
        , _queue_traversal_end(_queue.get())
        , _queue_current(_queue.get())
        , _reached_map(create_vertex_map<bool>(_graph, false)) {}

    template <typename G>
        requires detail::not_self<G, breadth_first_search> &&
                 graph_for<G, Graph>
    constexpr breadth_first_search(G && g, const vertex & s)
        : breadth_first_search(std::forward<G>(g)) {
        add_source(s);
    }

    template <typename... Args>
        requires std::constructible_from<breadth_first_search, Args...>
    constexpr breadth_first_search(Traits, Args &&... args)
        : breadth_first_search(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    // The three cursors are raw pointers into the _queue buffer, which the
    // unique_ptr hands over on a move; a copy would leave them pointing into
    // the source's buffer.
    constexpr breadth_first_search(const breadth_first_search &) = delete;
    constexpr breadth_first_search(breadth_first_search &&) = default;

    constexpr breadth_first_search & operator=(const breadth_first_search &) =
        delete;
    constexpr breadth_first_search & operator=(breadth_first_search &&) =
        default;

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

    constexpr breadth_first_search & reset() {
        _queue_traversal_begin = _queue_current = _queue_traversal_end =
            _queue.get();
        detail::fill(_reached_map, vertices(_graph), false);
        return *this;
    }
    // Strict precondition: the vertex must not have been reached. Re-seeding
    // one queues it twice, overrunning the num_vertices + 1 buffer.
    constexpr breadth_first_search & add_source(const vertex & s) {
        assert(!_reached_map[s]);
        _queue_traversal_begin = _queue_current;
        *_queue_traversal_end = s;
        ++_queue_traversal_end;
        _reached_map[s] = true;
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_queue_current == _queue_traversal_end)) {
        return _queue_current == _queue_traversal_end;
    }

    // The noexcept measures the copy the by-value return performs, not just the
    // dereference.
    [[nodiscard]] constexpr vertex current() const
        noexcept(noexcept(vertex(*_queue_current))) {
        assert(!finished());
        return *_queue_current;
    }
    constexpr void advance() {
        assert(!finished());
        // Straight off the buffer, which never reallocates here; this is the
        // hot loop.
        const vertex & u = *_queue_current;
        ++_queue_current;
        for(auto && w : out_neighbors(_graph, u)) {
            *_queue_traversal_end = w;
            _queue_traversal_end += !_reached_map[w];
            _reached_map[w] = true;
        }
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
    // but empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto reached_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_reached_map)))) {
        return maps::mapping_all(std::move(_reached_map));
    }
    [[nodiscard]] constexpr std::span<const vertex> traversal() const noexcept
        requires(Traits::store_traversal_range)
    {
        return std::span<const vertex>(
            _queue_traversal_begin,
            static_cast<std::size_t>(_queue_current - _queue_traversal_begin));
    }
};

template <typename Graph, typename Traits = breadth_first_search_default_traits>
breadth_first_search(Graph &&)
    -> breadth_first_search<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits = breadth_first_search_default_traits>
breadth_first_search(Graph &&, const vertex_t<Graph> &)
    -> breadth_first_search<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits>
breadth_first_search(Traits, Graph &&)
    -> breadth_first_search<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits>
breadth_first_search(Traits, Graph &&, const vertex_t<Graph> &)
    -> breadth_first_search<views::graph_all_t<Graph>, Traits>;

}  // namespace melon
