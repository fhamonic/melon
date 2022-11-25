#ifndef MELON_ALGORITHM_BIDIRECTIONAL_DIJKSTA_HPP
#define MELON_ALGORITHM_BIDIRECTIONAL_DIJKSTA_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/concepts/graph_concepts.hpp"
#include "melon/concepts/key_value_map.hpp"
#include "melon/concepts/priority_queue.hpp"
#include "melon/data_structures/d_ary_heap.hpp"
#include "melon/utils/constexpr_ternary.hpp"
#include "melon/utils/prefetch.hpp"
#include "melon/utils/semirings.hpp"
#include "melon/utils/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
namespace concepts {
template <typename T>
concept bidirectional_dijkstra_trait = semiring<typename T::semiring> &&
    updatable_priority_queue<typename T::heap> && requires() {
    { T::store_pred_vertices } -> std::convertible_to<bool>;
    { T::store_pred_arcs } -> std::convertible_to<bool>;
};
}  // namespace concepts
// clang-format on

template <typename G, typename L>
struct bidirectional_dijkstra_default_traits {
    using semiring =
        shortest_path_semiring<mapped_value_t<L, graph_vertex_t<G>>>;
    using heap =
        d_ary_heap<2, graph_vertex_t<G>, mapped_value_t<L, graph_vertex_t<G>>,
                   decltype(semiring::less), graph_vertex_map<G, std::size_t>>;

    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
};

template <concepts::incidence_list_graph G,
          concepts::map_of<graph_vertex_t<G>> L,
          concepts::bidirectional_dijkstra_trait T =
              bidirectional_dijkstra_default_traits<G, L>>
requires concepts::has_vertex_map<G> &&
    concepts::reversible_incidence_list_graph<G>
class bidirectional_dijkstra {
public:
    using vertex_t = graph_vertex_t<G>;
    using arc_t = graph_arc_t<G>;
    using value_t = mapped_value_t<L, graph_vertex_t<G>>;
    using traversal_entry = std::pair<vertex_t, value_t>;
    using traits = T;

    static_assert(std::is_same_v<traversal_entry, typename traits::heap::entry>,
                  "traversal_entry != heap_entry");

private:
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    using heap = traits::heap;
    using vertex_status_map =
        graph_vertex_map<G, std::pair<vertex_status, vertex_status>>;
    using pred_vertices_map =
        std::conditional<traits::store_pred_vertices,
                         graph_vertex_map<G, vertex_t>, std::monostate>::type;
    using pred_arcs_map =
        std::conditional<traits::store_pred_arcs, graph_vertex_map<G, arc_t>,
                         std::monostate>::type;

private:
    const G & _graph;
    const L & _length_map;

    heap _forward_heap;
    heap _reverse_heap;
    vertex_status_map _vertex_status_map;
    pred_vertices_map _pred_vertices_map;
    pred_arcs_map _pred_arcs_map;

public:
    bidirectional_dijkstra(const G & g, const L & l)
        : _graph(g)
        , _length_map(l)
        , _forward_heap(g.template create_vertex_map<std::size_t>())
        , _reverse_heap(g.template create_vertex_map<std::size_t>())
        , _vertex_status_map(g.template create_vertex_map<
                             std::pair<vertex_status, vertex_status>>(
              std::make_pair(PRE_HEAP, PRE_HEAP)))
        , _pred_vertices_map(constexpr_ternary<traits::store_pred_vertices>(
              g.template create_vertex_map<vertex_t>(), std::monostate{}))
        , _pred_arcs_map(constexpr_ternary<traits::store_pred_arcs>(
              g.template create_vertex_map<arc_t>(), std::monostate{})) {}

    bidirectional_dijkstra(const G & g, const L & l, const vertex_t s,
                           const vertex_t t)
        : bidirectional_dijkstra(g, l) {
        add_forward_source(s);
        add_reverse_source(t);
    }

