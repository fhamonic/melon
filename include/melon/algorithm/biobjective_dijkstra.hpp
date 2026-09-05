#pragma once

#include <cassert>
#include <ranges>
#include <set>
#include <utility>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/maps/element.hpp"
#include "melon/numeric/semiring.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/priority_queue.hpp"

namespace melon {

// clang-format on
struct biobjective_dijkstra_roles {
    struct pareto_front {};
};

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
                   typename blue_semiring::less_t, maps::element<1, 0>>;
};

// Label-setting Pareto search over two independent costs: instead of one
// distance per vertex it keeps the whole Pareto front of (blue, red) label
// pairs, discarding dominated ones as they appear. Iterating yields the
// non-dominated labels in order of increasing blue cost -- a vertex is
// therefore produced once per label it keeps, not once overall -- and
// pareto_front(v) exposes the front accumulated so far for a vertex.
// Two preconditions on the mapped values, uncheckable by any concept. The
// first is melon::dijkstra's, on both maps: a length must never improve a cost
// when combined. The second is what makes the pruning sound: both plus
// operations must preserve dominance under extension -- if one label dominates
// another at a vertex, extending both along the same arc must keep it
// dominating. Otherwise a discarded label was on the true Pareto front and the
// output is silently incomplete.
// Cost is output-sensitive: the number of Pareto-optimal labels can grow
// exponentially with the size of the graph, so there is no polynomial bound.
template <graph_view Graph, mapping_view<arc_t<Graph>> BlueLengthMap,
          mapping_view<arc_t<Graph>> RedLengthMap,
          biobjective_dijkstra_traits Traits =
              biobjective_dijkstra_default_traits<
                  Graph, mapped_value_t<BlueLengthMap, arc_t<Graph>>,
                  mapped_value_t<RedLengthMap, arc_t<Graph>>>>
    requires outward_incidence_graph<Graph> && has_vertex_map<Graph>
class biobjective_dijkstra
    : public algorithm_view_interface<
          biobjective_dijkstra<Graph, BlueLengthMap, RedLengthMap, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;
    using blue_length_type = mapped_value_t<BlueLengthMap, arc_t<Graph>>;
    using red_length_type = mapped_value_t<RedLengthMap, arc_t<Graph>>;
    using heap = Traits::heap;
    using label = Traits::label;
    struct labels_cmp {
        [[nodiscard]] constexpr bool operator()(const label & l1,
                                                const label & l2) const {
            return Traits::blue_semiring::less(l1.first, l2.first);
        }
    };
    using labels_set = std::set<label, labels_cmp>;

private:
    Graph _graph;
    BlueLengthMap _blue_length_map;
    RedLengthMap _red_length_map;

    vertex_map_t<Graph, labels_set, biobjective_dijkstra_roles::pareto_front>
        _pareto_front_map;
    heap _heap;

