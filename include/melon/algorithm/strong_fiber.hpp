#ifndef MELON_STRONG_FIBER_HPP
#define MELON_STRONG_FIBER_HPP

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/concepts/graph.hpp"
#include "melon/concepts/map_of.hpp"
#include "melon/concepts/priority_queue.hpp"
#include "melon/concepts/semiring.hpp"
#include "melon/data_structures/d_ary_heap.hpp"
#include "melon/utils/prefetch.hpp"
#include "melon/utils/semirings.hpp"
#include "melon/utils/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
namespace concepts {
template <typename T>
concept strong_fiber_trait = semiring<typename T::semiring> &&
    updatable_priority_queue<typename T::heap> && requires() {
    { T::store_distances } -> std::convertible_to<bool>;
    { T::store_paths } -> std::convertible_to<bool>;
};
}  // namespace concepts
// clang-format on

template <concepts::outward_incidence_graph G, typename T>
struct strong_fiber_default_traits {
    using semiring = shortest_path_semiring<T>;

    struct entry_cmp {
        std::reference_wrapper<const vertex_map_t<G, bool>> _strong_map;

        [[nodiscard]] constexpr explicit entry_cmp(
            const vertex_map_t<G, bool> & strong_map)
            : _strong_map(strong_map) {}

        constexpr bool operator()(const auto & e1,
                                  const auto & e2) const noexcept {
            if(e1.second == e2.second) {
                if constexpr(strictly_strong)
                    return !_strong_map.get()[e1.first] &&
                           _strong_map.get()[e2.first];
                else
                    return _strong_map.get()[e1.first] &&
                           !_strong_map.get()[e2.first];
            }
            return semiring::less(e1.second, e2.second);
        }
    };

    using heap =
        d_ary_heap<2, vertex_t<G>, T, entry_cmp, vertex_map_t<G, std::size_t>>;

    static constexpr bool strictly_strong = false;
    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};

template <concepts::outward_incidence_graph G, concepts::input_map<arc_t<G>> L1,
          concepts::input_map<arc_t<G>> L2,
          concepts::strong_fiber_trait T =
              strong_fiber_default_traits<G, mapped_value_t<L1, arc_t<G>>>>
requires std::is_same_v<mapped_value_t<L1, arc_t<G>>,
                        mapped_value_t<L2, arc_t<G>>>
class strong_fiber {
public:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;
    using value_t = L1::value_type;
    using traits = T;

private:
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    using vertex_status_map = vertex_map_t<G, vertex_status>;
    using vertex_strong_map = vertex_map_t<G, bool>;
    using heap = traits::heap;

    std::reference_wrapper<const G> _graph;
    std::reference_wrapper<const L1> _reduced_length_map;
    std::reference_wrapper<const L2> _length_map;

    vertex_status_map _vertex_status_map;
    vertex_strong_map _vertex_strong_map;
    heap _heap;
    std::size_t _nb_strong_candidates;

public:
    strong_fiber(const G & g, const L1 & l1, const L2 & l2)
        : _graph(g)
        , _reduced_length_map(l1)
        , _length_map(l2)
        , _vertex_status_map(create_vertex_map<vertex_status>(g, PRE_HEAP))
        , _vertex_strong_map(create_vertex_map<bool>(g))
        , _heap(create_vertex_map<std::size_t>(g),
                typename traits::entry_cmp(_vertex_strong_map))
        , _nb_strong_candidates(0) {}

    strong_fiber & reset() noexcept {
        _vertex_status_map.fill(PRE_HEAP);
        _vertex_strong_map.fill(false);
        _heap.clear();
        _nb_strong_candidates = 0;
        return *this;
    }

    strong_fiber & discard(const vertex & u) noexcept {
        assert(_vertex_status_map[u] != IN_HEAP);
        _vertex_status_map[u] = POST_HEAP;
        return *this;
    }

    strong_fiber & add_strong_arc_source(
        const arc & uv,
        const value_t & u_dist = traits::semiring::zero) noexcept {
        vertex u = source(_graph.get(), uv);
        discard(u);
        for(const arc a : out_arcs(_graph.get(), u)) {
            if(a == uv) continue;
            process_weak_vertex_out_arc(u, u_dist, a);
        }
        process_strong_vertex_out_arc(u, u_dist, uv);
        return *this;
    }

