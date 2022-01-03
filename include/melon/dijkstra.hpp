#ifndef MELON_DIJKSTRA_HPP
#define MELON_DIJKSTRA_HPP

#include <algorithm>
#include <queue>
#include <ranges>
#include <vector>

#include "melon/binary_heap.hpp"
#include "melon/static_digraph.hpp"
#include "melon/dijkstra_semirings.hpp"

namespace fhamonic {
namespace melon {

template <typename GR, typename LM,
          typename TR = DijkstraShortestPathSemiring<typename LM::value_type>>
class Dijkstra {
public:
    using Node = typename GR::Node;
    using Arc = typename GR::Arc;

    using Value = LM::value_type;
    using DijkstraSemiringTraits = TR;

    using Heap = BinaryHeap<Node, Value, decltype(DijkstraSemiringTraits::less)>;

private:
    const GR & graph;
    const LM & length_map;

    Heap heap;

public:
    Dijkstra(const GR & g, const LM & l) : graph(g), length_map(l), heap(g.nb_nodes()) {}

    void init(Node s, Value dist = DijkstraSemiringTraits::zero) {
        assert(!heap.contains(s));
        heap.push(s, dist);
    }
    bool emptyQueue() const { return heap.empty(); }

    std::pair<Node, Value> processNextNode() {
        const auto p = heap.pop();
        for(Arc a : graph.out_arcs(p.first)) {
            Node w = graph.target(a);
            const auto s = heap.state(w);
            if(s == Heap::IN_HEAP) {
                Value new_dist =
                    DijkstraSemiringTraits::plus(p.second, length_map[a]);
                if(DijkstraSemiringTraits::less(new_dist, heap.prio(w)))
                    heap.decrease(w, new_dist);
                continue;
            }
            if(s == Heap::POST_HEAP) continue;
            heap.push(w, DijkstraSemiringTraits::plus(p.second, length_map[a]));
        }
        return p;
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DIJKSTRA_HPP
