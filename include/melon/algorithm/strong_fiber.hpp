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
#include "melon/mapping.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"
#include "melon/utility/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
template <typename _Traits>
concept strong_fiber_trait = semiring<typename _Traits::semiring> &&
    updatable_priority_queue<typename _Traits::heap<>> && requires() {
    { _Traits::store_distances } -> std::convertible_to<bool>;
    { _Traits::store_paths } -> std::convertible_to<bool>;
};
// clang-format on

template <outward_incidence_graph _Graph, typename _Traits>
struct strong_fiber_default_traits {
    using semiring = shortest_path_semiring<_Traits>;
    template <typename CMP = std::less<std::pair<vertex_t<_Graph>, _Traits>>>
    using heap = d_ary_heap<2, vertex_t<_Graph>, _Traits, CMP,
                            vertex_map_t<_Graph, std::size_t>>;

    static constexpr bool strictly_strong = false;
    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};

template <outward_incidence_graph _Graph, input_mapping<arc_t<_Graph>> L1,
          input_mapping<arc_t<_Graph>> L2,
          strong_fiber_trait _Traits = strong_fiber_default_traits<
              _Graph, mapped_value_t<L1, arc_t<_Graph>>>>
    requires std::is_same_v<mapped_value_t<L1, arc_t<_Graph>>,
                            mapped_value_t<L2, arc_t<_Graph>>>
class strong_fiber {
private:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;
    using value_t = mapped_value_t<L1, arc_t<_Graph>>;

    struct entry_cmp {
        std::reference_wrapper<const vertex_map_t<_Graph, bool>> _strong_map;

        [[nodiscard]] constexpr explicit entry_cmp(
            const vertex_map_t<_Graph, bool> & strong_map)
            : _strong_map(strong_map) {}

        constexpr bool operator()(const auto & e1,
                                  const auto & e2) const noexcept {
            if(e1.second == e2.second) {
                if constexpr(_Traits::strictly_strong)
                    return !_strong_map.get()[e1.first] &&
                           _strong_map.get()[e2.first];
                else
                    return _strong_map.get()[e1.first] &&
                           !_strong_map.get()[e2.first];
            }
            return _Traits::semiring::less(e1.second, e2.second);
        }
    };
    using heap = typename _Traits::heap<entry_cmp>;

private:
    _Graph _graph;
    L1 _reduced_length_map;
    L2 _default_length_map;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };
    vertex_map_t<_Graph, vertex_status> _vertex_status_map;
    vertex_map_t<_Graph, bool> _vertex_strong_map;
    heap _heap;
    std::size_t _nb_strong_candidates;

