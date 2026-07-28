#pragma once

#include <set>
#include <utility>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"

namespace melon {

// clang-format on
template <typename Traits>
concept biobjective_dijkstra_trait = semiring<typename Traits::blue_semiring> &&
                                     semiring<typename Traits::red_semiring> &&
                                     priority_queue<typename Traits::heap> &&
                                     requires(typename Traits::label & e) {
                                         { e.first };
                                         { e.second };
                                     };
// clang-format on

template <outward_incidence_graph Graph, typename BlueValueType,
          typename RedValueType>
struct biobjective_dijkstra_default_traits {
    using blue_semiring = shortest_path_semiring<BlueValueType>;
    using red_semiring = shortest_path_semiring<RedValueType>;
    using label = std::pair<BlueValueType, RedValueType>;
    using heap =
        d_ary_heap<2, std::pair<vertex_t<Graph>, label>,
                   typename blue_semiring::less_t, views::element_map<1, 0>>;
};

// Label-setting Pareto search over two independent costs: instead of one
// distance per vertex it keeps the whole Pareto front of (blue, red) label
// pairs, discarding dominated ones as they appear. Iterating yields the
// non-dominated labels in order of increasing blue cost -- a vertex is
// therefore produced once per label it keeps, not once overall -- and
// pareto_front(v) exposes the front accumulated so far for a vertex. Same
// non-negativity requirement as melon::dijkstra, on both maps.
// Cost is output-sensitive: the number of Pareto-optimal labels can grow
// exponentially with the size of the graph, so there is no polynomial bound.
template <outward_incidence_graph Graph, input_mapping<arc_t<Graph>> BLM,
          input_mapping<arc_t<Graph>> RLM,
          biobjective_dijkstra_trait Traits =
              biobjective_dijkstra_default_traits<
                  Graph, mapped_value_t<BLM, arc_t<Graph>>,
                  mapped_value_t<RLM, arc_t<Graph>>>>
class biobjective_dijkstra
    : public algorithm_view_interface<
          biobjective_dijkstra<Graph, BLM, RLM, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;
    using blue_length_type = mapped_value_t<BLM, arc_t<Graph>>;
    using red_length_type = mapped_value_t<RLM, arc_t<Graph>>;
    using heap = Traits::heap;
    using label = Traits::label;

private:
    Graph _graph;
    BLM _blue_length_map;
    RLM _red_length_map;

    struct labels_cmp {
        [[nodiscard]] constexpr bool operator()(const label & l1,
                                                const label & l2) const {
            return Traits::blue_semiring::less(l1.first, l2.first);
        }
    };
    vertex_map_t<Graph, std::set<label, labels_cmp>> _pareto_front_map;
    heap _heap;

public:
    template <typename G, typename BlueMap, typename RedMap>
    biobjective_dijkstra(G && g, BlueMap && l1, RedMap && l2)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _blue_length_map(views::mapping_all(std::forward<BlueMap>(l1)))
        , _red_length_map(views::mapping_all(std::forward<RedMap>(l2)))
        , _pareto_front_map(
              create_vertex_map<std::set<label, labels_cmp>>(_graph))
        , _heap() {}

    template <typename... Args>
    [[nodiscard]] constexpr biobjective_dijkstra(Traits, Args &&... args)
        : biobjective_dijkstra(std::forward<Args>(args)...) {}

    [[nodiscard]] constexpr biobjective_dijkstra(const biobjective_dijkstra &) =
        default;
    [[nodiscard]] constexpr biobjective_dijkstra(biobjective_dijkstra &&) =
        default;

    constexpr biobjective_dijkstra & operator=(const biobjective_dijkstra &) =
        default;
    constexpr biobjective_dijkstra & operator=(biobjective_dijkstra &&) =
        default;

    template <typename BlueMap>
    biobjective_dijkstra & set_blue_length_map(
        BlueMap && blue_length_map) noexcept {
        _blue_length_map =
            views::mapping_all(std::forward<BlueMap>(blue_length_map));
        return *this;
    }

    template <typename RedMap>
    biobjective_dijkstra & set_red_length_map(
        RedMap && red_length_map) noexcept {
        _red_length_map =
            views::mapping_all(std::forward<RedMap>(red_length_map));
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
        if(Traits::blue_semiring::less(pred_it->first, l.first))
            return !Traits::red_semiring::less(l.second, pred_it->second);
        return Traits::red_semiring::less(pred_it->second, l.second);
    }

    void relax(const vertex & v, const label & l) noexcept {
        auto & labels = _pareto_front_map[v];
        auto it = labels.upper_bound(l);
        auto last_sub_it = it;

        if(it != labels.begin()) {
            const auto pred_it = std::prev(it);
            if(!Traits::red_semiring::less(l.second, pred_it->second)) return;
            if(!Traits::blue_semiring::less(pred_it->first, l.first))
                it = pred_it;
        }

        while(last_sub_it != labels.end() &&
              !Traits::red_semiring::less(last_sub_it->second, l.second))
            ++last_sub_it;

        labels.insert(labels.erase(it, last_sub_it), l);
        _heap.push(std::make_pair(v, l));
    }

    biobjective_dijkstra & add_source(
        const vertex & s,
        const blue_length_type blue_length = Traits::blue_semiring::zero,
        const red_length_type red_length =
            Traits::red_semiring::zero) noexcept {
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
                      std::make_pair(Traits::blue_semiring::plus(
                                         t_label.first, _blue_length_map[a]),
                                     Traits::red_semiring::plus(
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

template <typename Graph, typename BLM, typename RLM,
          typename Traits = biobjective_dijkstra_default_traits<
              Graph, mapped_value_t<views::mapping_all_t<BLM>, arc_t<Graph>>,
              mapped_value_t<views::mapping_all_t<RLM>, arc_t<Graph>>>>
biobjective_dijkstra(Graph &&, BLM &&, RLM &&)
    -> biobjective_dijkstra<views::graph_all_t<Graph>,
                            views::mapping_all_t<BLM>,
                            views::mapping_all_t<RLM>, Traits>;

template <typename Graph, typename BLM, typename RLM, typename Traits>
biobjective_dijkstra(Traits, Graph &&, BLM &&, RLM &&)
    -> biobjective_dijkstra<views::graph_all_t<Graph>,
                            views::mapping_all_t<BLM>,
                            views::mapping_all_t<RLM>, Traits>;

}  // namespace melon
