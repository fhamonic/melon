#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/fill.hpp"
#include "melon/detail/intrusive_iterator_base.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/maps/element.hpp"
#include "melon/numeric/semiring.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

struct dijkstra_roles {
    struct vertex_status {};
    struct heap_index {};
    struct pred_vertex {};
    struct pred_arc {};
    struct distance {};
};

template <typename Traits>
concept dijkstra_traits =
    semiring<typename Traits::semiring> &&
    updatable_priority_queue<typename Traits::heap> && requires {
        { Traits::store_distances } -> std::convertible_to<bool>;
        { Traits::store_paths } -> std::convertible_to<bool>;
    };

template <has_vertex_map Graph, typename ValueType>
struct dijkstra_default_traits {
    using semiring = shortest_path_semiring<ValueType>;
    using heap = updatable_d_ary_heap<
        2, std::pair<vertex_t<Graph>, ValueType>, typename semiring::less_t,
        vertex_map_t<Graph, std::size_t, dijkstra_roles::heap_index>,
        maps::element<1>, maps::element<0>>;

    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};

// Precondition on the mapped values, uncheckable by any concept: an arc length
// must never improve a distance when combined -- non-negative under the default
// shortest_path_semiring, in [0, 1] under most_reliable_path_semiring. Each
// vertex settles once and is never revisited, so a violation silently yields a
// wrong distance instead of an error.
// O((m + n) log n) with the default binary heap.
template <graph_view Graph, mapping_view<arc_t<Graph>> LengthMap,
          dijkstra_traits Traits = dijkstra_default_traits<
              Graph, mapped_value_t<LengthMap, arc_t<Graph>>>>
// has_vertex_map sits on the class -- the shape shared by every algorithm
// holding factory-created maps: the map members and the default traits
// name vertex_map_t at class-completion time, so a constructor-level
// constraint is consulted only after the hard error it was meant to
// prevent, and std::constructible_from probes error out instead of
// answering false.
    requires outward_incidence_graph<Graph> && has_vertex_map<Graph>
class dijkstra
    : public algorithm_view_interface<dijkstra<Graph, LengthMap, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    using length_type = mapped_value_t<LengthMap, arc_t<Graph>>;
    using traversal_entry = std::pair<vertex, length_type>;

    using heap = Traits::heap;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    static_assert(std::is_same_v<typename heap::value_type,
                                 std::pair<vertex, length_type>>,
                  "dijkstras requires heap entries type.");
    static_assert(
        heap_index_map_agrees<
            heap, vertex_map_t<Graph, std::size_t, dijkstra_roles::heap_index>>,
        "dijkstra requires the heap index map to be the graph's answer for "
        "dijkstra_roles::heap_index.");

private:
    Graph _graph;
    LengthMap _length_map;
    heap _heap;
    vertex_map_t<Graph, vertex_status, dijkstra_roles::vertex_status>
        _vertex_status_map;

    [[no_unique_address]] detail::vertex_map_if<
        Traits::store_paths && !has_arc_source<Graph>, Graph, vertex,
        dijkstra_roles::pred_vertex> _pred_vertices_map;
    [[no_unique_address]]
    detail::vertex_map_if<Traits::store_paths, Graph, std::optional<arc>,
                          dijkstra_roles::pred_arc> _pred_arcs_map;
    [[no_unique_address]]
    detail::vertex_map_if<Traits::store_distances, Graph, length_type,
                          dijkstra_roles::distance> _distances_map;

