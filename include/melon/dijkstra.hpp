#ifndef MELON_DIJKSTRA_HPP
#define MELON_DIJKSTRA_HPP

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/concepts/graph_concepts.hpp"

#include "melon/d_ary_heap.hpp"
#include "melon/dijkstra_semirings.hpp"
#include "melon/fast_binary_heap.hpp"

#include "melon/utils/traversal_algorithm_behavior.hpp"
#include "melon/utils/traversal_algorithm_iterator.hpp"

namespace fhamonic {
namespace melon {

template <concepts::adjacency_list_graph GR, typename LM,
          std::underlying_type_t<TraversalAlgorithmBehavior> BH =
              TraversalAlgorithmBehavior::TRACK_NONE,
          typename SR = DijkstraShortestPathSemiring<typename LM::value_type>,
          typename HP = FastBinaryHeap<
              typename GR::vertex, typename LM::value_type, decltype(SR::less)>>
class Dijkstra {
public:
    using vertex = GR::vertex;
    using arc = GR::arc;

    using Value = LM::value_type;
    using DijkstraSemiringTraits = SR;
    using Heap = HP;

    static constexpr bool track_predecessor_vertices =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_NODES);
    static constexpr bool track_predecessor_arcs =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_ARCS);
    static constexpr bool track_distances =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_DISTANCES);

    using PredverticesMap =
        std::conditional<track_predecessor_vertices, typename GR::vertexMap<vertex>,
                         std::monostate>::type;
    using PredarcsMap =
        std::conditional<track_predecessor_arcs, typename GR::vertexMap<arc>,
                         std::monostate>::type;
    using DistancesMap =
        std::conditional<track_distances, typename GR::vertexMap<Value>,
                         std::monostate>::type;

private:
    const GR & _graph;
    const LM & _length_map;

    Heap _heap;
    PredverticesMap _pred_vertices_map;
    PredarcsMap _pred_arcs_map;
    DistancesMap _dist_map;

public:
    Dijkstra(const GR & g, const LM & l)
        : _graph(g), _length_map(l), _heap(g.nb_vertices()) {
        if constexpr(track_predecessor_vertices)
            _pred_vertices_map.resize(g.nb_vertices());
        if constexpr(track_predecessor_arcs)
            _pred_arcs_map.resize(g.nb_vertices());
        if constexpr(track_distances) _dist_map.resize(g.nb_vertices());
    }

    Dijkstra & reset() noexcept {
        _heap.clear();
        return *this;
    }
    Dijkstra & add_source(vertex s,
                          Value dist = DijkstraSemiringTraits::zero) noexcept {
        assert(_heap.state(s) != Heap::IN_HEAP);
        _heap.push(s, dist);
        if constexpr(track_predecessor_vertices) _pred_vertices_map[s] = s;
        return *this;
    }

    bool empty_queue() const noexcept { return _heap.empty(); }

    std::pair<vertex, Value> next_node() noexcept {
        const auto p = _heap.top();
        if constexpr(std::ranges::contiguous_range<decltype(_graph.out_neighbors(
                         p.first))>) {
            if(_graph.out_arcs(p.first).size()) {
                __builtin_prefetch(_graph.out_neighbors(p.first).data());
                __builtin_prefetch(
                    &_length_map[_graph.out_arcs(p.first).front()]);
            }
        }
        _heap.pop();
        for(const arc a : _graph.out_arcs(p.first)) {
            const vertex w = _graph.target(a);
            const auto s = _heap.state(w);
            if(s == Heap::IN_HEAP) {
                const Value new_dist =
                    DijkstraSemiringTraits::plus(p.second, _length_map[a]);
                if(DijkstraSemiringTraits::less(new_dist, _heap.prio(w))) {
                    _heap.decrease(w, new_dist);
                    if constexpr(track_predecessor_vertices)
                        _pred_vertices_map[w] = p.first;
                    if constexpr(track_predecessor_arcs) _pred_arcs_map[w] = a;
                }
            } else if(s == Heap::PRE_HEAP) {
                _heap.push(
                    w, DijkstraSemiringTraits::plus(p.second, _length_map[a]));
                if constexpr(track_predecessor_vertices)
                    _pred_vertices_map[w] = p.first;
                if constexpr(track_predecessor_arcs) _pred_arcs_map[w] = a;
            }
        }
        if constexpr(track_distances) _dist_map[p.first] = p.second;
        return p;
    }

    void run() noexcept {
        while(!empty_queue()) next_node();
    }
    auto begin() noexcept { return traversal_algorithm_iterator(*this); }
    auto end() noexcept { return traversal_algorithm_end_iterator(); }

    vertex pred_node(const vertex u) const noexcept
        requires(track_predecessor_vertices) {
        assert(_heap.state(u) != Heap::PRE_HEAP);
        return _pred_vertices_map[u];
    }
    arc pred_arc(const vertex u) const noexcept requires(track_predecessor_arcs) {
        assert(_heap.state(u) != Heap::PRE_HEAP);
        return _pred_arcs_map[u];
    }
    Value dist(const vertex u) const noexcept requires(track_distances) {
        assert(_heap.state(u) == Heap::POST_HEAP);
        return _dist_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DIJKSTRA_HPP
