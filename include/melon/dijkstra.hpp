#ifndef MELON_DIJKSTRA_HPP
#define MELON_DIJKSTRA_HPP

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/d_ary_heap.hpp"
#include "melon/dijkstra_semirings.hpp"
#include "melon/fast_binary_heap.hpp"

#include "melon/utils/traversal_algorithm_behavior.hpp"
#include "melon/utils/traversal_algorithm_iterator.hpp"

namespace fhamonic {
namespace melon {

template <typename GR, typename LM,
          std::underlying_type_t<TraversalAlgorithmBehavior> BH =
              TraversalAlgorithmBehavior::TRACK_NONE,
          typename SR = DijkstraShortestPathSemiring<typename LM::value_type>,
          typename HP = FastBinaryHeap<
              typename GR::Node, typename LM::value_type, decltype(SR::less)>>
class Dijkstra {
public:
    using Node = GR::Node;
    using Arc = GR::Arc;

    using Value = LM::value_type;
    using DijkstraSemiringTraits = SR;
    using Heap = HP;

    static constexpr bool track_predecessor_nodes =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_NODES);
    static constexpr bool track_predecessor_arcs =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_ARCS);
    static constexpr bool track_distances =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_DISTANCES);

    using PredNodesMap =
        std::conditional<track_predecessor_nodes, typename GR::NodeMap<Node>,
                         std::monostate>::type;
    using PredArcsMap =
        std::conditional<track_predecessor_arcs, typename GR::NodeMap<Arc>,
                         std::monostate>::type;
    using DistancesMap =
        std::conditional<track_distances, typename GR::NodeMap<Value>,
                         std::monostate>::type;

private:
    const GR & _graph;
    const LM & _length_map;

    Heap _heap;
    PredNodesMap _pred_nodes_map;
    PredArcsMap _pred_arcs_map;
    DistancesMap _dist_map;

public:
    Dijkstra(const GR & g, const LM & l)
        : _graph(g), _length_map(l), _heap(g.nb_nodes()) {
        if constexpr(track_predecessor_nodes)
            _pred_nodes_map.resize(g.nb_nodes());
        if constexpr(track_predecessor_arcs)
            _pred_arcs_map.resize(g.nb_nodes());
        if constexpr(track_distances) _dist_map.resize(g.nb_nodes());
    }

    Dijkstra & reset() noexcept {
        _heap.clear();
        return *this;
    }
    Dijkstra & add_source(Node s,
                          Value dist = DijkstraSemiringTraits::zero) noexcept {
        assert(_heap.state(s) != Heap::IN_HEAP);
        _heap.push(s, dist);
        if constexpr(track_predecessor_nodes) _pred_nodes_map[s] = s;
        return *this;
    }

    bool empty_queue() const noexcept { return _heap.empty(); }

    std::pair<Node, Value> next_node() noexcept {
        const auto p = _heap.top();
        if constexpr(std::ranges::contiguous_range<decltype(_graph.out_targets(
                         p.first))>) {
            if(_graph.out_arcs(p.first).size()) {
                __builtin_prefetch(_graph.out_targets(p.first).data());
                __builtin_prefetch(
                    &_length_map[_graph.out_arcs(p.first).front()]);
            }
        }
        _heap.pop();
        for(const Arc a : _graph.out_arcs(p.first)) {
            const Node w = _graph.target(a);
            const auto s = _heap.state(w);
            if(s == Heap::IN_HEAP) {
                const Value new_dist =
                    DijkstraSemiringTraits::plus(p.second, _length_map[a]);
                if(DijkstraSemiringTraits::less(new_dist, _heap.prio(w))) {
                    _heap.decrease(w, new_dist);
                    if constexpr(track_predecessor_nodes)
                        _pred_nodes_map[w] = p.first;
                    if constexpr(track_predecessor_arcs) _pred_arcs_map[w] = a;
                }
            } else if(s == Heap::PRE_HEAP) {
                _heap.push(
                    w, DijkstraSemiringTraits::plus(p.second, _length_map[a]));
                if constexpr(track_predecessor_nodes)
                    _pred_nodes_map[w] = p.first;
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

    Node pred_node(const Node u) const noexcept
        requires(track_predecessor_nodes) {
        assert(_heap.state(u) != Heap::PRE_HEAP);
        return _pred_nodes_map[u];
    }
    Arc pred_arc(const Node u) const noexcept requires(track_predecessor_arcs) {
        assert(_heap.state(u) != Heap::PRE_HEAP);
        return _pred_arcs_map[u];
    }
    Value dist(const Node u) const noexcept requires(track_distances) {
        assert(_heap.state(u) == Heap::POST_HEAP);
        return _dist_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DIJKSTRA_HPP
