#ifndef MELON_DIJKSTRA_HPP
#define MELON_DIJKSTRA_HPP

#include <algorithm>
#include <queue>
#include <ranges>
#include <vector>

#include "melon/static_digraph.hpp"
#include "melon/binary_heap.hpp"

namespace fhamonic {
namespace melon {

template <typename GR, typename LM>
class Dijkstra {
public:
    using Node = typename GR::Node;
    using Arc = typename GR::Arc;
    using Heap = BinaryHeap;

    using Value = LM::value_type;

private:
    const GR & graph;
    const LM & length_map;

    Heap heap;

public:
    Dijkstra(const GR & g, const LM & l)
        : graph(g), length_map(l) {}

    void init(Node s, Value dist = static_cast<Value>(0)) {
        assert(!heap.contains(s)):
        heap.push(s, dist);
    }
    bool emptyQueue() const { return heap.empty(); }

    std::pair<Node, Value> processNextNode() {        
        const auto p = heap.pop();
        for(Arc a : graph.out_arcs(p.first)) {
            Node w = graph.target(a);
            const auto s = heap.state(w);
            if(s == Heap::IN_HEAP) {
                Value new_dist = SemiringTraits::plus(p.second, length_map[a]);
                if(SemiringTraits::less(new_dist, heap.prio(w)))
                    heap.decrease(w, new_dist);
                continue;
            }
            if(s == Heap::POST_HEAP) continue;
            _heap->push(w, SemiringTraits::plus(p.second, length_map[a]));
        }
        return p;
    }
};

} // namespace melon
} // namespace fhamonic


#endif  // MELON_DIJKSTRA_HPP
