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
#include "melon/detail/concat_view.hpp"
#include "melon/detail/fill.hpp"
#include "melon/detail/intrusive_iterator_base.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/maps/element.hpp"
#include "melon/numeric/semiring.hpp"
#include "melon/utility/priority_queue.hpp"

namespace melon {

struct bidirectional_dijkstra_roles {
    struct vertex_status {};
    struct forward_heap_index {};
    struct reverse_heap_index {};
    struct forward_pred_arc {};
    struct reverse_pred_arc {};
};

template <typename Traits>
concept bidirectional_dijkstra_traits =
    semiring<typename Traits::semiring> &&
    updatable_priority_queue<typename Traits::heap> && requires {
        { Traits::store_paths } -> std::convertible_to<bool>;
    };

template <has_vertex_map Graph, typename ValueType>
struct bidirectional_dijkstra_default_traits {
    using semiring = shortest_path_semiring<ValueType>;
    using heap = updatable_d_ary_heap<
        2, std::pair<vertex_t<Graph>, ValueType>, typename semiring::less_t,
        vertex_map_t<Graph, std::size_t,
                     bidirectional_dijkstra_roles::forward_heap_index>,
        maps::element<1>, maps::element<0>>;

    static constexpr bool store_paths = true;
};

// One Dijkstra forward from the sources and one backward from the targets,
// stopping once no unexplored path can beat the best meeting found so far.
// Only the s-t distance and the path through that meeting vertex are exposed.
// Two preconditions on the mapped values, uncheckable by any concept: the
// first is melon::dijkstra's, an arc length must never improve a distance when
// combined; the second is the stopping criterion's, plus must be monotone in
// both arguments, since plus(u1_dist, u2_dist) over the two frontier tops is
// taken as a lower bound on every path not yet seen. Either violation silently
// returns a suboptimal dist() instead of an error.
// O((m + n) log n) with the default binary heap.
template <graph_view Graph, mapping_view<arc_t<Graph>> LengthMap,
          bidirectional_dijkstra_traits Traits =
              bidirectional_dijkstra_default_traits<
                  Graph, mapped_value_t<LengthMap, arc_t<Graph>>>>
    requires outward_incidence_graph<Graph> && inward_incidence_graph<Graph> &&
             has_vertex_map<Graph>
class bidirectional_dijkstra {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;
    // Keyed on the *arc*, as LengthMap is constrained as
    // mapping_view<arc_t<Graph>>: keying on the vertex still compiles wherever
    // a graph spells both handles unsigned int, and breaks on every other one.
    using length_type = mapped_value_t<LengthMap, arc>;

    using heap = Traits::heap;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    static_assert(std::is_same_v<typename heap::value_type,
                                 std::pair<vertex, length_type>>,
                  "bidirectional_dijkstra requires heap entries type.");
    // One heap type serves both directions, so the graph must answer the two
    // heap-index roles with the same map type; answering only one of them
    // with a projection is a mismatch here rather than a silent copy.
    static_assert(
        heap_index_map_agrees<
            heap,
            vertex_map_t<Graph, std::size_t,
                         bidirectional_dijkstra_roles::forward_heap_index>> &&
            heap_index_map_agrees<
                heap,
                vertex_map_t<Graph, std::size_t,
                             bidirectional_dijkstra_roles::reverse_heap_index>>,
        "bidirectional_dijkstra requires the heap index map to be the graph's "
        "answer for both bidirectional_dijkstra_roles heap_index roles.");

    using optional_arc = std::optional<arc>;
    using forward_pred_arcs_map =
        detail::vertex_map_if<Traits::store_paths, Graph, optional_arc,
                              bidirectional_dijkstra_roles::forward_pred_arc>;
    using reverse_pred_arcs_map =
        detail::vertex_map_if<Traits::store_paths, Graph, optional_arc,
                              bidirectional_dijkstra_roles::reverse_pred_arc>;
    struct no_optional_midpoint {};
    using optional_midpoint =
        std::conditional_t<Traits::store_paths, std::optional<vertex>,
                           no_optional_midpoint>;

private:
    Graph _graph;
    LengthMap _length_map;
    heap _forward_heap;
    heap _reverse_heap;
    vertex_map_t<Graph, std::pair<vertex_status, vertex_status>,
                 bidirectional_dijkstra_roles::vertex_status>
        _vertex_status_map;

    [[no_unique_address]] forward_pred_arcs_map _forward_pred_arcs_map;
    [[no_unique_address]] reverse_pred_arcs_map _reverse_pred_arcs_map;
    [[no_unique_address]] optional_midpoint _midpoint;
    length_type _st_dist = Traits::semiring::infty;

public:
    // ---- Construction -------------------------------------------------------

