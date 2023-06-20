#ifndef MELON_STRONG_FIBER_HPP
#define MELON_STRONG_FIBER_HPP

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/constexpr_ternary.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"
#include "melon/utility/traversal_iterator.hpp"
#include "melon/utility/value_map.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
template <typename T>
concept strong_fiber_trait = semiring<typename T::semiring> &&
    updatable_priority_queue<typename T::heap<>> && requires() {
    { T::store_distances } -> std::convertible_to<bool>;
    { T::store_paths } -> std::convertible_to<bool>;
};
// clang-format on

template <outward_incidence_graph G, typename T>
struct strong_fiber_default_traits {
    using semiring = shortest_path_semiring<T>;
    template <typename CMP = std::less<std::pair<vertex_t<G>, T>>>
    using heap =
        d_ary_heap<2, vertex_t<G>, T, CMP, vertex_map_t<G, std::size_t>>;

    static constexpr bool strictly_strong = false;
    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};

template <outward_incidence_graph G, input_value_map<arc_t<G>> L1,
          input_value_map<arc_t<G>> L2,
          strong_fiber_trait T =
              strong_fiber_default_traits<G, mapped_value_t<L1, arc_t<G>>>>
    requires std::is_same_v<mapped_value_t<L1, arc_t<G>>,
                            mapped_value_t<L2, arc_t<G>>>
class strong_fiber {
public:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;
    using value_t = mapped_value_t<L1, arc_t<G>>;
    using traits = T;

private:
    struct entry_cmp {
        std::reference_wrapper<const vertex_map_t<G, bool>> _strong_map;

        [[nodiscard]] constexpr explicit entry_cmp(
            const vertex_map_t<G, bool> & strong_map)
            : _strong_map(strong_map) {}

        constexpr bool operator()(const auto & e1,
                                  const auto & e2) const noexcept {
            if(e1.second == e2.second) {
                if constexpr(traits::strictly_strong)
                    return !_strong_map.get()[e1.first] &&
                           _strong_map.get()[e2.first];
                else
                    return _strong_map.get()[e1.first] &&
                           !_strong_map.get()[e2.first];
            }
            return traits::semiring::less(e1.second, e2.second);
        }
    };
    using heap = typename traits::heap<entry_cmp>;

    std::reference_wrapper<const G> _graph;
    std::reference_wrapper<const L1> _lower_length_map;
    std::reference_wrapper<const L2> _upper_length_map;

    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };
    vertex_map_t<G, vertex_status> _vertex_status_map;
    vertex_map_t<G, bool> _vertex_strong_map;
    heap _heap;
    std::size_t _nb_strong_candidates;

public:
    strong_fiber(const G & g, const L1 & l1, const L2 & l2)
        : _graph(g)
        , _lower_length_map(l1)
        , _upper_length_map(l2)
        , _vertex_status_map(create_vertex_map<vertex_status>(g, PRE_HEAP))
        , _vertex_strong_map(create_vertex_map<bool>(g))
        , _heap(create_vertex_map<std::size_t>(g),
                entry_cmp(_vertex_strong_map))
        , _nb_strong_candidates(0) {}

    strong_fiber & set_lower_length_map(const L1 & l1) noexcept {
        _lower_length_map = std::ref(l1);
        return *this;
    }

    strong_fiber & set_upper_length_map(const L2 & l2) noexcept {
        _upper_length_map = std::ref(l2);
        return *this;
    }

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

    template <input_value_map_of<arc, bool> S,
              input_value_map_of<arc, value_t> L,
              input_value_map_of<arc, value_t> U>
    strong_fiber & add_source(const vertex & u, const S & is_strong_arc,
                              const value_t & u_dist,
                              const L & lower_length_map,
                              const U & upper_length_map) noexcept {
        discard(u);
        for(const arc a : out_arcs(_graph.get(), u)) {
            const vertex & w = arc_target(_graph.get(), a);
            if(is_strong_arc[a]) {
                process_strong_vertex(
                    w, traits::semiring::plus(u_dist, upper_length_map[a]));
            } else {
                process_useless_vertex(
                    w, traits::semiring::plus(u_dist, lower_length_map[a]));
            }
        }
        return *this;
    }

    template <input_value_map_of<arc, bool> S>
    strong_fiber & add_source(
        const vertex & u, const S & is_strong_arc,
        const value_t & u_dist = traits::semiring::zero) noexcept {
        return add_source(u, is_strong_arc, u_dist, _lower_length_map.get(),
                          _upper_length_map.get());
    }

    void process_strong_vertex(const vertex & w,
                               const value_t new_dist) noexcept {
        auto && w_status = _vertex_status_map[w];
        if(w_status == IN_HEAP) {
            const value_t old_dist = _heap.priority(w);
            if(constexpr_ternary<traits::strictly_strong>(
                   traits::semiring::less(new_dist, old_dist),
                   traits::semiring::less(new_dist, old_dist) ||
                       (new_dist == old_dist && !_vertex_strong_map[w]))) {
                if(!_vertex_strong_map[w]) {
                    _vertex_strong_map[w] = true;
                    ++_nb_strong_candidates;
                }
                _heap.promote(w, new_dist);
            }
        } else if(w_status == PRE_HEAP) {
            _vertex_strong_map[w] = true;
            _heap.push(w, new_dist);
            _vertex_status_map[w] = IN_HEAP;
            ++_nb_strong_candidates;
        }
    }

    void process_useless_vertex(const vertex & w,
                                const value_t new_dist) noexcept {
        auto && w_status = _vertex_status_map[w];
        if(w_status == IN_HEAP) {
            const value_t old_dist = _heap.priority(w);
            if(constexpr_ternary<traits::strictly_strong>(
                   traits::semiring::less(new_dist, old_dist) ||
                       (new_dist == old_dist && _vertex_strong_map[w]),
                   traits::semiring::less(new_dist, old_dist))) {
                if(_vertex_strong_map[w]) {
                    _vertex_strong_map[w] = false;
                    --_nb_strong_candidates;
                }
                _heap.promote(w, new_dist);
            }
        } else if(w_status == PRE_HEAP) {
            _vertex_strong_map[w] = false;
            _heap.push(w, new_dist);
            _vertex_status_map[w] = IN_HEAP;
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
            prefetch_mapped_values(out_arcs_range,
                                   arc_targets_map(_graph.get()));
            if(_vertex_strong_map[t]) {
                prefetch_mapped_values(out_arcs_range, _upper_length_map.get());
                _heap.pop();
                --_nb_strong_candidates;
                for(const arc & a : out_arcs_range) {
                    const vertex & w = arc_target(_graph.get(), a);
                    process_strong_vertex(
                        w, traits::semiring::plus(t_dist,
                                                  _upper_length_map.get()[a]));
                }
            } else {
                prefetch_mapped_values(out_arcs_range, _lower_length_map.get());
                _heap.pop();
                for(const arc a : out_arcs_range) {
                    const vertex & w = arc_target(_graph.get(), a);
                    process_useless_vertex(
                        w, traits::semiring::plus(t_dist,
                                                  _lower_length_map.get()[a]));
                }
            }

            // std::cout << t << ':' << t_dist << "  " << _nb_strong_candidates
            //           << std::endl;

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
