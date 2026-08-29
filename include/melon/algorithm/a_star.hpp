#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/intrusive_iterator_base.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/no_unique_address.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

template <typename Traits>
concept a_star_traits =
    semiring<typename Traits::semiring> &&
    updatable_priority_queue<typename Traits::heap> && requires {
        { Traits::store_distances } -> std::convertible_to<bool>;
        { Traits::store_paths } -> std::convertible_to<bool>;
    };

template <typename Graph, typename ValueType>
struct a_star_default_traits {
    using semiring = shortest_path_semiring<ValueType>;

    // Compares the combined key alone. An ordering that consults the distance
    // first (std::less on the pair) silently degrades to dijkstra's order:
    // every distance stays correct and every test passes, but the heuristic
    // stops pruning anything.
    struct priority_less {
        constexpr bool operator()(
            const std::pair<ValueType, ValueType> & p1,
            const std::pair<ValueType, ValueType> & p2) const {
            return semiring::less(p1.second, p2.second);
        }
    };
    using heap = updatable_d_ary_heap<
        2, std::pair<vertex_t<Graph>, std::pair<ValueType, ValueType>>,
        priority_less, vertex_map_t<Graph, std::size_t>, maps::element_map<1>,
        maps::element_map<0>>;

    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};

// dijkstra's traversal reordered by the combined key plus(dist, heuristic):
// with a consistent heuristic it settles the same distances while never
// examining the vertices whose key exceeds the requested targets', and yields
// vertices in key order, not distance order. current() and dist() report
// plain distances -- the key never appears in any result.
//
// Precondition on the heuristic map, uncheckable by any concept
// ("consistency"): relaxing an arc must never improve the combined key --
// plus(plus(d, length(a)), heuristic(target(a))) may not compare less than
// plus(d, heuristic(source(a))). Each vertex settles once and is never
// revisited, so a violation -- an admissible-only heuristic, or a consistent
// one eroded by floating-point rounding -- silently yields wrong distances;
// advance() asserts the inequality at every examined arc in debug builds.
//
// Deliberately no defaulted zero heuristic: that instance is dijkstra with
// double-width heap entries, so spell dijkstra instead.
// O((m + n) log n) with the default binary heap, over the explored subgraph.
template <graph_view Graph, mapping_view<arc_t<Graph>> LengthMap,
          mapping_view<vertex_t<Graph>> HeuristicMap,
          a_star_traits Traits = a_star_default_traits<
              Graph, mapped_value_t<LengthMap, arc_t<Graph>>>>
    requires outward_incidence_graph<Graph> &&
             std::same_as<mapped_value_t<HeuristicMap, vertex_t<Graph>>,
                          mapped_value_t<LengthMap, arc_t<Graph>>>
class a_star : public algorithm_view_interface<
                   a_star<Graph, LengthMap, HeuristicMap, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    using length_type = mapped_value_t<LengthMap, arc_t<Graph>>;
    using heap_priority = std::pair<length_type, length_type>;
    using traversal_entry = std::pair<vertex, length_type>;

    using heap = Traits::heap;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    static_assert(std::is_same_v<typename heap::value_type,
                                 std::pair<vertex, heap_priority>>,
                  "a_star requires heap entries pairing a vertex with a "
                  "(distance, key) priority pair.");

private:
    Graph _graph;
    LengthMap _length_map;
    HeuristicMap _heuristic_map;
    heap _heap;
    vertex_map_t<Graph, vertex_status> _vertex_status_map;

    MELON_NO_UNIQUE_ADDRESS detail::vertex_map_if<
        Traits::store_paths && !has_arc_source<Graph>, Graph, vertex>
        _pred_vertices_map;
    MELON_NO_UNIQUE_ADDRESS
    detail::vertex_map_if<Traits::store_paths, Graph, std::optional<arc>>
        _pred_arcs_map;
    MELON_NO_UNIQUE_ADDRESS
    detail::vertex_map_if<Traits::store_distances, Graph, length_type>
        _distances_map;

