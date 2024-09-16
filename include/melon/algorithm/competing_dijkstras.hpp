#ifndef MELON_competing_dijksTRAS_HPP
#define MELON_competing_dijksTRAS_HPP

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
template <typename _Traits>
concept competing_dijkstras_trait = semiring<typename _Traits::semiring> &&
    updatable_priority_queue<typename _Traits::heap> && requires() {
    { _Traits::store_distances } -> std::convertible_to<bool>;
    { _Traits::store_paths } -> std::convertible_to<bool>;
};
// clang-format on

template <outward_incidence_graph _Graph, typename _ValueType>
struct competing_dijkstras_default_traits {
    using semiring = shortest_path_semiring<_ValueType>;
    using entry = std::pair<_ValueType, bool>;
    static bool compare_entries(const entry & e1, const entry & e2) {
        if(e1.first == e2.first) {
            return e1.second && !e2.second;
        }
        return semiring::less(e1.first, e2.first);
    }
    struct entry_cmp {
        [[nodiscard]] constexpr bool operator()(
            const auto & e1, const auto & e2) const noexcept {
            return compare_entries(e1, e2);
        }
    };
    using heap = d_ary_heap<2, std::pair<vertex_t<_Graph>, entry>,
                            views::get_map<1>, entry_cmp, views::get_map<0>,
                            vertex_map_t<_Graph, std::size_t>>;

    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};

template <outward_incidence_graph _Graph, input_mapping<arc_t<_Graph>> BLM,
          input_mapping<arc_t<_Graph>> RLM,
          competing_dijkstras_trait _Traits =
              competing_dijkstras_default_traits<
                  _Graph, mapped_value_t<BLM, arc_t<_Graph>>>>
    requires std::is_same_v<mapped_value_t<BLM, arc_t<_Graph>>,
                            mapped_value_t<RLM, arc_t<_Graph>>>
class competing_dijkstras {
private:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;
    using value_t = mapped_value_t<BLM, arc_t<_Graph>>;
    using entry_t = typename _Traits::entry;
    using entry_cmp = typename _Traits::entry_cmp;
    using heap = typename _Traits::heap;

private:
    _Graph _graph;
    BLM _blue_length_map;
    RLM _red_length_map;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };
    vertex_map_t<_Graph, vertex_status> _vertex_status_map;
    heap _heap;
    std::size_t _nb_blue_candidates;

