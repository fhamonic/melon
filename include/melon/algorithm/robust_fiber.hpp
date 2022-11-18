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
#include "melon/utils/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

template <concepts::incidence_list_graph G, typename L>
struct strong_fiber_default_traits {
    using semiring = shortest_path_semiring<typename L::value_type>;
    using heap = fast_binary_heap<typename G::vertex_t, typename L::value_type,
                                  decltype(semiring::less)>;

    static constexpr bool strictly_strong = false;
};

template <concepts::adjacency_list_graph GR, typename LM1, typename LM2,
          typename F1, typename F2, bool strictly_strong = false,
          typename SR = shortest_path_semiring<typename LM1::value_type>>
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

    using Heap = fast_binary_heap<typename GR::vertex_t, Entry, entry_cmp>;

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

    RobustFiber & discard(vertex_t u) noexcept {
        assert(_heap.state(u) != Heap::IN_HEAP);
        _heap.discard(u);
        return *this;
    }

    RobustFiber & add_strong_arc_source(
        arc_t uv, Value u_dist = DijkstraSemiringTraits::zero) noexcept {
        vertex_t u = _graph.source(uv);
        discard(u);
        for(const arc_t a : _graph.out_arcs(u)) {
            if(a == uv) continue;
            process_weak_vertex_out_arc(u, u_dist, a);
        }
        process_strong_vertex_out_arc(u, u_dist, uv);
        _callback_weak(u);
        return *this;
    }

    RobustFiber & add_useless_arc_source(
        arc_t uv, Value u_dist = DijkstraSemiringTraits::zero) noexcept {
        vertex_t u = _graph.source(uv);
        discard(u);
        for(const arc_t a : _graph.out_arcs(u)) {
            if(a == uv) continue;
            process_strong_vertex_out_arc(u, u_dist, a);
        }
        process_weak_vertex_out_arc(u, u_dist, uv);
        _callback_weak(u);
        return *this;
    }

    RobustFiber & add_strong_source(vertex_t u, Value dist) noexcept {
        discard(u);
        for(const arc_t a : _graph.out_arcs(u)) {
            process_strong_vertex_out_arc(u, Entry(dist, true), a);
        }
        return *this;
    }

    void process_strong_vertex_out_arc(const vertex_t u, const Value dist,
                                       const arc_t uw) noexcept {
        const vertex_t w = _graph.target(uw);
        const auto s = _heap.state(w);
        if(s == Heap::IN_HEAP) {
            const Entry new_entry(dist + _length_map[uw], true);
            const Entry old_entry = _heap.priority(w);
            if(cmp(new_entry, old_entry)) {
                if(!old_entry.strong) {
                    --nb_weak_candidates;
                    ++nb_strong_candidates;
                }
                _heap.decrease(w, new_entry);
            }
        } else if(s == Heap::PRE_HEAP) {
            _heap.push(w, Entry(dist + _length_map[uw], true));
            ++nb_strong_candidates;
        }
    }

    void process_weak_vertex_out_arc(const vertex_t u, const Value dist,
                                     const arc_t uw) noexcept {
        const vertex_t w = _graph.target(uw);
        const auto s = _heap.state(w);
        if(s == Heap::IN_HEAP) {
            const Entry new_entry(dist + _reduced_length_map[uw], false);
            const Entry old_entry = _heap.priority(w);
            if(cmp(new_entry, old_entry)) {
                if(old_entry.strong) {
                    --nb_strong_candidates;
                    ++nb_weak_candidates;
                }
                _heap.decrease(w, new_entry);
            }
        } else if(s == Heap::PRE_HEAP) {
            _heap.push(w, Entry(dist + _reduced_length_map[uw], false));
            ++nb_weak_candidates;
        }
    }

    RobustFiber & run() noexcept {
        while(nb_strong_candidates > 0 && nb_weak_candidates > 0) {
            const auto && [u, entry] = _heap.top();
            prefetch_range(_graph.out_arcs(u));
            prefetch_range(_graph.out_neighbors(u));
            if(entry.strong) {
                prefetch_map_values(_graph.out_arcs(u), _length_map);
                _callback_strong(u);
                _heap.pop();
                --nb_strong_candidates;
                for(const arc_t a : _graph.out_arcs(u)) {
                    process_strong_vertex_out_arc(u, entry.dist, a);
                }
            } else {
                prefetch_map_values(_graph.out_arcs(u), _reduced_length_map);
                _callback_weak(u);
                _heap.pop();
                --nb_weak_candidates;
                for(const arc_t a : _graph.out_arcs(u)) {
                    process_weak_vertex_out_arc(u, entry.dist, a);
                }
            }
        }
        if(nb_strong_candidates > 0) {
            for(auto && v : _graph.vertices()) {
                if(_heap.state(v) == Heap::POST_HEAP) continue;
                _callback_strong(v);
            }
            return *this;
        }
        if(nb_weak_candidates > 0) {
            for(auto && v : _graph.vertices()) {
                if(_heap.state(v) == Heap::POST_HEAP) continue;
                _callback_weak(v);
            }
        }
        return *this;
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_ROBUST_FIBER_HPP