    strong_fiber & add_useless_arc_source(
        const arc & uv,
        const value_t & u_dist = traits::semiring::zero) noexcept {
        vertex u = source(_graph.get(), uv);
        discard(u);
        for(const arc a : out_arcs(_graph.get(), u)) {
            if(a == uv) continue;
            process_strong_vertex_out_arc(u, u_dist, a);
        }
        process_weak_vertex_out_arc(u, u_dist, uv);
        return *this;
    }

    strong_fiber & add_strong_source(const vertex & u,
                                     const value_t & dist) noexcept {
        discard(u);
        for(const arc & a : out_arcs(_graph.get(), u)) {
            process_strong_vertex_out_arc(u, entry(dist, true), a);
        }
        return *this;
    }

    void process_strong_vertex_out_arc(const vertex & u, const value_t & dist,
                                       const arc & uw) noexcept {
        const vertex & w = target(_graph.get(), uw);
        auto && w_status = _vertex_status_map[w];
        if(w_status == IN_HEAP) {
            const value_t new_dist =
                traits::semiring::plus(dist, _length_map.get()[uw]);
            const value_t old_dist = _heap.priority(w);
            if(traits::semiring::less(new_dist, old_dist)) {
                if(!_vertex_strong_map[w]) {
                    _vertex_strong_map[w] = true;
                    ++_nb_strong_candidates;
                }
                _heap.promote(w, new_dist);
            }
        } else if(w_status == PRE_HEAP) {
            _heap.push(w, traits::semiring::plus(dist, _length_map.get()[uw]));
            _vertex_status_map[w] = IN_HEAP;
            _vertex_strong_map[w] = true;
            ++_nb_strong_candidates;
        }
    }

    void process_weak_vertex_out_arc(const vertex & u, const value_t & dist,
                                     const arc & uw) noexcept {
        const vertex & w = target(_graph.get(), uw);
        auto && w_status = _vertex_status_map[w];
        if(w_status == IN_HEAP) {
            const value_t new_dist =
                traits::semiring::plus(dist, _reduced_length_map.get()[uw]);
            const value_t old_dist = _heap.priority(w);
            if(traits::semiring::less(new_dist, old_dist)) {
                if(_vertex_strong_map[w]) {
                    _vertex_strong_map[w] = false;
                    --_nb_strong_candidates;
                }
                _heap.promote(w, new_dist);
            }
        } else if(w_status == PRE_HEAP) {
            _heap.push(
                w, traits::semiring::plus(dist, _reduced_length_map.get()[uw]));
            _vertex_status_map[w] = IN_HEAP;
            _vertex_strong_map[w] = false;
        }
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _nb_strong_candidates == 0;
    }

    [[nodiscard]] constexpr auto current() const noexcept {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() noexcept {
        do {
            const auto && [t, t_dist] = _heap.top();
            _vertex_status_map[t] = POST_HEAP;
            const auto & out_arcs_range = out_arcs(_graph.get(), t);
            prefetch_range(out_arcs_range);
            prefetch_mapped_values(out_arcs_range, targets_map(_graph.get()));
            if(_vertex_strong_map[t]) {
                prefetch_mapped_values(out_arcs_range, _length_map.get());
                _heap.pop();
                --_nb_strong_candidates;
                for(const arc & a : out_arcs_range) {
                    process_strong_vertex_out_arc(t, t_dist, a);
                }
            } else {
                prefetch_mapped_values(out_arcs_range,
                                       _reduced_length_map.get());
                _heap.pop();
                for(const arc a : out_arcs_range) {
                    process_weak_vertex_out_arc(t, t_dist, a);
                }
            }
        } while(_nb_strong_candidates > 0 &&
                !_vertex_strong_map[_heap.top().first]);
    }

    constexpr void init() noexcept {
        if(!_vertex_strong_map[_heap.top().first]) advance();
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }
    [[nodiscard]] constexpr auto begin() noexcept {
        init();
        return traversal_iterator(*this);
    }
    [[nodiscard]] constexpr auto end() noexcept {
        return traversal_end_sentinel();
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STRONG_FIBER_HPP