public:
    template <typename _G, typename _L1, typename _L2>
    strong_fiber(_G && g, _L1 && l1, _L2 && l2)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _reduced_length_map(views::mapping_all(std::forward<_L1>(l1)))
        , _default_length_map(views::mapping_all(std::forward<_L2>(l2)))
        , _vertex_status_map(create_vertex_map<vertex_status>(_graph, PRE_HEAP))
        , _vertex_strong_map(create_vertex_map<bool>(_graph))
        , _heap(create_vertex_map<std::size_t>(_graph),
                entry_cmp(_vertex_strong_map))
        , _nb_strong_candidates(0) {}

    template <typename... _Args>
    [[nodiscard]] constexpr strong_fiber(_Traits, _Args &&... args)
        : strong_fiber(std::forward<_Args>(args)...) {}

    strong_fiber & set_reduced_length_map(const L1 & l1) noexcept {
        _reduced_length_map = std::ref(l1);
        return *this;
    }

    strong_fiber & set_default_length_map(const L2 & l2) noexcept {
        _default_length_map = std::ref(l2);
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

    template <input_mapping_of<arc, bool> S, input_mapping_of<arc, value_t> L,
              input_mapping_of<arc, value_t> U>
    strong_fiber & add_source(const vertex & u, const S & is_strong_arc,
                              const value_t & u_dist,
                              const L & reduced_length_map,
                              const U & upper_length_map) noexcept {
        discard(u);
        for(const arc a : out_arcs(_graph, u)) {
            const vertex & w = arc_target(_graph, a);
            if(is_strong_arc[a]) {
                relax_strong_vertex(
                    w, _Traits::semiring::plus(u_dist, upper_length_map[a]));
            } else {
                relax_useless_vertex(
                    w, _Traits::semiring::plus(u_dist, reduced_length_map[a]));
            }
        }
        return *this;
    }

    template <input_mapping_of<arc, bool> S>
    strong_fiber & add_source(
        const vertex & u, const S & is_strong_arc,
        const value_t & u_dist = _Traits::semiring::zero) noexcept {
        return add_source(u, is_strong_arc, u_dist, _reduced_length_map,
                          _default_length_map);
    }

    void relax_strong_vertex(
        const vertex & w,
        const value_t new_dist = _Traits::semiring::zero) noexcept {
        // std::cout << "relax_strong: " << w << " " << new_dist << std::endl;
        auto && w_status = _vertex_status_map[w];
        if(w_status == IN_HEAP) {
            const value_t old_dist = _heap.priority(w);
            if(constexpr_ternary<_Traits::strictly_strong>(
                   _Traits::semiring::less(new_dist, old_dist),
                   _Traits::semiring::less(new_dist, old_dist) ||
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

    void relax_useless_vertex(
        const vertex & w,
        const value_t new_dist = _Traits::semiring::zero) noexcept {
        // std::cout << "relax_useless: " << w << " " << new_dist << std::endl;
        auto && w_status = _vertex_status_map[w];
        if(w_status == IN_HEAP) {
            const value_t old_dist = _heap.priority(w);
            if(constexpr_ternary<_Traits::strictly_strong>(
                   _Traits::semiring::less(new_dist, old_dist) ||
                       (new_dist == old_dist && _vertex_strong_map[w]),
                   _Traits::semiring::less(new_dist, old_dist))) {
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
            // std::cout << "pop: " << t << " " << t_dist << std::endl;
            _vertex_status_map[t] = POST_HEAP;
            auto && out_arcs_range = out_arcs(_graph, t);
            prefetch_range(out_arcs_range);
            prefetch_mapped_values(out_arcs_range, arc_targets_map(_graph));
            if(_vertex_strong_map[t]) {
                prefetch_mapped_values(out_arcs_range, _default_length_map);
                _heap.pop();
                --_nb_strong_candidates;
                for(const arc & a : out_arcs_range) {
                    const vertex & w = arc_target(_graph, a);
                    relax_strong_vertex(w, _Traits::semiring::plus(
                                               t_dist, _default_length_map[a]));
                }
            } else {
                prefetch_mapped_values(out_arcs_range, _reduced_length_map);
                _heap.pop();
                for(const arc a : out_arcs_range) {
                    const vertex & w = arc_target(_graph, a);
                    relax_useless_vertex(
                        w, _Traits::semiring::plus(t_dist,
                                                   _reduced_length_map[a]));
                }
            }

            // std::cout << "nb_strong_candidates: " << _nb_strong_candidates <<
            // std::endl;

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

template <typename _Graph, typename _L1, typename _L2,
          typename _Traits = strong_fiber_default_traits<
              _Graph, mapped_value_t<_L1, arc_t<_Graph>>>>
strong_fiber(_Graph &&, _L1 &&, _L2 &&)
    -> strong_fiber<views::graph_all_t<_Graph>, views::mapping_all_t<_L1>,
                    views::mapping_all_t<_L2>, _Traits>;

template <typename _Graph, typename _L1, typename _L2, typename _Traits>
strong_fiber(_Traits, _Graph &&, _L1 &&, _L2 &&)
    -> strong_fiber<views::graph_all_t<_Graph>, views::mapping_all_t<_L1>,
                    views::mapping_all_t<_L2>, _Traits>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STRONG_FIBER_HPP
