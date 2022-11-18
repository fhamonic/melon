#ifndef MELON_ALGORITHM_DIJKSTRA_HPP
#define MELON_ALGORITHM_DIJKSTRA_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/algorithm/dijkstra_semirings.hpp"
#include "melon/concepts/graph_concepts.hpp"
#include "melon/concepts/priority_queue.hpp"
#include "melon/data_structures/d_ary_heap.hpp"
#include "melon/data_structures/fast_binary_heap.hpp"
#include "melon/utils/constexpr_ternary.hpp"
#include "melon/utils/prefetch.hpp"
#include "melon/utils/traversal_algorithm_behavior.hpp"
#include "melon/utils/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

namespace concepts {
template <typename T>
concept dijkstra_trait = semiring<typename T::semiring> &&
    updatable_priority_queue<typename T::heap> && requires() {
    { T::store_pred_vertices } -> std::convertible_to<bool>;
    { T::store_pred_arcs } -> std::convertible_to<bool>;
    { T::store_distances } -> std::convertible_to<bool>;
};
}  // namespace concepts

template <concepts::graph GR, typename LM>
requires concepts::incidence_list_graph<GR> && concepts::has_arc_target<GR>
struct dijkstra_default_traits {
    using semiring = shortest_path_semiring<typename LM::value_type>;
    using heap =
        fast_binary_heap<typename GR::vertex_t, typename LM::value_type,
                         decltype(semiring::less)>;

    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
};

template <concepts::graph GR, typename LM,
          concepts::dijkstra_trait TR = dijkstra_default_traits<GR, LM>>
requires concepts::incidence_list_graph<GR> && concepts::has_arc_target<GR>
class Dijkstra {
public:
    using vertex_t = GR::vertex_t;
    using arc_t = GR::arc_t;
    using value_t = LM::value_type;
    using traits = TR;
    using traversal_entry = typename TR::heap::entry;

private:
    using heap = traits::heap;
    using PredVerticesMap = std::conditional<traits::store_pred_vertices,
                                             typename GR::vertex_map<vertex_t>,
                                             std::monostate>::type;
    using PredArcsMap =
        std::conditional<traits::store_pred_arcs,
                         typename GR::vertex_map<arc_t>, std::monostate>::type;
    using DistancesMap = std::conditional<traits::store_distances,
                                          typename GR::vertex_map<value_t>,
                                          std::monostate>::type;

private:
    const GR & _graph;
    const LM & _length_map;

    heap _heap;
    PredVerticesMap _pred_vertices_map;
    PredArcsMap _pred_arcs_map;
    DistancesMap _distances_map;

public:
    Dijkstra(const GR & g, const LM & l)
        : _graph(g)
        , _length_map(l)
        , _heap(g.nb_vertices())
        , _pred_vertices_map(constexpr_ternary<traits::store_pred_vertices>(
              g.nb_vertices(), std::monostate{}))
        , _pred_arcs_map(constexpr_ternary<traits::store_pred_arcs>(
              g.nb_vertices(), std::monostate{}))
        , _distances_map(constexpr_ternary<traits::store_distances>(
              g.nb_vertices(), std::monostate{})) {}

    Dijkstra & reset() noexcept {
        _heap.clear();
        return *this;
    }
    Dijkstra & add_source(vertex_t s,
                          value_t dist = traits::semiring::zero) noexcept {
        assert(_heap.state(s) != heap::IN_HEAP);
        _heap.push(s, dist);
        if constexpr(traits::store_pred_vertices) _pred_vertices_map[s] = s;
        return *this;
    }

    bool empty_queue() const noexcept { return _heap.empty(); }

    traversal_entry next_vertex() noexcept {
        const traversal_entry p = _heap.top();
        prefetch_range(_graph.out_arcs(p.first));
        prefetch_map_values(_graph.out_arcs(p.first), _graph.targets_map());
        prefetch_map_values(_graph.out_arcs(p.first), _length_map);
        _heap.pop();
        for(const arc_t a : _graph.out_arcs(p.first)) {
            const vertex_t w = _graph.target(a);
            const auto s = _heap.state(w);
            if(s == heap::IN_HEAP) {
                const value_t new_dist =
                    traits::semiring::plus(p.second, _length_map[a]);
                if(traits::semiring::less(new_dist, _heap.priority(w))) {
                    _heap.decrease(w, new_dist);
                    if constexpr(traits::store_pred_vertices)
                        _pred_vertices_map[w] = p.first;
                    if constexpr(traits::store_pred_arcs) _pred_arcs_map[w] = a;
                }
            } else if(s == heap::PRE_HEAP) {
                _heap.push(w, traits::semiring::plus(p.second, _length_map[a]));
                if constexpr(traits::store_pred_vertices)
                    _pred_vertices_map[w] = p.first;
                if constexpr(traits::store_pred_arcs) _pred_arcs_map[w] = a;
            }
        }
        if constexpr(traits::store_distances)
            _distances_map[p.first] = p.second;
        return p;
    }

    void run() noexcept {
        while(!empty_queue()) next_vertex();
    }
    auto begin() noexcept { return traversal_iterator(*this); }
    auto end() noexcept { return traversal_end_sentinel(); }

    vertex_t pred_vertex(const vertex_t u) const noexcept
        requires(traits::store_pred_vertices) {
        assert(_heap.state(u) != heap::PRE_HEAP);
        return _pred_vertices_map[u];
    }
    arc_t pred_arc(const vertex_t u) const noexcept
        requires(traits::store_pred_arcs) {
        assert(_heap.state(u) != heap::PRE_HEAP);
        return _pred_arcs_map[u];
    }
    value_t dist(const vertex_t u) const noexcept
        requires(traits::store_distances) {
        assert(_heap.state(u) == heap::POST_HEAP);
        return _distances_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_DIJKSTRA_HPP