public:
    // ---- Construction -------------------------------------------------------

    // Constrained on storability into each member, so std::is_constructible
    // answers what construction actually does instead of hard-erroring in the
    // mem-initializer, outside the immediate context.
    template <graph_for<Graph> G, mapping_for<LengthMap> LM,
              mapping_for<HeuristicMap> HM>
        requires has_vertex_map<Graph>
    constexpr a_star(G && g, LM && lm, HM && hm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _length_map(maps::mapping_all(std::forward<LM>(lm)))
        , _heuristic_map(maps::mapping_all(std::forward<HM>(hm)))
        , _heap(typename heap::priority_compare(),
                create_vertex_map<std::size_t>(_graph))
        , _vertex_status_map(create_vertex_map<vertex_status>(_graph, PRE_HEAP))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _distances_map(_graph) {}

    template <graph_for<Graph> G, mapping_for<LengthMap> LM,
              mapping_for<HeuristicMap> HM>
    constexpr a_star(G && g, LM && lm, HM && hm, const vertex & s)
        : a_star(std::forward<G>(g), std::forward<LM>(lm),
                 std::forward<HM>(hm)) {
        add_source(s);
    }

    template <typename... Args>
        requires std::constructible_from<a_star, Args...>
    constexpr a_star(Traits, Args &&... args)
        : a_star(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    constexpr a_star(const a_star &) = delete;
    constexpr a_star(a_star &&) = default;

    constexpr a_star & operator=(const a_star &) = delete;
    constexpr a_star & operator=(a_star &&) = default;

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

    constexpr a_star & reset() {
        _heap.clear();
        _vertex_status_map.fill(PRE_HEAP);
        return *this;
    }
    // Strict precondition: the vertex must be untouched. Re-seeding a settled
    // one silently corrupts stored paths and distances, so PRE_HEAP is asserted
    // rather than merely `!= IN_HEAP`.
    constexpr a_star & add_source(
        const vertex & s, const length_type & dist = Traits::semiring::zero) {
        assert(_vertex_status_map[s] == PRE_HEAP);
        _heap.push(std::make_pair(
            s, heap_priority(dist,
                             Traits::semiring::plus(dist, _heuristic_map[s]))));
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

    // The noexcept measures the copies the by-value return performs, not just
    // the top() call.
    [[nodiscard]] constexpr traversal_entry current() const
        noexcept(noexcept(traversal_entry(_heap.top().first,
                                          _heap.top().second.first))) {
        assert(!finished());
        const auto & [u, p] = _heap.top();
        return traversal_entry(u, p.first);
    }

    constexpr void advance() {
        assert(!finished());
        const auto [t, t_prio] = _heap.top();
        if constexpr(Traits::store_distances) _distances_map[t] = t_prio.first;
        _vertex_status_map[t] = POST_HEAP;
        auto && out_arcs_range = melon::out_arcs(_graph, t);
        detail::prefetch_keys_and_values(out_arcs_range,
                                         arc_targets_map(_graph), _length_map);
        _heap.pop();
        for(const arc & a : out_arcs_range) {
            const vertex & w = melon::arc_target(_graph, a);
            const vertex_status & w_status = _vertex_status_map[w];
            // The class contract's consistency precondition, caught at the
            // offending arc -- checked before the status dispatch: the
            // violations that corrupt results land on already-settled
            // targets, which the dispatch below silently skips.
            assert(!Traits::semiring::less(
                Traits::semiring::plus(
                    Traits::semiring::plus(t_prio.first, _length_map[a]),
                    _heuristic_map[w]),
                t_prio.second));
            if(w_status == IN_HEAP) {
                const length_type new_dist =
                    Traits::semiring::plus(t_prio.first, _length_map[a]);
                if(Traits::semiring::less(new_dist, _heap.priority(w).first)) {
                    const length_type new_key =
                        Traits::semiring::plus(new_dist, _heuristic_map[w]);
                    _heap.promote(w, heap_priority(new_dist, new_key));
                    if constexpr(Traits::store_paths) {
                        _pred_arcs_map[w].emplace(a);
                        if constexpr(!has_arc_source<Graph>)
                            _pred_vertices_map[w] = t;
                    }
                }
            } else if(w_status == PRE_HEAP) {
                const length_type new_dist =
                    Traits::semiring::plus(t_prio.first, _length_map[a]);
                const length_type new_key =
                    Traits::semiring::plus(new_dist, _heuristic_map[w]);
                _heap.push(std::make_pair(w, heap_priority(new_dist, new_key)));
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
        return maps::map([this](const vertex & u) { return reached(u); });
    }
    // Terminal, like std::move(alg).base() -- the member left behind is valid
    // but empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto reached_map() && {
        return maps::map(
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
    // .first: the distance -- .second, the heap's ordering key, also compiles
    // and silently reports heuristic-shifted values.
    [[nodiscard]] constexpr length_type current_dist(const vertex & u) const {
        assert(reached(u) && !visited(u));
        return _heap.priority(u).first;
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
        : public detail::intrusive_iterator_base<a_star, vertex> {
        // MSVC (through at least VS 18.6) denies hidden friends of a nested
        // class the enclosing class's private access: a sentinel friend
        // reading _pred_arcs_map directly fails to compile there.
        [[nodiscard]] constexpr bool _at_path_end() const {
            return !this->_structure->_pred_arcs_map[this->_cursor].has_value();
        }

    public:
        using value_type = arc;
        using reference = arc;
        using detail::intrusive_iterator_base<a_star,
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

template <typename Graph, typename LengthMap, typename HeuristicMap>
a_star(Graph &&, LengthMap &&, HeuristicMap &&)
    -> a_star<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>,
              maps::mapping_all_t<HeuristicMap>>;

template <typename Graph, typename LengthMap, typename HeuristicMap>
a_star(Graph &&, LengthMap &&, HeuristicMap &&, const vertex_t<Graph> &)
    -> a_star<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>,
              maps::mapping_all_t<HeuristicMap>>;

template <typename Graph, typename LengthMap, typename HeuristicMap,
          typename Traits>
a_star(Traits, Graph &&, LengthMap &&, HeuristicMap &&)
    -> a_star<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>,
              maps::mapping_all_t<HeuristicMap>, Traits>;

template <typename Graph, typename LengthMap, typename HeuristicMap,
          typename Traits>
a_star(Traits, Graph &&, LengthMap &&, HeuristicMap &&, const vertex_t<Graph> &)
    -> a_star<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>,
              maps::mapping_all_t<HeuristicMap>, Traits>;

}  // namespace melon