public:
    // ---- Construction -------------------------------------------------------

    // Constrained on storability into each member, so std::is_constructible
    // answers what construction actually does instead of hard-erroring in the
    // mem-initializer, outside the immediate context.
    template <graph_for<Graph> G, mapping_for<LengthMap> LM>
    constexpr dijkstra(G && g, LM && lm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _length_map(maps::mapping_all(std::forward<LM>(lm)))
        , _heap(typename Traits::semiring::less_t(),
                create_vertex_map<std::size_t, dijkstra_roles::heap_index>(
                    _graph))
        , _vertex_status_map(
              create_vertex_map<vertex_status, dijkstra_roles::vertex_status>(
                  _graph, PRE_HEAP))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _distances_map(_graph) {}

    template <graph_for<Graph> G, mapping_for<LengthMap> LM>
    constexpr dijkstra(G && g, LM && lm, const vertex & s)
        : dijkstra(std::forward<G>(g), std::forward<LM>(lm)) {
        add_source(s);
    }

    template <typename... Args>
        requires std::constructible_from<dijkstra, Args...>
    constexpr dijkstra(Traits, Args &&... args)
        : dijkstra(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    constexpr dijkstra(const dijkstra &) = delete;
    constexpr dijkstra(dijkstra &&) = default;

    constexpr dijkstra & operator=(const dijkstra &) = delete;
    constexpr dijkstra & operator=(dijkstra &&) = default;

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

    constexpr dijkstra & reset() {
        _heap.clear();
        detail::fill(_vertex_status_map, vertices(_graph), PRE_HEAP);
        return *this;
    }
    // Strict precondition: the vertex must be untouched. Re-seeding a settled
    // one silently corrupts stored paths and distances, so PRE_HEAP is asserted
    // rather than merely `!= IN_HEAP`.
    constexpr dijkstra & add_source(
        const vertex & s, const length_type & dist = Traits::semiring::zero) {
        assert(_vertex_status_map[s] == PRE_HEAP);
        _heap.push(std::make_pair(s, dist));
        _vertex_status_map[s] = IN_HEAP;
        if constexpr(Traits::store_paths) {
            _pred_arcs_map[s].reset();
            if constexpr(!has_arc_source<Graph>) _pred_vertices_map[s] = s;
        }
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_heap.empty())) {
        return _heap.empty();
    }

    // The noexcept measures the copy the by-value return performs, not just the
    // top() call.
    [[nodiscard]] constexpr traversal_entry current() const
        noexcept(noexcept(traversal_entry(_heap.top()))) {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() {
        assert(!finished());
        const auto [t, st_dist] = _heap.top();
        if constexpr(Traits::store_distances) _distances_map[t] = st_dist;
        _vertex_status_map[t] = POST_HEAP;
        auto && out_arcs_range = melon::out_arcs(_graph, t);
        detail::prefetch_keys_and_values(out_arcs_range,
                                         arc_targets_map(_graph), _length_map);
        _heap.pop();
        for(const arc & a : out_arcs_range) {
            const vertex & w = melon::arc_target(_graph, a);
            const vertex_status & w_status = _vertex_status_map[w];
            if(w_status == IN_HEAP) {
                const length_type new_dist =
                    Traits::semiring::plus(st_dist, _length_map[a]);
                if(Traits::semiring::less(new_dist, _heap.priority(w))) {
                    _heap.promote(w, new_dist);
                    if constexpr(Traits::store_paths) {
                        _pred_arcs_map[w].emplace(a);
                        if constexpr(!has_arc_source<Graph>)
                            _pred_vertices_map[w] = t;
                    }
                }
            } else if(w_status == PRE_HEAP) {
                _heap.push(std::make_pair(
                    w, Traits::semiring::plus(st_dist, _length_map[a])));
                _vertex_status_map[w] = IN_HEAP;
                if constexpr(Traits::store_paths) {
                    _pred_arcs_map[w].emplace(a);
                    if constexpr(!has_arc_source<Graph>)
                        _pred_vertices_map[w] = t;
                }
            }
        }
    }

    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] constexpr bool reached(const vertex & u) const
        noexcept(noexcept(_vertex_status_map[u] != PRE_HEAP)) {
        return _vertex_status_map[u] != PRE_HEAP;
    }
    // Refers into the algorithm, like every melon map view: valid while this
    // object lives and stays put, mapping_ref_view's contract.
    [[nodiscard]] constexpr auto reached_map() const & {
        return maps::function([this](const vertex & u) { return reached(u); });
    }
    // Terminal, like std::move(alg).base() -- the member left behind is valid
    // but empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto reached_map() && {
        return maps::function(
            [status_map = std::move(_vertex_status_map)](const vertex & u) {
                return status_map[u] != PRE_HEAP;
            });
    }
    [[nodiscard]] constexpr bool visited(const vertex & u) const
        noexcept(noexcept(_vertex_status_map[u] == POST_HEAP)) {
        return _vertex_status_map[u] == POST_HEAP;
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const
        requires(Traits::store_paths)
    {
        assert(reached(u) && _pred_arcs_map[u].has_value());
        return *_pred_arcs_map[u];
    }
    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const
        requires(Traits::store_paths)
    {
        assert(reached(u) && _pred_arcs_map[u].has_value());
        if constexpr(has_arc_source<Graph>)
            return melon::arc_source(_graph, pred_arc(u));
        else
            return _pred_vertices_map[u];
    }
    // Reads the heap, not _distances_map, so it does not need store_distances.
    [[nodiscard]] constexpr length_type current_dist(const vertex & u) const {
        assert(reached(u) && !visited(u));
        return _heap.priority(u);
    }
    [[nodiscard]] constexpr length_type dist(const vertex & u) const
        noexcept(noexcept(length_type(_distances_map[u])))
        requires(Traits::store_distances)
    {
        assert(visited(u));
        return _distances_map[u];
    }
    // reached_map()'s contract: valid while this object lives and stays put.
    // Unlike dist() it cannot assert per read, so vertices not yet visited
    // still hold indeterminate values -- read it once they are out.
    [[nodiscard]] constexpr auto dists_map() const & noexcept(
        noexcept(maps::mapping_all(_distances_map._map)))
        requires(Traits::store_distances)
    {
        return maps::mapping_all(_distances_map._map);
    }
    // Terminal, like std::move(alg).base(): the member left behind is valid but
    // empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto dists_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_distances_map._map))))
        requires(Traits::store_distances)
    {
        return maps::mapping_all(std::move(_distances_map._map));
    }

private:
    class path_iterator
        : public detail::intrusive_iterator_base<dijkstra, vertex> {
        // MSVC (through at least VS 18.6) denies hidden friends of a nested
        // class the enclosing class's private access: a sentinel friend
        // reading _pred_arcs_map directly fails to compile there.
        [[nodiscard]] constexpr bool _at_path_end() const {
            return !this->_structure->_pred_arcs_map[this->_cursor].has_value();
        }

    public:
        using value_type = arc;
        using reference = arc;
        using detail::intrusive_iterator_base<dijkstra,
                                              vertex>::intrusive_iterator_base;

        // A plain prvalue, not a `const` one: a const prvalue inhibits moves
        // and makes std::iterator_traits disagree with the `reference` typedef
        // above -- a std::indirectly_readable hazard.
        constexpr reference operator*() const {
            return this->_structure->_pred_arcs_map[this->_cursor].value();
        }
        constexpr path_iterator & operator++() {
            this->_cursor = this->_structure->pred_vertex(this->_cursor);
            return *this;
        }
        constexpr path_iterator operator++(int) {
            path_iterator it(*this);
            operator++();
            return it;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const path_iterator & it, std::default_sentinel_t) {
            return it._at_path_end();
        }
        [[nodiscard]] constexpr friend bool operator==(
            const path_iterator & it1,
            const path_iterator & it2) noexcept(noexcept(it1._cursor ==
                                                         it2._cursor)) {
            assert(it1._structure == it2._structure);
            return it1._cursor == it2._cursor;
        }
    };

public:
    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] constexpr auto path_to(const vertex & t) const
        noexcept(noexcept(std::ranges::subrange(path_iterator(this, t),
                                                std::default_sentinel)))
        requires(Traits::store_paths)
    {
        assert(reached(t));
        return std::ranges::subrange(path_iterator(this, t),
                                     std::default_sentinel);
    }
};

template <typename Graph, typename LengthMap>
dijkstra(Graph &&, LengthMap &&)
    -> dijkstra<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>>;

template <typename Graph, typename LengthMap>
dijkstra(Graph &&, LengthMap &&, const vertex_t<Graph> &)
    -> dijkstra<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>>;

template <typename Graph, typename LengthMap, typename Traits>
dijkstra(Traits, Graph &&,
         LengthMap &&) -> dijkstra<views::graph_all_t<Graph>,
                                   maps::mapping_all_t<LengthMap>, Traits>;

template <typename Graph, typename LengthMap, typename Traits>
dijkstra(Traits, Graph &&, LengthMap &&, const vertex_t<Graph> &)
    -> dijkstra<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>,
                Traits>;

}  // namespace melon