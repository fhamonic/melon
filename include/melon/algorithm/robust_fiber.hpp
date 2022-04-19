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
#include "melon/utils/prefetch.hpp"
#include "melon/utils/traversal_algorithm_behavior.hpp"
#include "melon/utils/traversal_algorithm_iterator.hpp"

namespace fhamonic {
namespace melon {

template <concepts::adjacency_list_graph GR, typename LM1, typename LM2,
          typename F1, typename F2, bool strictly_strong = false,
          typename SR = DijkstraShortestPathSemiring<typename LM1::value_type>>
class RobustFiber {
public:
    using vertex_t = GR::vertex_t;
    using arc_t = GR::arc_t;

    using Value = LM1::value_type;
    using DijkstraSemiringTraits = SR;

    struct Entry {
        Value dist;
        bool strong;

        Entry operator+(Value v) const noexcept {
            return Entry(DijkstraSemiringTraits::plus(dist, v), strong);
        }
        bool operator==(const Entry & o) const noexcept {
            return dist == o.dist && strong == o.strong;
        }
    };

    struct entry_cmp {
        constexpr bool operator()(const Entry & e1,
                                  const Entry & e2) const noexcept {
            if constexpr(strictly_strong) {
                if(e1.dist == e2.dist) return !e1.strong && e2.strong;
            } else {
                if(e1.dist == e2.dist) return e1.strong & !e2.strong;
            }
            return DijkstraSemiringTraits::less(e1.dist, e2.dist);
        }
    };

    using Heap = FastBinaryHeap<typename GR::vertex_t, Entry, entry_cmp>;

private:
    const GR & _graph;
    const LM1 & _reduced_length_map;
    const LM2 & _length_map;

    F1 _callback_strong;
    F2 _callback_weak;

    Heap _heap;
    entry_cmp cmp;
    std::size_t nb_strong_candidates;
    std::size_t nb_weak_candidates;

public:
    RobustFiber(const GR & g, const LM1 & l1, const LM2 & l2, F1 && f1,
                F2 && f2)
        : _graph(g)
        , _reduced_length_map(l1)
        , _length_map(l2)
        , _callback_strong(std::forward<F1>(f1))
        , _callback_weak(std::forward<F2>(f2))
        , _heap(g.nb_vertices())
        , nb_strong_candidates(0)
        , nb_weak_candidates(0) {}

    RobustFiber & reset() noexcept {
        _heap.clear();
        nb_strong_candidates = 0;
        nb_weak_candidates = 0;
        return *this;
    }
    RobustFiber & add_fiber_arc(vertex_t s, arc_t st,
                             Value dist = DijkstraSemiringTraits::zero) noexcept {
        assert(_heap.state(s) != Heap::IN_HEAP);
        _heap.push(s, Entry(dist, strong));
            ++nb_strong_candidates;
        return *this;
    }



    RobustFiber & add_definitive_source(
        vertex_t s, Value dist = DijkstraSemiringTraits::zero,
        bool strong = false) noexcept {
        assert(_heap.state(s) != Heap::IN_HEAP);
        _heap.discard(s);
        if(strong) {
            process_strong_node(s, Entry(dist, strong));
        } else {
            process_weak_node(s, Entry(dist, strong));
        }
        return *this;
    }

    void process_strong_node(const vertex_t u, const Entry e) noexcept {
        for(const arc_t a : _graph.out_arcs(u)) {
            const vertex_t w = _graph.target(a);
            const auto s = _heap.state(w);
            if(s == Heap::IN_HEAP) {
                const Entry new_entry = e + _length_map[a];
                const Entry old_entry = _heap.prio(w);
                if(cmp(new_entry, old_entry)) {
                    if(!old_entry.strong) {
                        --nb_weak_candidates;
                        ++nb_strong_candidates;
                    }
                    _heap.decrease(w, new_entry);
                }
            } else if(s == Heap::PRE_HEAP) {
                _heap.push(w, e + _length_map[a]);
                ++nb_strong_candidates;
            }
        }
    }

    void process_weak_node(const vertex_t u, const Entry e) noexcept {
        for(const arc_t a : _graph.out_arcs(u)) {
            const vertex_t w = _graph.target(a);
            const auto s = _heap.state(w);
            if(s == Heap::IN_HEAP) {
                const Entry new_entry = e + _reduced_length_map[a];
                const Entry old_entry = _heap.prio(w);
                if(cmp(new_entry, old_entry)) {
                    if(old_entry.strong) {
                        --nb_strong_candidates;
                        ++nb_weak_candidates;
                    }
                    _heap.decrease(w, new_entry);
                }
            } else if(s == Heap::PRE_HEAP) {
                _heap.push(w, e + _reduced_length_map[a]);
                ++nb_weak_candidates;
            }
        }
    }

    void run() noexcept {
        while(nb_strong_candidates > 0 && nb_weak_candidates > 0) {
            const auto && [u, entry] = _heap.top();
            prefetch_range(_graph.out_arcs(u));
            prefetch_range(_graph.out_neighbors(u));
            if(entry.strong) {
                prefetch_map_values(_graph.out_arcs(u), _length_map);
                _callback_strong(u);
                _heap.pop();
                --nb_strong_candidates;
                process_strong_node(u, entry);
            } else {
                prefetch_map_values(_graph.out_arcs(u), _reduced_length_map);
                _callback_weak(u);
                _heap.pop();
                --nb_weak_candidates;
                process_weak_node(u, entry);
            }
        }
        if(nb_strong_candidates > 0) {
            for(auto && v : _graph.vertices()) {
                if(_heap.state(v) == Heap::POST_HEAP) continue;
                _callback_strong(v);
            }
            return;
        }
        if(nb_weak_candidates > 0) {
            for(auto && v : _graph.vertices()) {
                if(_heap.state(v) == Heap::POST_HEAP) continue;
                _callback_weak(v);
            }
        }
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_ROBUST_FIBER_HPP
