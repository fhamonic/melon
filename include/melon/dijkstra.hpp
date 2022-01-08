#ifndef MELON_DIJKSTRA_HPP
#define MELON_DIJKSTRA_HPP

#include <algorithm>
#include <queue>
#include <ranges>
#include <vector>

#include "melon/binary_heap.hpp"
#include "melon/dijkstra_semirings.hpp"
#include "melon/static_digraph.hpp"

namespace fhamonic {
namespace melon {

template <typename GR, typename LM,
          typename SR = DijkstraShortestPathSemiring<typename LM::value_type>,
          typename HP = BinaryHeap<typename GR::Node, typename LM::value_type,
                                   decltype(SR::less)>>
class Dijkstra {
public:
    using Node = GR::Node;
    using Arc = GR::Arc;

    using Value = LM::value_type;
    using DijkstraSemiringTraits = SR;
    using Heap = HP;

private:
    const GR & graph;
    const LM & length_map;

    Heap heap;
    typename GR::ArcMap<Node> pred_map;

public:
    Dijkstra(const GR & g, const LM & l)
        : graph(g), length_map(l), heap(g.nb_nodes()), pred_map(g.nb_nodes()) {}

    void addSource(Node s, Value dist = DijkstraSemiringTraits::zero) noexcept {
        assert(!heap.contains(s));
        heap.push(s, dist);
        pred_map[s] = s;
    }
    bool emptyQueue() const noexcept { return heap.empty(); }
    void reset() noexcept { heap.clear(); }

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
                    pred_map[w] = p.first;
                }
                continue;
            }
            if(s == Heap::PRE_HEAP) {
                heap.push(w, DijkstraSemiringTraits::plus(p.second, length_map[a]));
                pred_map[w] = p.first;
            }
        }
        return p;
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DIJKSTRA_HPP
