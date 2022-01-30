#ifndef MELON_DIJKSTRA_HPP
#define MELON_DIJKSTRA_HPP

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <variant>
#include <vector>

#include "melon/d_ary_heap.hpp"
#include "melon/dijkstra_semirings.hpp"
#include "melon/fast_binary_heap.hpp"

#include "melon/node_search_behavior.hpp"

namespace fhamonic {
namespace melon {

template <typename GR, typename LM,
          std::underlying_type_t<NodeSeachBehavior> BH =
              NodeSeachBehavior::TRACK_NONE,
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
        static_cast<bool>(BH & NodeSeachBehavior::TRACK_PRED_NODES);
    static constexpr bool track_predecessor_arcs =
        static_cast<bool>(BH & NodeSeachBehavior::TRACK_PRED_ARCS);
    static constexpr bool track_distances =
        static_cast<bool>(BH & NodeSeachBehavior::TRACK_DISTANCES);

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
    const GR & graph;
    const LM & length_map;

    Heap heap;
    PredNodesMap pred_nodes_map;
    PredArcsMap pred_arcs_map;
    DistancesMap dist_map;

public:
    Dijkstra(const GR & g, const LM & l)
        : graph(g), length_map(l), heap(g.nb_nodes()) {
        if constexpr(track_predecessor_nodes)
            pred_nodes_map.resize(g.nb_nodes());
        if constexpr(track_predecessor_arcs) pred_arcs_map.resize(g.nb_nodes());
        if constexpr(track_distances) dist_map.resize(g.nb_nodes());
    }

    Dijkstra & reset() noexcept {
        heap.clear();
        return *this;
    }
    Dijkstra & addSource(Node s,
                         Value dist = DijkstraSemiringTraits::zero) noexcept {
        assert(heap.state(s) != Heap::IN_HEAP);
        heap.push(s, dist);
        if constexpr(track_predecessor_nodes) pred_nodes_map[s] = s;
        return *this;
    }

    bool emptyQueue() const noexcept { return heap.empty(); }

    std::pair<Node, Value> processNextNode() noexcept {
        const auto p = heap.pop();
        for(Arc a : graph.out_arcs(p.first)) {
            Node w = graph.target(a);
            const auto s = heap.state(w);
            if(s == Heap::IN_HEAP) {
                Value new_dist =
                    DijkstraSemiringTraits::plus(p.second, length_map[a]);
                if(DijkstraSemiringTraits::less(new_dist, heap.prio(w))) {
                    heap.decrease(w, new_dist);
                    if constexpr(track_predecessor_nodes)
                        pred_nodes_map[w] = p.first;
                    if constexpr(track_predecessor_arcs) pred_arcs_map[w] = a;
                }
                continue;
            }
            if(s == Heap::PRE_HEAP) {
                heap.push(
                    w, DijkstraSemiringTraits::plus(p.second, length_map[a]));
                if constexpr(track_predecessor_nodes)
                    pred_nodes_map[w] = p.first;
                if constexpr(track_predecessor_arcs) pred_arcs_map[w] = a;
            }
        }
        if constexpr(track_distances) dist_map[p.first] = p.second;
        return p;
    }

    void run() noexcept {
        while(!emptyQueue()) {
            processNextNode();
        }
    }

    Node pred_node(const Node u) const noexcept
        requires(track_predecessor_nodes) {
        assert(heap.state(u) != Heap::PRE_HEAP);
        return pred_nodes_map[u];
    }
    Arc pred_arc(const Node u) const noexcept requires(track_predecessor_arcs) {
        assert(heap.state(u) != Heap::PRE_HEAP);
        return pred_arcs_map[u];
    }
    Value dist(const Node u) const noexcept requires(track_distances) {
        assert(heap.state(u) == Heap::POST_HEAP);
        return dist_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DIJKSTRA_HPP