public:
    template <typename _G, typename _BLM, typename _RLM>
    competing_dijkstras(_G && g, _BLM && l1, _RLM && l2)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _blue_length_map(views::mapping_all(std::forward<_BLM>(l1)))
        , _red_length_map(views::mapping_all(std::forward<_RLM>(l2)))
        , _vertex_status_map(create_vertex_map<vertex_status>(_graph, PRE_HEAP))
        , _heap(create_vertex_map<std::size_t>(_graph), entry_cmp{})
        , _nb_blue_candidates(0) {}

    template <typename... _Args>
    [[nodiscard]] constexpr competing_dijkstras(_Traits, _Args &&... args)
        : competing_dijkstras(std::forward<_Args>(args)...) {}

    template <typename _BLM>
    competing_dijkstras & set_blue_length_map(
        _BLM && blue_length_map) noexcept {
        _blue_length_map =
            views::mapping_all(std::forward<_BLM>(blue_length_map));
        return *this;
    }

    template <typename _RLM>
    competing_dijkstras & set_red_length_map(_RLM && red_length_map) noexcept {
        _red_length_map =
            views::mapping_all(std::forward<_RLM>(red_length_map));
        return *this;
    }

    competing_dijkstras & reset() noexcept {
        _vertex_status_map.fill(PRE_HEAP);
        _heap.clear();
        _nb_blue_candidates = 0;
        return *this;
    }

    competing_dijkstras & add_blue_source(
        const vertex & s,
        const value_t dist_v = _Traits::semiring::zero) noexcept {
        assert(_vertex_status_map[s] != IN_HEAP);
        _heap.push(std::make_pair(s, entry_t{dist_v, true}));
        ++_nb_blue_candidates;
        _vertex_status_map[s] = IN_HEAP;
        // if constexpr(_Traits::store_paths) {
        //     _pred_arcs_map[s].reset();
        //     if constexpr(!has_arc_source<_Graph>) _pred_vertices_map[s] = s;
        // }
        return *this;
    }
    competing_dijkstras & add_red_source(
        const vertex & s,
        const value_t dist_v = _Traits::semiring::zero) noexcept {
        assert(_vertex_status_map[s] != IN_HEAP);
        _heap.push(std::make_pair(s, entry_t{dist_v, false}));
        _vertex_status_map[s] = IN_HEAP;
        // if constexpr(_Traits::store_paths) {
        //     _pred_arcs_map[s].reset();
        //     if constexpr(!has_arc_source<_Graph>) _pred_vertices_map[s] = s;
        // }
        return *this;
    }

    void relax_blue_vertex(const vertex & w,
                           const value_t new_dist_v) noexcept {
        const entry_t new_dist = {new_dist_v, true};
        auto && w_status = _vertex_status_map[w];
        if(w_status == IN_HEAP) {
            const entry_t old_dist = _heap.priority(w);
            if(_Traits::compare_entries(new_dist, old_dist)) {
                if(!old_dist.second) {
                    ++_nb_blue_candidates;
                }
                _heap.promote(w, new_dist);
            }
        } else if(w_status == PRE_HEAP) {
            _heap.push(std::make_pair(w, new_dist));
            _vertex_status_map[w] = IN_HEAP;
            ++_nb_blue_candidates;
        }
    }

    void relax_red_vertex(const vertex & w, const value_t new_dist_v) noexcept {
        const entry_t new_dist = {new_dist_v, false};
        auto && w_status = _vertex_status_map[w];
        if(w_status == IN_HEAP) {
            const entry_t old_dist = _heap.priority(w);
            if(_Traits::compare_entries(new_dist, old_dist)) {
                if(old_dist.second) {
                    --_nb_blue_candidates;
                }
                _heap.promote(w, new_dist);
            }
        } else if(w_status == PRE_HEAP) {
            _heap.push(std::make_pair(w, new_dist));
            _vertex_status_map[w] = IN_HEAP;
        }
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _nb_blue_candidates == 0;
    }

    [[nodiscard]] constexpr auto current() const noexcept {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() noexcept {
        do {
            const auto && [t, t_dist] = _heap.top();
            _vertex_status_map[t] = POST_HEAP;
            auto && out_arcs_range = out_arcs(_graph, t);
            prefetch_range(out_arcs_range);
            prefetch_mapped_values(out_arcs_range, arc_targets_map(_graph));
            if(t_dist.second) {
                prefetch_mapped_values(out_arcs_range, _blue_length_map);
                _heap.pop();
                --_nb_blue_candidates;
                for(const arc & a : out_arcs_range) {
                    const vertex & w = arc_target(_graph, a);
                    relax_blue_vertex(
                        w, _Traits::semiring::plus(t_dist.first,
                                                   _blue_length_map[a]));
                }
            } else {
                prefetch_mapped_values(out_arcs_range, _red_length_map);
                _heap.pop();
                for(const arc a : out_arcs_range) {
                    const vertex & w = arc_target(_graph, a);
                    relax_red_vertex(w, _Traits::semiring::plus(
                                            t_dist.first, _red_length_map[a]));
                }
            }
        } while(_nb_blue_candidates > 0 && !_heap.top().second.second);
    }

    constexpr void init() noexcept {
        if(!_heap.top().second.second) advance();
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }
    [[nodiscard]] constexpr auto begin() noexcept {
        init();
        return algorithm_iterator(*this);
    }
    [[nodiscard]] constexpr auto end() noexcept {
        return algorithm_end_sentinel();
    }
};

template <typename _Graph, typename _BLM, typename _RLM,
          typename _Traits = competing_dijkstras_default_traits<
              _Graph, mapped_value_t<_BLM, arc_t<_Graph>>>>
competing_dijkstras(_Graph &&, _BLM &&, _RLM &&)
    -> competing_dijkstras<views::graph_all_t<_Graph>,
                           views::mapping_all_t<_BLM>,
                           views::mapping_all_t<_RLM>, _Traits>;

template <typename _Graph, typename _BLM, typename _RLM, typename _Traits>
competing_dijkstras(_Traits, _Graph &&, _BLM &&, _RLM &&)
    -> competing_dijkstras<views::graph_all_t<_Graph>,
                           views::mapping_all_t<_BLM>,
                           views::mapping_all_t<_RLM>, _Traits>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_competing_dijksTRAS_HPP
