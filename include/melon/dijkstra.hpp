#ifndef STATIC_GRAPH_HPP
#define STATIC_GRAPH_HPP

#include <algorithm>
#include <queue>
#include <ranges>
#include <vector>

#include "melon/static_digraph.hpp"

namespace fhamonic {
namespace melon {

template <typename GR, typename LM>
class Dijkstra {
public:
    using Node = typename GR::Node;
    using Arc = typename GR::Arc;
    using Heap = std::priority_queue;

private:
    const GR & graph;
    const LM & length_map;

    Heap heap;

public:
    Dijkstra(const GR & g, const LM & l)
        : graph(g), length_map(l) {}

    // void init(Node s) {
    //     _heap->clear();
    //     for(NodeIt u(*G); u != INVALID; ++u)
    //         _heap_cross_ref->set(u, Heap::PRE_HEAP);
    //     if(_heap->state(s) != Heap::IN_HEAP)
    //         _heap->push(s, OperationTraits::zero());
    // }

    // bool emptyQueue() const { return _heap->empty(); }

    // std::pair<typename GR::Node, double> processNextNode() {        
    //     const auto p = _heap->p_top();
    //     _heap->pop();
    //     for(OutArcIt e(*G, p.first); e != INVALID; ++e) {
    //         Node w = G->target(e);
    //         const auto s = _heap->state(w);
    //         if(s == Heap::IN_HEAP) {
    //             Value newvalue = OperationTraits::plus(p.second, (*_length)[e]);
    //             if(OperationTraits::less(newvalue, (*_heap)[w]))
    //                 _heap->decrease(w, newvalue);
    //             continue;
    //         }
    //         if(s == Heap::POST_HEAP) continue;
    //         _heap->push(w, OperationTraits::plus(p.second, (*_length)[e]));
    //     }
    //     return p;
    // }
};

} // namespace melon
} // namespace fhamonic


#endif  // STATIC_GRAPH_HPP
