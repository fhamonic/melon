#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/concat_view.hpp"
#include "melon/detail/intrusive_iterator_base.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"

namespace melon {

// clang-format off
template <typename Traits>
concept bidirectional_dijkstra_traits = semiring<typename Traits::semiring> &&
    updatable_priority_queue<typename Traits::heap> && requires() {
    { Traits::store_paths } -> std::convertible_to<bool>;
};
// clang-format on

template <typename Graph, typename ValueType>
struct bidirectional_dijkstra_default_traits {
    using semiring = shortest_path_semiring<ValueType>;
    using heap =
        updatable_d_ary_heap<2, std::pair<vertex_t<Graph>, ValueType>,
                             typename semiring::less_t,
                             vertex_map_t<Graph, std::size_t>,
                             maps::element_map<1>, maps::element_map<0>>;

    static constexpr bool store_paths = true;
};

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
    // Keyed on the *arc*, like dijkstra and network_voronoi, and like this
    // class's own default Traits above: LengthMap is constrained as
    // mapping_view<arc_t<Graph>>, so keying on the vertex only ever worked
    // because every melon container spells both handles unsigned int.
    using length_type = mapped_value_t<LengthMap, arc>;

    using heap = Traits::heap;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    static_assert(std::is_same_v<typename heap::value_type,
                                 std::pair<vertex, length_type>>,
                  "bidirectional_dijkstra requires heap entries type.");

    using optional_arc = std::optional<arc>;
    struct no_forward_pred_arcs_map {};
    using forward_pred_arcs_map =
        vertex_map_if<Traits::store_paths, Graph, optional_arc,
                      no_forward_pred_arcs_map>;
    struct no_reverse_pred_arcs_map {};
    using reverse_pred_arcs_map =
        vertex_map_if<Traits::store_paths, Graph, optional_arc,
                      no_reverse_pred_arcs_map>;
    struct no_optional_midpoint {};
    using optional_midpoint =
        std::conditional_t<Traits::store_paths, std::optional<vertex>,
                           no_optional_midpoint>;

private:
    Graph _graph;
    LengthMap _length_map;
    heap _forward_heap;
    heap _reverse_heap;
    vertex_map_t<Graph, std::pair<vertex_status, vertex_status>>
        _vertex_status_map;

    [[no_unique_address]] forward_pred_arcs_map _forward_pred_arcs_map;
    [[no_unique_address]] reverse_pred_arcs_map _reverse_pred_arcs_map;
    [[no_unique_address]] optional_midpoint _midpoint;
    // The best s-t distance found so far; infty until a meeting point exists.
    // A member rather than a local of run(), which is what makes run()
    // idempotent -- re-running used to re-seed a local to infty and return
    // that -- and what lets dist() answer after the run, where the result
    // used to be gone the moment run()'s return value was discarded.
    length_type _st_dist = Traits::semiring::infty;

public:
    // graph_storable_as / mapping_storable_as, so std::is_constructible
    // answers what the mem-initializers actually do -- see dijkstra's
    // constructor.
    template <typename G, typename M>
        requires graph_storable_as<G, Graph> &&
                     mapping_storable_as<M, LengthMap>
    constexpr bidirectional_dijkstra(G && g, M && l)
        : _graph(detail::store_graph<Graph>(std::forward<G>(g)))
        , _length_map(detail::store_mapping<LengthMap>(std::forward<M>(l)))
        , _forward_heap(typename Traits::semiring::less_t(),
                        create_vertex_map<std::size_t>(_graph))
        , _reverse_heap(typename Traits::semiring::less_t(),
                        create_vertex_map<std::size_t>(_graph))
        // Through the CPO, like every other algorithm: calling the member
        // directly rejects a graph whose maps come from ADL, although
        // has_vertex_map -- which this class requires -- accepts one.
        , _vertex_status_map(
              create_vertex_map<std::pair<vertex_status, vertex_status>>(
                  _graph, std::make_pair(PRE_HEAP, PRE_HEAP)))
        , _forward_pred_arcs_map(_graph)
        , _reverse_pred_arcs_map(_graph) {}

    template <typename G, typename M>
        requires graph_storable_as<G, Graph> &&
                 mapping_storable_as<M, LengthMap>
    constexpr bidirectional_dijkstra(G && g, M && l, const vertex & s,
                                     const vertex & t)
        : bidirectional_dijkstra(std::forward<G>(g), std::forward<M>(l)) {
        add_source(s);
        add_target(t);
    }

