#pragma once

#include <cassert>
#include <ranges>
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
concept biobjective_dijkstra_traits =
    semiring<typename Traits::blue_semiring> &&
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
                   typename blue_semiring::less_t, maps::element_map<1, 0>>;
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
template <graph_view Graph, mapping_view<arc_t<Graph>> BLM,
          mapping_view<arc_t<Graph>> RLM,
          biobjective_dijkstra_traits Traits =
              biobjective_dijkstra_default_traits<
                  Graph, mapped_value_t<BLM, arc_t<Graph>>,
                  mapped_value_t<RLM, arc_t<Graph>>>>
    requires outward_incidence_graph<Graph> && has_vertex_map<Graph>
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
    // graph_storable_as / mapping_storable_as, so std::is_constructible
    // answers what the mem-initializers actually do -- see dijkstra's
    // constructor.
    template <typename G, typename BlueMap, typename RedMap>
        requires graph_storable_as<G, Graph> &&
                     mapping_storable_as<BlueMap, BLM> &&
                     mapping_storable_as<RedMap, RLM>
    biobjective_dijkstra(G && g, BlueMap && l1, RedMap && l2)
        : _graph(detail::store_graph<Graph>(std::forward<G>(g)))
        , _blue_length_map(
              detail::store_mapping<BLM>(std::forward<BlueMap>(l1)))
        , _red_length_map(detail::store_mapping<RLM>(std::forward<RedMap>(l2)))
        , _pareto_front_map(
              create_vertex_map<std::set<label, labels_cmp>>(_graph))
        , _heap() {}

    template <typename... Args>
        requires std::constructible_from<biobjective_dijkstra, Args...>
    constexpr biobjective_dijkstra(Traits, Args &&... args)
        : biobjective_dijkstra(std::forward<Args>(args)...) {}

    constexpr biobjective_dijkstra(const biobjective_dijkstra &) = default;
    constexpr biobjective_dijkstra(biobjective_dijkstra &&) = default;

    constexpr biobjective_dijkstra & operator=(const biobjective_dijkstra &) =
        default;
    constexpr biobjective_dijkstra & operator=(biobjective_dijkstra &&) =
        default;

    // The graph the algorithm runs over. An algorithm owns its view rather
    // than adapting it, so this is the std::ranges::owning_view shape --
    // references, ref-qualified -- and not the filter_view shape the graph
    // *views* use. Returning a copy here would also put traversal_forest back
    // where it started: it reaches its sources through base(), and an owned
    // graph view is move-only.
    [[nodiscard]] constexpr Graph & base() & noexcept { return _graph; }
    [[nodiscard]] constexpr const Graph & base() const & noexcept {
        return _graph;
    }
    [[nodiscard]] constexpr Graph && base() && noexcept {
        return std::move(_graph);
    }
    [[nodiscard]] constexpr const Graph && base() const && noexcept {
        return std::move(_graph);
    }

    template <typename BlueMap>
        requires mapping_storable_as<BlueMap, BLM> &&
                 std::assignable_from<BLM &, BLM>
    biobjective_dijkstra & set_blue_length_map(BlueMap && blue_length_map) {
        _blue_length_map =
            detail::store_mapping<BLM>(std::forward<BlueMap>(blue_length_map));
        return *this;
    }

    template <typename RedMap>
        requires mapping_storable_as<RedMap, RLM> &&
                 std::assignable_from<RLM &, RLM>
    biobjective_dijkstra & set_red_length_map(RedMap && red_length_map) {
        _red_length_map =
            detail::store_mapping<RLM>(std::forward<RedMap>(red_length_map));
        return *this;
    }

    biobjective_dijkstra & reset() {
        for(const vertex & v : vertices(_graph)) _pareto_front_map[v].clear();
        _heap.clear();
        return *this;
    }

    [[nodiscard]] bool is_dominated(const vertex & v, const label & l) const {
        auto & labels = _pareto_front_map[v];
        auto it = labels.upper_bound(l);
        if(it == labels.begin()) return false;
        const auto pred_it = std::prev(it);
        if(Traits::blue_semiring::less(pred_it->first, l.first))
            return !Traits::red_semiring::less(l.second, pred_it->second);
        return Traits::red_semiring::less(pred_it->second, l.second);
    }

private:
    // Private, like every other algorithm's relaxation step: a public relax()
    // let callers push arbitrary labels behind the traversal's back -- the
    // same encapsulation hole as strongly_connected_components' removed
    // push_tarjan. Sources enter through add_source().
    void relax(const vertex & v, const label & l) {
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

public:
    biobjective_dijkstra & add_source(
        const vertex & s,
        const blue_length_type blue_length = Traits::blue_semiring::zero,
        const red_length_type red_length = Traits::red_semiring::zero) {
        relax(s, std::make_pair(blue_length, red_length));
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_heap.empty())) {
        return _heap.empty();
    }

    // See competing_dijkstras::current(): the noexcept measures the copy the
    // return performs, not just the top() call.
    [[nodiscard]] constexpr auto current() const
        noexcept(noexcept(typename heap::value_type(_heap.top()))) {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() {
        assert(!finished());
        for(;;) {
            // A copy, not a reference binding: top() returns a reference into
            // the heap array, and t_label is read after the pop() below
            // reorders it.
            const auto [t, t_label] = _heap.top();
            if(is_dominated(t, t_label)) {
                _heap.pop();
                if(_heap.empty()) return;
                continue;
            }
            auto && out_arcs_range = out_arcs(_graph, t);
            prefetch_keys_and_values(out_arcs_range, arc_targets_map(_graph),
                                     _blue_length_map, _red_length_map);
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

    [[nodiscard]] constexpr auto pareto_front(const vertex & v) const
        noexcept(noexcept(std::views::all(_pareto_front_map[v]))) {
        return std::views::all(_pareto_front_map[v]);
    }

    // reached()/reached_map(), like every rooted algorithm in the family: a
    // vertex is reached once it holds at least one non-dominated label.
    [[nodiscard]] constexpr bool reached(const vertex & v) const
        noexcept(noexcept(_pareto_front_map[v].empty())) {
        return !_pareto_front_map[v].empty();
    }
    // Derived, like dijkstra's: valid while this object lives and stays put.
    [[nodiscard]] constexpr auto reached_map() const {
        return maps::map([this](const vertex & v) { return reached(v); });
    }
};

// No Traits parameter: see competing_dijkstras' guide for why defaulting it
// here made the deduced type differ from the spelled-out one.
template <typename Graph, typename BLM, typename RLM>
biobjective_dijkstra(Graph &&, BLM &&, RLM &&)
    -> biobjective_dijkstra<views::graph_all_t<Graph>, maps::mapping_all_t<BLM>,
                            maps::mapping_all_t<RLM>>;

template <typename Graph, typename BLM, typename RLM, typename Traits>
biobjective_dijkstra(Traits, Graph &&, BLM &&, RLM &&)
    -> biobjective_dijkstra<views::graph_all_t<Graph>, maps::mapping_all_t<BLM>,
                            maps::mapping_all_t<RLM>, Traits>;

}  // namespace melon
