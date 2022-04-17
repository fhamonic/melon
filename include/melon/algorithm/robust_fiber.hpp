#ifndef MELON_ALGORITHM_ROBUST_FIBER_HPP
#define MELON_ALGORITHM_ROBUST_FIBER_HPP

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/algorithm/dijkstra_semirings.hpp"
#include "melon/concepts/graph_concepts.hpp"
#include "melon/data_structures/d_ary_heap.hpp"
#include "melon/data_structures/fast_binary_heap.hpp"
#include "melon/utils/traversal_algorithm_behavior.hpp"
#include "melon/utils/traversal_algorithm_iterator.hpp"

namespace fhamonic {
namespace melon {

template <concepts::adjacency_list_graph GR, typename LM1, typename LM2,
          typename SR = DijkstraShortestPathSemiring<typename LM1::value_type>>
class RobustFiber {
public:
    using vertex_t = GR::vertex_t;
    using arc_t = GR::arc_t;

    using Value = LM::value_type;
    using DijkstraSemiringTraits = SR;

    struct Entry {
        Value dist;
        bool strong;

        Entry operator+(Value v) {
            return Entry(DijkstraSemiringTraits::plus(dist, v), strong);
        }
        bool operator<(const Entry & o) {
            if(dist == o.dist) return !strong & o.strong;
            return DijkstraSemiringTraits::less(dist, o.dist);
        }
        bool operator==(const Entry & o) {
            return dist == o.dist && strong == o.strong;
        }
    };

    using Heap = FastBinaryHeap<typename GR::vertex_t, Entry, decltype(SR::less)>;

private:
    const GR & _graph;
    const LM1 & _length_map;
    const LM2 & _reduced_length_map;

    Heap _heap;

public:
    Dijkstra(const GR & g, const LM1 & l1, const LM2 & l2)
        : _graph(g)
        , _length_map(l1)
        , _reduced_length_map(l2)
        , _heap(g.nb_vertices()) {}

    Dijkstra & reset() noexcept {
        _heap.clear();
        return *this;
    }
    Dijkstra & add_source(vertex_t s, Entry e) noexcept {
        assert(_heap.state(s) != Heap::IN_HEAP);
        _heap.push(s, e);
        return *this;
    }

    bool empty_queue() const noexcept { return _heap.empty(); }

    std::pair<vertex_t, Entry> next_node() noexcept {
        const auto p = _heap.top();
        if constexpr(std::ranges::contiguous_range<decltype(
                         _graph.out_neighbors(p.first))>) {
            if(_graph.out_arcs(p.first).size()) {
                __builtin_prefetch(_graph.out_neighbors(p.first).data());
            }
        }
        _heap.pop();
        for(const arc_t a : _graph.out_arcs(p.first)) {
            const vertex_t w = _graph.target(a);
            const auto s = _heap.state(w);
            if(s == Heap::IN_HEAP) {
                const Entry new_entry = p.second + p.second.strong ? _length_map[a] : _reduced_length_map[a]);
                if(new_entry < _heap.prio(w)) {
                    _heap.decrease(w, new_entry);
                }
            } else if(s == Heap::PRE_HEAP) {
                _heap.push(w, p.second + _length_map[a]);
            }
        }
        return p;
    }

    void run() noexcept {
        while(!empty_queue()) next_node();
    }
    auto begin() noexcept { return traversal_algorithm_iterator(*this); }
    auto end() noexcept { return traversal_algorithm_end_iterator(); }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_ROBUST_FIBER_HPP
