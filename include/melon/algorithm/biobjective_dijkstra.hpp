#ifndef MELON_BIOBJECTIVE_DIJKSTRA_HPP
#define MELON_BIOBJECTIVE_DIJKSTRA_HPP

#include <set>
#include <utility>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"

namespace fhamonic {
namespace melon {

// clang-format on
template <typename _Traits>
concept biobjective_dijkstra_trait =
    semiring<typename _Traits::blue_semiring> &&
    semiring<typename _Traits::red_semiring> &&
    priority_queue<typename _Traits::heap> &&
    requires(typename _Traits::label & e) {
        { e.first };
        { e.second };
    };
// clang-format on

template <outward_incidence_graph _Graph, typename _BlueValueType,
          typename _RedValueType>
struct biobjective_dijkstra_default_traits {
    using blue_semiring = shortest_path_semiring<_BlueValueType>;
    using red_semiring = shortest_path_semiring<_RedValueType>;
    using label = std::pair<_BlueValueType, _RedValueType>;
    struct label_blue_cmp {
        [[nodiscard]] constexpr bool operator()(const label & e1,
                                                const label & e2) const {
            return blue_semiring::less(e1.first, e2.first);
        }
    };
    using heap = d_ary_heap<2, std::pair<vertex_t<_Graph>, label>,
                            label_blue_cmp, views::element_map<1>>;
};

template <outward_incidence_graph _Graph, input_mapping<arc_t<_Graph>> BLM,
          input_mapping<arc_t<_Graph>> RLM,
          biobjective_dijkstra_trait _Traits =
              biobjective_dijkstra_default_traits<
                  _Graph, mapped_value_t<BLM, arc_t<_Graph>>,
                  mapped_value_t<RLM, arc_t<_Graph>>>>
class biobjective_dijkstra
    : public algorithm_view_interface<
          biobjective_dijkstra<_Graph, BLM, RLM, _Traits>> {
private:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;
    using blue_length_type = mapped_value_t<BLM, arc_t<_Graph>>;
    using red_length_type = mapped_value_t<RLM, arc_t<_Graph>>;
    using heap = _Traits::heap;
    using label = _Traits::label;

private:
    _Graph _graph;
    BLM _blue_length_map;
    RLM _red_length_map;

    struct labels_cmp {
        [[nodiscard]] constexpr bool operator()(const label & l1,
                                                const label & l2) const {
            return _Traits::blue_semiring::less(l1.first, l2.first);
        }
    };
    vertex_map_t<_Graph, std::set<label, labels_cmp>> _pareto_front_map;
    heap _heap;

public:
    template <typename _G, typename _BLM, typename _RLM>
    biobjective_dijkstra(_G && g, _BLM && l1, _RLM && l2)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _blue_length_map(views::mapping_all(std::forward<_BLM>(l1)))
        , _red_length_map(views::mapping_all(std::forward<_RLM>(l2)))
        , _pareto_front_map(
              create_vertex_map<std::set<label, labels_cmp>>(_graph))
        , _heap() {}

    template <typename... _Args>
    [[nodiscard]] constexpr biobjective_dijkstra(_Traits, _Args &&... args)
        : biobjective_dijkstra(std::forward<_Args>(args)...) {}

    template <typename _BLM>
    biobjective_dijkstra & set_blue_length_map(
        _BLM && blue_length_map) noexcept {
        _blue_length_map =
            views::mapping_all(std::forward<_BLM>(blue_length_map));
        return *this;
    }

    template <typename _RLM>
    biobjective_dijkstra & set_red_length_map(_RLM && red_length_map) noexcept {
        _red_length_map =
            views::mapping_all(std::forward<_RLM>(red_length_map));
        return *this;
    }

    biobjective_dijkstra & reset() noexcept {
        for(const vertex & v : vertices(_graph)) _pareto_front_map[v].clear();
        _heap.clear();
        return *this;
    }

    bool is_dominated(const vertex & v, const label & l) const noexcept {
        auto & labels = _pareto_front_map[v];
        auto it = labels.upper_bound(l);
        if(it == labels.begin()) return false;
        const auto pred_it = std::prev(it);
        if(_Traits::blue_semiring::less(pred_it->first, l.first))
            return !_Traits::red_semiring::less(l.second, pred_it->second);
        return _Traits::red_semiring::less(pred_it->second, l.second);
    }

    void relax(const vertex & v, const label & l) noexcept {
        auto & labels = _pareto_front_map[v];
        auto it = labels.upper_bound(l);
        auto last_sub_it = it;

        if(it != labels.begin()) {
            const auto pred_it = std::prev(it);
            if(!_Traits::red_semiring::less(l.second, pred_it->second)) return;
            if(!_Traits::blue_semiring::less(pred_it->first, l.first))
                it = pred_it;
        }

        while(last_sub_it != labels.end() &&
              !_Traits::red_semiring::less(last_sub_it->second, l.second))
            ++last_sub_it;

        labels.insert(labels.erase(it, last_sub_it), l);
        _heap.push(std::make_pair(v, l));
    }

    biobjective_dijkstra & add_source(
        const vertex & s,
        const blue_length_type blue_length = _Traits::blue_semiring::zero,
        const red_length_type red_length =
            _Traits::red_semiring::zero) noexcept {
        relax(s, std::make_pair(blue_length, red_length));
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _heap.empty();
    }

    [[nodiscard]] constexpr auto current() const noexcept {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() noexcept {
        for(;;) {
            const auto && [t, t_label] = _heap.top();
            if(is_dominated(t, t_label)) {
                _heap.pop();
                if(_heap.empty()) return;
                continue;
            }
            auto && out_arcs_range = out_arcs(_graph, t);
            prefetch_range(out_arcs_range);
            prefetch_mapped_values(out_arcs_range, arc_targets_map(_graph));
            prefetch_mapped_values(out_arcs_range, _blue_length_map);
            prefetch_mapped_values(out_arcs_range, _red_length_map);
            _heap.pop();
            for(const arc & a : out_arcs_range) {
                const vertex & w = arc_target(_graph, a);
                relax(w,
                      std::make_pair(_Traits::blue_semiring::plus(
                                         t_label.first, _blue_length_map[a]),
                                     _Traits::red_semiring::plus(
                                         t_label.second, _red_length_map[a])));
            }
            return;
        }
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }

    constexpr auto pareto_front(const vertex & v) const noexcept {
        return std::views::all(_pareto_front_map[v]);
    }
};

template <typename _Graph, typename _BLM, typename _RLM,
          typename _Traits = biobjective_dijkstra_default_traits<
              _Graph, mapped_value_t<_BLM, arc_t<_Graph>>,
              mapped_value_t<_RLM, arc_t<_Graph>>>>
biobjective_dijkstra(_Graph &&, _BLM &&, _RLM &&)
    -> biobjective_dijkstra<views::graph_all_t<_Graph>,
                            views::mapping_all_t<_BLM>,
                            views::mapping_all_t<_RLM>, _Traits>;

template <typename _Graph, typename _BLM, typename _RLM, typename _Traits>
biobjective_dijkstra(_Traits, _Graph &&, _BLM &&, _RLM &&)
    -> biobjective_dijkstra<views::graph_all_t<_Graph>,
                            views::mapping_all_t<_BLM>,
                            views::mapping_all_t<_RLM>, _Traits>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_BIOBJECTIVE_DIJKSTRA_HPP
