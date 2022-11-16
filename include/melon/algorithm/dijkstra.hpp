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
#include "melon/data_structures/d_ary_heap.hpp"
#include "melon/data_structures/fast_binary_heap.hpp"
#include "melon/utils/prefetch.hpp"
#include "melon/utils/traversal_algorithm_behavior.hpp"
#include "melon/utils/traversal_algorithm_iterator.hpp"

namespace fhamonic {
namespace melon {

template <concepts::adjacency_list_graph GR, typename LM>
struct DijkstraDefaultTraits {
    using Semiring = DijkstraShortestPathSemiring<typename LM::value_type>;
    using Heap = FastBinaryHeap<typename GR::vertex_t, typename LM::value_type,
                                decltype(Semiring::less)>;

    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
};

template <concepts::adjacency_list_graph GR, typename LM,
          typename TR = DijkstraDefaultTraits<GR, LM>>
class Dijkstra {
public:
    using vertex_t = GR::vertex_t;
    using arc_t = GR::arc_t;
    using value_t = LM::value_type;

private:
    using Heap = TR::Heap;
    using PredVerticesMap = std::conditional<TR::store_pred_vertices,
                                             typename GR::vertex_map<vertex_t>,
                                             std::monostate>::type;
    using PredArcsMap =
        std::conditional<TR::store_pred_arcs, typename GR::vertex_map<arc_t>,
                         std::monostate>::type;
    using DistancesMap =
        std::conditional<TR::store_distances, typename GR::vertex_map<value_t>,
                         std::monostate>::type;

private:
    const GR & _graph;
    const LM & _length_map;

    Heap _heap;
    PredVerticesMap _pred_vertices_map;
    PredArcsMap _pred_arcs_map;
    DistancesMap _dist_map;

public:
    Dijkstra(const GR & g, const LM & l)
        : _graph(g), _length_map(l), _heap(g.nb_vertices()) {
        if constexpr(TR::store_pred_vertices)
            _pred_vertices_map.resize(g.nb_vertices());
        if constexpr(TR::store_pred_arcs)
            _pred_arcs_map.resize(g.nb_vertices());
        if constexpr(TR::store_distances) _dist_map.resize(g.nb_vertices());
    }

    Dijkstra & reset() noexcept {
        _heap.clear();
        return *this;
    }
    Dijkstra & add_source(vertex_t s,
                          value_t dist = TR::Semiring::zero) noexcept {
        assert(_heap.state(s) != Heap::IN_HEAP);
        _heap.push(s, dist);
        if constexpr(TR::store_pred_vertices) _pred_vertices_map[s] = s;
        return *this;
    }

    bool empty_queue() const noexcept { return _heap.empty(); }

    std::pair<vertex_t, value_t> next_node() noexcept {
        const auto p = _heap.top();
        prefetch_range(_graph.out_arcs(p.first));
        prefetch_range(_graph.out_neighbors(p.first));
        prefetch_map_values(_graph.out_arcs(p.first), _length_map);
        _heap.pop();
        for(const arc_t a : _graph.out_arcs(p.first)) {
            const vertex_t w = _graph.target(a);
            const auto s = _heap.state(w);
            if(s == Heap::IN_HEAP) {
                const value_t new_dist =
                    TR::Semiring::plus(p.second, _length_map[a]);
                if(TR::Semiring::less(new_dist, _heap.prio(w))) {
                    _heap.decrease(w, new_dist);
                    if constexpr(TR::store_pred_vertices)
                        _pred_vertices_map[w] = p.first;
                    if constexpr(TR::store_pred_arcs) _pred_arcs_map[w] = a;
                }
            } else if(s == Heap::PRE_HEAP) {
                _heap.push(w, TR::Semiring::plus(p.second, _length_map[a]));
                if constexpr(TR::store_pred_vertices)
                    _pred_vertices_map[w] = p.first;
                if constexpr(TR::store_pred_arcs) _pred_arcs_map[w] = a;
            }
        }
        if constexpr(TR::store_distances) _dist_map[p.first] = p.second;
        return p;
    }

    void run() noexcept {
        while(!empty_queue()) next_node();
    }
    auto begin() noexcept { return traversal_algorithm_iterator(*this); }
    auto end() noexcept { return traversal_algorithm_end_iterator(); }

    vertex_t pred_vertex(const vertex_t u) const noexcept
        requires(TR::store_pred_vertices) {
        assert(_heap.state(u) != Heap::PRE_HEAP);
        return _pred_vertices_map[u];
    }
    arc_t pred_arc(const vertex_t u) const noexcept
        requires(TR::store_pred_arcs) {
        assert(_heap.state(u) != Heap::PRE_HEAP);
        return _pred_arcs_map[u];
    }
    value_t dist(const vertex_t u) const noexcept
        requires(TR::store_distances) {
        assert(_heap.state(u) == Heap::POST_HEAP);
        return _dist_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_DIJKSTRA_HPP