public:
    // ---- Construction -------------------------------------------------------

    template <graph_for<Graph> G, mapping_for<BlueLengthMap> BLM,
              mapping_for<RedLengthMap> RLM>
    biobjective_dijkstra(G && g, BLM && blm, RLM && rlm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _blue_length_map(maps::mapping_all(std::forward<BLM>(blm)))
        , _red_length_map(maps::mapping_all(std::forward<RLM>(rlm)))
        , _pareto_front_map(
              create_vertex_map<labels_set,
                                biobjective_dijkstra_roles::pareto_front>(
                  _graph))
        , _heap() {}

    template <typename... Args>
        requires std::constructible_from<biobjective_dijkstra, Args...>
    constexpr biobjective_dijkstra(Traits, Args &&... args)
        : biobjective_dijkstra(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept.
    constexpr biobjective_dijkstra(const biobjective_dijkstra &) = delete;
    constexpr biobjective_dijkstra(biobjective_dijkstra &&) = default;

    constexpr biobjective_dijkstra & operator=(const biobjective_dijkstra &) =
        delete;
    constexpr biobjective_dijkstra & operator=(biobjective_dijkstra &&) =
        default;

    // ---- Base access --------------------------------------------------------

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

    // ---- Setup --------------------------------------------------------------

    template <mapping_for<BlueLengthMap> BLM>
        requires std::assignable_from<BlueLengthMap &, BlueLengthMap>
    biobjective_dijkstra & set_blue_length_map(BLM && blue_length_map) {
        _blue_length_map =
            maps::mapping_all(std::forward<BLM>(blue_length_map));
        return *this;
    }

    template <mapping_for<RedLengthMap> RLM>
        requires std::assignable_from<RedLengthMap &, RedLengthMap>
    biobjective_dijkstra & set_red_length_map(RLM && red_length_map) {
        _red_length_map = maps::mapping_all(std::forward<RLM>(red_length_map));
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
    // Private: a public relax() would let callers push arbitrary labels behind
    // the traversal's back, breaking the invariant that every heap entry was
    // non-dominated when inserted. Sources enter through add_source().
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

    // Restores the generator invariant after every mutation: either the heap
    // is empty or its top is a live, non-dominated label. Skipping lazily on
    // the *next* advance() instead leaves a dominated top for current() to
    // hand out between the calls -- run() never notices, only iteration does.
    constexpr void skip_dominated_prefix() {
        while(!_heap.empty()) {
            const auto & [t, t_label] = _heap.top();
            if(!is_dominated(t, t_label)) break;
            _heap.pop();
        }
    }

public:
    // ---- Setup --------------------------------------------------------------

    biobjective_dijkstra & add_source(
        const vertex & s,
        const blue_length_type & blue_length = Traits::blue_semiring::zero,
        const red_length_type & red_length = Traits::red_semiring::zero) {
        relax(s, std::make_pair(blue_length, red_length));
        skip_dominated_prefix();
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_heap.empty())) {
        return _heap.empty();
    }

    // The noexcept measures the copy the by-value return performs, not just the
    // top() call.
    [[nodiscard]] constexpr auto current() const
        noexcept(noexcept(typename heap::value_type(_heap.top()))) {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() {
        assert(!finished());
        // A copy, not a reference binding: top() returns a reference into
        // the heap array, and t_label is read after the pop() below
        // reorders it.
        const auto [t, t_label] = _heap.top();
        auto && out_arcs_range = out_arcs(_graph, t);
        detail::prefetch_keys_and_values(out_arcs_range,
                                         arc_targets_map(_graph),
                                         _blue_length_map, _red_length_map);
        _heap.pop();
        for(const arc & a : out_arcs_range) {
            const vertex & w = arc_target(_graph, a);
            relax(w, std::make_pair(Traits::blue_semiring::plus(
                                        t_label.first, _blue_length_map[a]),
                                    Traits::red_semiring::plus(
                                        t_label.second, _red_length_map[a])));
        }
        skip_dominated_prefix();
    }

    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] constexpr auto pareto_front(const vertex & v) const
        noexcept(noexcept(std::views::all(_pareto_front_map[v]))) {
        return std::views::all(_pareto_front_map[v]);
    }

    // A vertex is reached once it holds at least one non-dominated label.
    [[nodiscard]] constexpr bool reached(const vertex & v) const
        noexcept(noexcept(_pareto_front_map[v].empty())) {
        return !_pareto_front_map[v].empty();
    }
    // Derived, not stored: valid while this object lives and stays put.
    [[nodiscard]] constexpr auto reached_map() const & {
        return maps::function([this](const vertex & v) { return reached(v); });
    }
    // Terminal, like std::move(alg).base() -- the member left behind is valid
    // but empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto reached_map() && {
        return maps::function(
            [front_map = std::move(_pareto_front_map)](const vertex & v) {
                return !front_map[v].empty();
            });
    }
};

template <typename Graph, typename BlueLengthMap, typename RedLengthMap>
biobjective_dijkstra(Graph &&, BlueLengthMap &&, RedLengthMap &&)
    -> biobjective_dijkstra<views::graph_all_t<Graph>,
                            maps::mapping_all_t<BlueLengthMap>,
                            maps::mapping_all_t<RedLengthMap>>;

template <typename Graph, typename BlueLengthMap, typename RedLengthMap,
          typename Traits>
biobjective_dijkstra(Traits, Graph &&, BlueLengthMap &&, RedLengthMap &&)
    -> biobjective_dijkstra<views::graph_all_t<Graph>,
                            maps::mapping_all_t<BlueLengthMap>,
                            maps::mapping_all_t<RedLengthMap>, Traits>;

}  // namespace melon