    template <typename... Args>
        requires std::constructible_from<bidirectional_dijkstra, Args...>
    constexpr bidirectional_dijkstra(Traits, Args &&... args)
        : bidirectional_dijkstra(std::forward<Args>(args)...) {}

    constexpr bidirectional_dijkstra(const bidirectional_dijkstra &) = default;
    constexpr bidirectional_dijkstra(bidirectional_dijkstra &&) = default;

    constexpr bidirectional_dijkstra & operator=(
        const bidirectional_dijkstra &) = default;
    constexpr bidirectional_dijkstra & operator=(bidirectional_dijkstra &&) =
        default;

    // The graph the algorithm runs over. An algorithm owns its view rather
    // than adapting it, so this is the std::ranges::owning_view shape --
    // references, ref-qualified -- and not the filter_view shape the graph
    // *views* use. Returning a copy here would also put traversal_forest back
    // where it started: it reaches its sources through base(), and an owned
    // graph view is move-only.
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

    bidirectional_dijkstra & reset() {
        _forward_heap.clear();
        _reverse_heap.clear();
        _vertex_status_map.fill(std::make_pair(PRE_HEAP, PRE_HEAP));
        if constexpr(Traits::store_paths) _midpoint.reset();
        _st_dist = Traits::semiring::infty;
        return *this;
    }
    bidirectional_dijkstra & add_source(
        const vertex & s, const length_type dist = Traits::semiring::zero) {
        assert(_vertex_status_map[s].first == PRE_HEAP);
        _forward_heap.push(std::make_pair(s, dist));
        _vertex_status_map[s].first = IN_HEAP;
        if constexpr(Traits::store_paths) _forward_pred_arcs_map[s].reset();
        return *this;
    }
    bidirectional_dijkstra & add_target(
        const vertex & t, const length_type dist = Traits::semiring::zero) {
        assert(_vertex_status_map[t].second == PRE_HEAP);
        _reverse_heap.push(std::make_pair(t, dist));
        _vertex_status_map[t].second = IN_HEAP;
        if constexpr(Traits::store_paths) _reverse_pred_arcs_map[t].reset();
        return *this;
    }

public:
    // Returns *this like every run() in the family; the computed distance is
    // read through dist() afterwards. Idempotent: the loop below resumes from
    // the member state and immediately re-derives the same stopping
    // condition, so a second call is a no-op.
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
                const auto & out_arcs_range = out_arcs(_graph, u1);
                prefetch_keys_and_values(out_arcs_range,
                                         arc_targets_map(_graph), _length_map);
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
                const auto & in_arcs_range = in_arcs(_graph, u2);
                prefetch_keys_and_values(in_arcs_range, arc_sources_map(_graph),
                                         _length_map);
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

    // The best s-t distance found; Traits::semiring::infty when no path
    // connects a source to a target. Meaningful once run() has returned.
    [[nodiscard]] constexpr length_type dist() const noexcept {
        return _st_dist;
    }

    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const
        requires(Traits::store_paths)
    {
        // See dijkstra::pred_arc: .value() would throw for the source, whose
        // optional add_source() resets, where the caller has simply violated
        // a precondition.
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
        : public intrusive_iterator_base<bidirectional_dijkstra,
                                         std::optional<arc>> {
    public:
        using value_type = arc;
        using reference = arc;
        using intrusive_iterator_base<
            bidirectional_dijkstra,
            std::optional<arc>>::intrusive_iterator_base;

        // Returns a plain prvalue, not a `const` one: a const prvalue
        // inhibits moves and makes std::iterator_traits disagree with the
        // `reference` typedef right above, which is a
        // std::indirectly_readable hazard.
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
    };
    class reverse_path_iterator
        : public intrusive_iterator_base<bidirectional_dijkstra,
                                         std::optional<arc>> {
    public:
        using value_type = arc;
        using reference = arc;
        using intrusive_iterator_base<
            bidirectional_dijkstra,
            std::optional<arc>>::intrusive_iterator_base;

        // Returns a plain prvalue, not a `const` one: a const prvalue
        // inhibits moves and makes std::iterator_traits disagree with the
        // `reference` typedef right above, which is a
        // std::indirectly_readable hazard.
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
    };

public:
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

// No Traits parameter: the class template's own default computes it, so the
// deduced type and the explicitly written `bidirectional_dijkstra<G, LM>`
// agree.
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
