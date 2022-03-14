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
          typename SR = DijkstraShortestPathSemiring<typename LM1::value_type>,
          typename HP =
              FastBinaryHeap<typename GR::vertex, typename LM1::value_type,
                             decltype(SR::less)>>
class RobustFiber {
public:
    using vertex = GR::vertex;
    using arc = GR::arc;

    using Value = LM::value_type;
    using DijkstraSemiringTraits = SR;
    using Heap = HP;

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
    Dijkstra & add_source(vertex s,
                          Value dist = DijkstraSemiringTraits::zero) noexcept {
        assert(_heap.state(s) != Heap::IN_HEAP);
        _heap.push(s, dist);
        return *this;
    }

    bool empty_queue() const noexcept { return _heap.empty(); }

    std::pair<vertex, Value> next_node() noexcept {
        const auto p = _heap.top();
        if constexpr(std::ranges::contiguous_range<
                         decltype(_graph.out_neighbors(p.first))>) {
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
                }
            } else if(s == Heap::PRE_HEAP) {
                _heap.push(
                    w, DijkstraSemiringTraits::plus(p.second, _length_map[a]));
            }
        }
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
    arc pred_arc(const vertex u) const noexcept
        requires(track_predecessor_arcs) {
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

#endif  // MELON_ALGORITHM_ROBUST_FIBER_HPP
