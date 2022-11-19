#ifndef MELON_STRONG_FIBER_HPP
#define MELON_STRONG_FIBER_HPP

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
#include "melon/utils/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

template <concepts::incidence_list_graph G, typename L>
struct strong_fiber_default_traits {
    using value_t = typename L::value_type;
    using semiring = shortest_path_semiring<value_t>;

    struct entry {
        value_t dist;
        bool strong;

        entry operator+(value_t v) const noexcept {
            return entry(semiring::plus(dist, v), strong);
        }
        bool operator==(const entry & o) const noexcept {
            return dist == o.dist && strong == o.strong;
        }
    };

    static constexpr bool strictly_strong = false;

    struct entry_cmp {
        constexpr bool operator()(const entry & e1,
                                  const entry & e2) const noexcept {
            if constexpr(strictly_strong) {
                if(e1.dist == e2.dist) return !e1.strong && e2.strong;
            } else {
                if(e1.dist == e2.dist) return e1.strong && !e2.strong;
            }
            return semiring::less(e1.dist, e2.dist);
        }
    };

    using heap = fast_binary_heap<typename G::vertex_t, entry, entry_cmp>;
};

template <concepts::adjacency_list_graph G, typename L1, typename L2,
          typename F1, typename F2,
          typename T = strong_fiber_default_traits<G, L1>>
requires std::is_same_v<typename L1::value_type, typename L2::value_type>
class strong_fiber {
public:
    using vertex_t = G::vertex_t;
    using arc_t = G::arc_t;
    using value_t = L1::value_type;
    // using traversal_entry = std::pair<vertex_t, bool>;
    using traits = T;

private:
    using entry = traits::entry;
    using entry_cmp = traits::entry_cmp;
    using heap = traits::heap;

    const G & _graph;
    const L1 & _reduced_length_map;
    const L2 & _length_map;

    F1 _callback_strong;
    F2 _callback_weak;

    heap _heap;
    entry_cmp cmp;
    std::size_t nb_strong_candidates;
    std::size_t nb_weak_candidates;

public:
    strong_fiber(const G & g, const L1 & l1, const L2 & l2, F1 && f1, F2 && f2)
        : _graph(g)
        , _reduced_length_map(l1)
        , _length_map(l2)
        , _callback_strong(std::forward<F1>(f1))
        , _callback_weak(std::forward<F2>(f2))
        , _heap(g.nb_vertices())
        , nb_strong_candidates(0)
        , nb_weak_candidates(0) {}

    strong_fiber & reset() noexcept {
        _heap.clear();
        nb_strong_candidates = 0;
        nb_weak_candidates = 0;
        return *this;
    }

    strong_fiber & discard(vertex_t u) noexcept {
        assert(_heap.state(u) != heap::IN_HEAP);
        _heap.discard(u);
        return *this;
    }

    strong_fiber & add_strong_arc_source(
        arc_t uv, value_t u_dist = traits::semiring::zero) noexcept {
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

    strong_fiber & add_useless_arc_source(
        arc_t uv, value_t u_dist = traits::semiring::zero) noexcept {
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

    strong_fiber & add_strong_source(vertex_t u, value_t dist) noexcept {
        discard(u);
        for(const arc_t a : _graph.out_arcs(u)) {
            process_strong_vertex_out_arc(u, entry(dist, true), a);
        }
        return *this;
    }

    void process_strong_vertex_out_arc(const vertex_t u, const value_t dist,
                                       const arc_t uw) noexcept {
        const vertex_t w = _graph.target(uw);
        const auto s = _heap.state(w);
        if(s == heap::IN_HEAP) {
            const entry new_entry(dist + _length_map[uw], true);
            const entry old_entry = _heap.priority(w);
            if(cmp(new_entry, old_entry)) {
                if(!old_entry.strong) {
                    --nb_weak_candidates;
                    ++nb_strong_candidates;
                }
                _heap.decrease(w, new_entry);
            }
        } else if(s == heap::PRE_HEAP) {
            _heap.push(w, entry(dist + _length_map[uw], true));
            ++nb_strong_candidates;
        }
    }

    void process_weak_vertex_out_arc(const vertex_t u, const value_t dist,
                                     const arc_t uw) noexcept {
        const vertex_t w = _graph.target(uw);
        const auto s = _heap.state(w);
        if(s == heap::IN_HEAP) {
            const entry new_entry(dist + _reduced_length_map[uw], false);
            const entry old_entry = _heap.priority(w);
            if(cmp(new_entry, old_entry)) {
                if(old_entry.strong) {
                    --nb_strong_candidates;
                    ++nb_weak_candidates;
                }
                _heap.decrease(w, new_entry);
            }
        } else if(s == heap::PRE_HEAP) {
            _heap.push(w, entry(dist + _reduced_length_map[uw], false));
            ++nb_weak_candidates;
        }
    }

    strong_fiber & run() noexcept {
        while(nb_strong_candidates > 0 && nb_weak_candidates > 0) {
            const auto && [u, e] = _heap.top();
            prefetch_range(_graph.out_arcs(u));
            prefetch_range(_graph.out_neighbors(u));
            if(e.strong) {
                prefetch_map_values(_graph.out_arcs(u), _length_map);
                _callback_strong(u);
                _heap.pop();
                --nb_strong_candidates;
                for(const arc_t a : _graph.out_arcs(u)) {
                    process_strong_vertex_out_arc(u, e.dist, a);
                }
            } else {
                prefetch_map_values(_graph.out_arcs(u), _reduced_length_map);
                _callback_weak(u);
                _heap.pop();
                --nb_weak_candidates;
                for(const arc_t a : _graph.out_arcs(u)) {
                    process_weak_vertex_out_arc(u, e.dist, a);
                }
            }
        }
        if(nb_strong_candidates > 0) {
            for(auto && v : _graph.vertices()) {
                if(_heap.state(v) == heap::POST_HEAP) continue;
                _callback_strong(v);
            }
            return *this;
        }
        if(nb_weak_candidates > 0) {
            for(auto && v : _graph.vertices()) {
                if(_heap.state(v) == heap::POST_HEAP) continue;
                _callback_weak(v);
            }
        }
        return *this;
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STRONG_FIBER_HPP