    template <graph_for<Graph> G, mapping_for<LengthMap> LM>
    constexpr bidirectional_dijkstra(G && g, LM && lm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _length_map(maps::mapping_all(std::forward<LM>(lm)))
        , _forward_heap(
              typename Traits::semiring::less_t(),
              create_vertex_map<
                  std::size_t,
                  bidirectional_dijkstra_roles::forward_heap_index>(_graph))
        , _reverse_heap(
              typename Traits::semiring::less_t(),
              create_vertex_map<
                  std::size_t,
                  bidirectional_dijkstra_roles::reverse_heap_index>(_graph))
        , _vertex_status_map(
              create_vertex_map<std::pair<vertex_status, vertex_status>,
                                bidirectional_dijkstra_roles::vertex_status>(
                  _graph, std::make_pair(PRE_HEAP, PRE_HEAP)))
        , _forward_pred_arcs_map(_graph)
        , _reverse_pred_arcs_map(_graph) {}

    template <graph_for<Graph> G, mapping_for<LengthMap> LM>
    constexpr bidirectional_dijkstra(G && g, LM && lm, const vertex & s,
                                     const vertex & t)
        : bidirectional_dijkstra(std::forward<G>(g), std::forward<LM>(lm)) {
        add_source(s);
        add_target(t);
    }

    template <typename... Args>
        requires std::constructible_from<bidirectional_dijkstra, Args...>
    constexpr bidirectional_dijkstra(Traits, Args &&... args)
        : bidirectional_dijkstra(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    constexpr bidirectional_dijkstra(const bidirectional_dijkstra &) = delete;
    constexpr bidirectional_dijkstra(bidirectional_dijkstra &&) = default;

    constexpr bidirectional_dijkstra & operator=(
        const bidirectional_dijkstra &) = delete;
    constexpr bidirectional_dijkstra & operator=(bidirectional_dijkstra &&) =
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

    constexpr bidirectional_dijkstra & reset() {
        _forward_heap.clear();
        _reverse_heap.clear();
        detail::fill(_vertex_status_map, vertices(_graph),
                     std::make_pair(PRE_HEAP, PRE_HEAP));
        if constexpr(Traits::store_paths) _midpoint.reset();
        _st_dist = Traits::semiring::infty;
        return *this;
    }
    // Strict precondition: the vertex must be untouched in the direction being
    // seeded. Re-seeding a settled one silently corrupts the stored paths and
    // the meeting distance.
    constexpr bidirectional_dijkstra & add_source(
        const vertex & s, const length_type & dist = Traits::semiring::zero) {
        assert(_vertex_status_map[s].first == PRE_HEAP);
        _forward_heap.push(std::make_pair(s, dist));
        _vertex_status_map[s].first = IN_HEAP;
        if constexpr(Traits::store_paths) _forward_pred_arcs_map[s].reset();
        return *this;
    }
    constexpr bidirectional_dijkstra & add_target(
        const vertex & t, const length_type & dist = Traits::semiring::zero) {
        assert(_vertex_status_map[t].second == PRE_HEAP);
        _reverse_heap.push(std::make_pair(t, dist));
        _vertex_status_map[t].second = IN_HEAP;
        if constexpr(Traits::store_paths) _reverse_pred_arcs_map[t].reset();
        return *this;
    }

public:
    // ---- Execution ----------------------------------------------------------

    // Idempotent: the loop resumes from the member state and re-derives the
    // same stopping condition, so a second call is a no-op.
    constexpr bidirectional_dijkstra & run() {
        while(!_forward_heap.empty() && !_reverse_heap.empty()) {
            // Copies, not reference bindings: top() returns a reference into
            // the heap array, and both distances are read after the pop()
            // below reorders it.
            const auto [u1, u1_dist] = _forward_heap.top();
            const auto [u2, u2_dist] = _reverse_heap.top();
            if(Traits::semiring::less(_st_dist,
                                      Traits::semiring::plus(u1_dist, u2_dist)))
                break;
            if(Traits::semiring::less(u1_dist, u2_dist)) {
                auto && out_arcs_range = out_arcs(_graph, u1);
                detail::prefetch_keys_and_values(
                    out_arcs_range, arc_targets_map(_graph), _length_map);
                _vertex_status_map[u1].first = POST_HEAP;
                _forward_heap.pop();
                for(const arc a : out_arcs_range) {
                    const vertex w = arc_target(_graph, a);
                    auto [w_forward_status, w_reverse_status] =
                        _vertex_status_map[w];
                    if(w_forward_status == IN_HEAP) {
                        const length_type new_w_dist =
                            Traits::semiring::plus(u1_dist, _length_map[a]);
                        if(Traits::semiring::less(new_w_dist,
                                                  _forward_heap.priority(w))) {
                            _forward_heap.promote(w, new_w_dist);
                            if(w_reverse_status == IN_HEAP) {
                                const length_type new_st_dist =
                                    Traits::semiring::plus(
                                        new_w_dist, _reverse_heap.priority(w));
                                if(Traits::semiring::less(new_st_dist,
                                                          _st_dist)) {
                                    _st_dist = new_st_dist;
                                    if constexpr(Traits::store_paths)
                                        _midpoint.emplace(w);
                                }
                            }
                            if constexpr(Traits::store_paths)
                                _forward_pred_arcs_map[w].emplace(a);
                        }
                    } else if(w_forward_status == PRE_HEAP) {
                        const length_type new_w_dist =
                            Traits::semiring::plus(u1_dist, _length_map[a]);
                        _forward_heap.push(std::make_pair(w, new_w_dist));
                        _vertex_status_map[w].first = IN_HEAP;
                        if(w_reverse_status == IN_HEAP) {
                            const length_type new_st_dist =
                                Traits::semiring::plus(
                                    new_w_dist, _reverse_heap.priority(w));
                            if(Traits::semiring::less(new_st_dist, _st_dist)) {
                                _st_dist = new_st_dist;
                                if constexpr(Traits::store_paths)
                                    _midpoint.emplace(w);
                            }
                        }
                        if constexpr(Traits::store_paths)
                            _forward_pred_arcs_map[w].emplace(a);
                    }
                }
            } else {
                auto && in_arcs_range = in_arcs(_graph, u2);
                detail::prefetch_keys_and_values(
                    in_arcs_range, arc_sources_map(_graph), _length_map);
                _vertex_status_map[u2].second = POST_HEAP;
                _reverse_heap.pop();
                for(const arc a : in_arcs_range) {
                    const vertex w = arc_source(_graph, a);
                    auto [w_forward_status, w_reverse_status] =
                        _vertex_status_map[w];
                    if(w_reverse_status == IN_HEAP) {
                        const length_type new_w_dist =
                            Traits::semiring::plus(u2_dist, _length_map[a]);
                        if(Traits::semiring::less(new_w_dist,
                                                  _reverse_heap.priority(w))) {
                            _reverse_heap.promote(w, new_w_dist);
                            if(w_forward_status == IN_HEAP) {
                                const length_type new_st_dist =
                                    Traits::semiring::plus(
                                        new_w_dist, _forward_heap.priority(w));
                                if(Traits::semiring::less(new_st_dist,
                                                          _st_dist)) {
                                    _st_dist = new_st_dist;
                                    if constexpr(Traits::store_paths)
                                        _midpoint.emplace(w);
                                }
                            }
                            if constexpr(Traits::store_paths)
                                _reverse_pred_arcs_map[w].emplace(a);
                        }
                    } else if(w_reverse_status == PRE_HEAP) {
                        const length_type new_w_dist =
                            Traits::semiring::plus(u2_dist, _length_map[a]);
                        _reverse_heap.push(std::make_pair(w, new_w_dist));
                        _vertex_status_map[w].second = IN_HEAP;
                        if(w_forward_status == IN_HEAP) {
                            const length_type new_st_dist =
                                Traits::semiring::plus(
                                    new_w_dist, _forward_heap.priority(w));
                            if(Traits::semiring::less(new_st_dist, _st_dist)) {
                                _st_dist = new_st_dist;
                                if constexpr(Traits::store_paths)
                                    _midpoint.emplace(w);
                            }
                        }
                        if constexpr(Traits::store_paths)
                            _reverse_pred_arcs_map[w].emplace(a);
                    }
                }
            }
        }
        return *this;
    }

    // ---- Queries ------------------------------------------------------------

    // The best s-t distance found; Traits::semiring::infty when no path
    // connects a source to a target. Meaningful once run() has returned.
    [[nodiscard]] constexpr length_type dist() const
        noexcept(std::is_nothrow_copy_constructible_v<length_type>) {
        return _st_dist;
    }

    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const
        requires(Traits::store_paths)
    {
        // Asserted rather than left to .value(): the source's own optional is
        // empty, so walking past it is a caller precondition violation, not an
        // exceptional condition.
        assert(_vertex_status_map[u].first != PRE_HEAP &&
               _forward_pred_arcs_map[u].has_value());
        return *_forward_pred_arcs_map[u];
    }
    [[nodiscard]] constexpr arc succ_arc(const vertex & u) const
        requires(Traits::store_paths)
    {
        // Likewise for the target, whose optional add_target() resets.
        assert(_vertex_status_map[u].second != PRE_HEAP &&
               _reverse_pred_arcs_map[u].has_value());
        return *_reverse_pred_arcs_map[u];
    }
    [[nodiscard]] constexpr bool path_found() const
        noexcept(noexcept(_midpoint.has_value()))
        requires(Traits::store_paths)
    {
        return _midpoint.has_value();
    }

private:
    class forward_path_iterator
        : public detail::intrusive_iterator_base<bidirectional_dijkstra,
                                                 std::optional<arc>> {
    public:
        using value_type = arc;
        using reference = arc;
        using detail::intrusive_iterator_base<
            bidirectional_dijkstra,
            std::optional<arc>>::intrusive_iterator_base;

        // A plain prvalue, not a `const` one: a const prvalue inhibits moves
        // and makes std::iterator_traits disagree with the `reference` typedef
        // above -- a std::indirectly_readable hazard.
        constexpr reference operator*() const { return this->_cursor.value(); }
        constexpr forward_path_iterator & operator++() {
            this->_cursor = this->_structure->_forward_pred_arcs_map[arc_source(
                this->_structure->_graph, operator*())];
            return *this;
        }
        constexpr forward_path_iterator operator++(int) {
            forward_path_iterator it(*this);
            operator++();
            return it;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const forward_path_iterator & it, std::default_sentinel_t) {
            return !it._cursor.has_value();
        }
        [[nodiscard]] constexpr friend bool operator==(
            const forward_path_iterator & it1,
            const forward_path_iterator & it2) noexcept(noexcept(it1._cursor ==
                                                                 it2._cursor)) {
            assert(it1._structure == it2._structure);
            return it1._cursor == it2._cursor;
        }
    };
    class reverse_path_iterator
        : public detail::intrusive_iterator_base<bidirectional_dijkstra,
                                                 std::optional<arc>> {
    public:
        using value_type = arc;
        using reference = arc;
        using detail::intrusive_iterator_base<
            bidirectional_dijkstra,
            std::optional<arc>>::intrusive_iterator_base;

        // A plain prvalue, not a `const` one: a const prvalue inhibits moves
        // and makes std::iterator_traits disagree with the `reference` typedef
        // above -- a std::indirectly_readable hazard.
        constexpr reference operator*() const { return this->_cursor.value(); }
        constexpr reverse_path_iterator & operator++() {
            this->_cursor = this->_structure->_reverse_pred_arcs_map[arc_target(
                this->_structure->_graph, operator*())];
            return *this;
        }
        constexpr reverse_path_iterator operator++(int) {
            reverse_path_iterator it(*this);
            operator++();
            return it;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const reverse_path_iterator & it, std::default_sentinel_t) {
            return !it._cursor.has_value();
        }
        [[nodiscard]] constexpr friend bool operator==(
            const reverse_path_iterator & it1,
            const reverse_path_iterator & it2) noexcept(noexcept(it1._cursor ==
                                                                 it2._cursor)) {
            assert(it1._structure == it2._structure);
            return it1._cursor == it2._cursor;
        }
    };

public:
    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] constexpr auto
    path() const noexcept(noexcept(detail::views::concat(
        std::ranges::subrange(
            forward_path_iterator(this,
                                  _forward_pred_arcs_map[_midpoint.value()]),
            std::default_sentinel),
        std::ranges::subrange(
            reverse_path_iterator(this,
                                  _reverse_pred_arcs_map[_midpoint.value()]),
            std::default_sentinel))))
        requires(Traits::store_paths)
    {
        assert(path_found());
        return detail::views::concat(
            std::ranges::subrange(
                forward_path_iterator(
                    this, _forward_pred_arcs_map[_midpoint.value()]),
                std::default_sentinel),
            std::ranges::subrange(
                reverse_path_iterator(
                    this, _reverse_pred_arcs_map[_midpoint.value()]),
                std::default_sentinel));
    }
};

template <typename Graph, typename LengthMap>
bidirectional_dijkstra(Graph &&, LengthMap &&)
    -> bidirectional_dijkstra<views::graph_all_t<Graph>,
                              maps::mapping_all_t<LengthMap>>;

template <typename Graph, typename LengthMap>
bidirectional_dijkstra(Graph &&, LengthMap &&, const vertex_t<Graph> &,
                       const vertex_t<Graph> &)
    -> bidirectional_dijkstra<views::graph_all_t<Graph>,
                              maps::mapping_all_t<LengthMap>>;

template <typename Graph, typename LengthMap, typename Traits>
bidirectional_dijkstra(Traits, Graph &&, LengthMap &&)
    -> bidirectional_dijkstra<views::graph_all_t<Graph>,
                              maps::mapping_all_t<LengthMap>, Traits>;

template <typename Graph, typename LengthMap, typename Traits>
bidirectional_dijkstra(Traits, Graph &&, LengthMap &&, const vertex_t<Graph> &,
                       const vertex_t<Graph> &)
    -> bidirectional_dijkstra<views::graph_all_t<Graph>,
                              maps::mapping_all_t<LengthMap>, Traits>;

}  // namespace melon