    bidirectional_dijkstra & reset() noexcept {
        _forward_heap.clear();
        _reverse_heap.clear();
        for(auto && u : _graph.vertices())
            _vertex_status_map[u] = std::make_pair(PRE_HEAP, PRE_HEAP);
        return *this;
    }
    bidirectional_dijkstra & add_forward_source(
        vertex_t s, value_t dist = traits::semiring::zero) noexcept {
        assert(_vertex_status_map[s].first == PRE_HEAP);
        _forward_heap.push(s, dist);
        _vertex_status_map[s].first = IN_HEAP;
        if constexpr(traits::store_pred_vertices) _pred_vertices_map[s] = s;
        return *this;
    }
    bidirectional_dijkstra & add_reverse_source(
        vertex_t s, value_t dist = traits::semiring::zero) noexcept {
        assert(_vertex_status_map[s].second == PRE_HEAP);
        _reverse_heap.push(s, dist);
        _vertex_status_map[s].second = IN_HEAP;
        if constexpr(traits::store_pred_vertices) _pred_vertices_map[s] = s;
        return *this;
    }

public:
    value_t run() noexcept {
        value_t st_dist = traits::semiring::infty;
        while(!_forward_heap.empty() && !_reverse_heap.empty()) {
            const auto && [u1, u1_dist] = _forward_heap.top();
            const auto && [u2, u2_dist] = _reverse_heap.top();
            if(traits::semiring::less(st_dist,
                                      traits::semiring::plus(u1_dist, u2_dist)))
                break;
            if(traits::semiring::less(u1_dist, u2_dist)) {
                prefetch_range(_graph.out_arcs(u1));
                prefetch_map_values(_graph.out_arcs(u1), _graph.targets_map());
                prefetch_map_values(_graph.out_arcs(u1), _length_map);
                _vertex_status_map[u1].first = POST_HEAP;
                _forward_heap.pop();
                for(const arc_t a : _graph.out_arcs(u1)) {
                    const vertex_t w = _graph.target(a);
                    auto [w_forward_status, w_reverse_status] =
                        _vertex_status_map[w];
                    if(w_forward_status == IN_HEAP) {
                        const value_t new_w_dist =
                            traits::semiring::plus(u1_dist, _length_map[a]);
                        if(traits::semiring::less(new_w_dist,
                                                  _forward_heap.priority(w))) {
                            _forward_heap.promote(w, new_w_dist);
                            if(w_reverse_status == IN_HEAP) {
                                const value_t new_st_dist =
                                    new_w_dist + _reverse_heap.priority(w);
                                if(traits::semiring::less(new_st_dist, st_dist))
                                    st_dist = new_st_dist;
                            }
                            if constexpr(traits::store_pred_vertices)
                                _pred_vertices_map[w] = u1;
                            if constexpr(traits::store_pred_arcs)
                                _pred_arcs_map[w] = a;
                        }
                    } else if(w_forward_status == PRE_HEAP) {
                        const value_t new_w_dist =
                            traits::semiring::plus(u1_dist, _length_map[a]);
                        _forward_heap.push(w, new_w_dist);
                        _vertex_status_map[w].first = IN_HEAP;
                        if(w_reverse_status == IN_HEAP) {
                            const value_t new_st_dist =
                                new_w_dist + _reverse_heap.priority(w);
                            if(traits::semiring::less(new_st_dist, st_dist))
                                st_dist = new_st_dist;
                        }
                        if constexpr(traits::store_pred_vertices)
                            _pred_vertices_map[w] = u2;
                        if constexpr(traits::store_pred_arcs)
                            _pred_arcs_map[w] = a;
                    }
                }
            } else {
                prefetch_range(_graph.in_arcs(u2));
                prefetch_map_values(_graph.in_arcs(u2), _graph.sources_map());
                prefetch_map_values(_graph.in_arcs(u2), _length_map);
                _vertex_status_map[u2].second = POST_HEAP;
                _reverse_heap.pop();
                for(const arc_t a : _graph.in_arcs(u2)) {
                    const vertex_t w = _graph.source(a);
                    auto [w_forward_status, w_reverse_status] =
                        _vertex_status_map[w];
                    if(w_reverse_status == IN_HEAP) {
                        const value_t new_w_dist =
                            traits::semiring::plus(u2_dist, _length_map[a]);
                        if(traits::semiring::less(new_w_dist,
                                                  _reverse_heap.priority(w))) {
                            _reverse_heap.promote(w, new_w_dist);
                            if(w_forward_status == IN_HEAP) {
                                const value_t new_st_dist =
                                    new_w_dist + _forward_heap.priority(w);
                                if(traits::semiring::less(new_st_dist, st_dist))
                                    st_dist = new_st_dist;
                            }
                            if constexpr(traits::store_pred_vertices)
                                _pred_vertices_map[w] = u2;
                            if constexpr(traits::store_pred_arcs)
                                _pred_arcs_map[w] = a;
                        }
                    } else if(w_reverse_status == PRE_HEAP) {
                        const value_t new_w_dist =
                            traits::semiring::plus(u2_dist, _length_map[a]);
                        _reverse_heap.push(w, new_w_dist);
                        _vertex_status_map[w].second = IN_HEAP;
                        if(w_forward_status == IN_HEAP) {
                            const value_t new_st_dist =
                                new_w_dist + _forward_heap.priority(w);
                            if(traits::semiring::less(new_st_dist, st_dist))
                                st_dist = new_st_dist;
                        }
                        if constexpr(traits::store_pred_vertices)
                            _pred_vertices_map[w] = u2;
                        if constexpr(traits::store_pred_arcs)
                            _pred_arcs_map[w] = a;
                    }
                }
            }
        }
        return st_dist;
    }
    auto begin() noexcept { return traversal_iterator(*this); }
    auto end() noexcept { return traversal_end_sentinel(); }

    vertex_t pred_vertex(const vertex_t u) const noexcept
        requires(traits::store_pred_vertices) {
        assert(_vertex_status_map[u] != PRE_HEAP);
        return _pred_vertices_map[u];
    }
    arc_t pred_arc(const vertex_t u) const noexcept
        requires(traits::store_pred_arcs) {
        assert(_vertex_status_map[u] != PRE_HEAP);
        return _pred_arcs_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_BIDIRECTIONAL_DIJKSTA_HPP
