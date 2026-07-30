#pragma once

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"

namespace melon {

template <graph_view Graph, mapping_view<arc_t<Graph>> CapacityMap>
    requires outward_incidence_graph<Graph> && inward_incidence_graph<Graph> &&
             has_vertex_map<Graph> && has_arc_map<Graph>
class edmonds_karp {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;
    using value_t = mapped_value_t<CapacityMap, arc_t<Graph>>;

private:
    Graph _graph;
    CapacityMap _capacity_map;
    vertex _s;
    vertex _t;
    arc_map_t<Graph, value_t> _carried_flow_map;
    std::vector<vertex> _bfs_queue;
    vertex_map_t<Graph, bool> _bfs_reached_map;
    vertex_map_t<Graph, arc> _bfs_pred_arc;

public:
    // graph_storable_as / mapping_storable_as, so std::is_constructible
    // answers what the mem-initializers actually do -- see dijkstra's
    // constructor.
    template <typename G, typename M>
        requires graph_storable_as<G, Graph> &&
                     mapping_storable_as<M, CapacityMap>
    constexpr edmonds_karp(G && g, M && c)
        : _graph(detail::store_graph<Graph>(std::forward<G>(g)))
        , _capacity_map(detail::store_mapping<CapacityMap>(std::forward<M>(c)))
        , _carried_flow_map(create_arc_map<value_t>(_graph))
        , _bfs_reached_map(create_vertex_map<bool>(_graph))
        , _bfs_pred_arc(create_vertex_map<arc>(_graph)) {
        if constexpr(has_num_vertices<Graph>) {
            _bfs_queue.reserve(num_vertices(_graph));
        }
        reset();
    }

    template <typename G, typename M>
        requires graph_storable_as<G, Graph> &&
                 mapping_storable_as<M, CapacityMap>
    constexpr edmonds_karp(G && g, M && c, const vertex & s, const vertex & t)
        : edmonds_karp(std::forward<G>(g), std::forward<M>(c)) {
        set_source(s);
        set_target(t);
    }

    constexpr edmonds_karp(const edmonds_karp &) = default;
    constexpr edmonds_karp(edmonds_karp &&) = default;

    constexpr edmonds_karp & operator=(const edmonds_karp &) = default;
    constexpr edmonds_karp & operator=(edmonds_karp &&) = default;

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

    constexpr edmonds_karp & set_source(const vertex & s) {
        _s = s;
        return *this;
    }
    constexpr edmonds_karp & set_target(const vertex & t) {
        _t = t;
        return *this;
    }
    constexpr edmonds_karp & reset() {
        _carried_flow_map.fill(0);
        return *this;
    }

private:
    bool find_unsaturated_path() {
        const auto & out_arcs_range = out_arcs(_graph, _s);
        prefetch_keys_and_values(out_arcs_range, arc_targets_map(_graph),
                                 _capacity_map, _carried_flow_map);
        _bfs_reached_map.fill(false);
        _bfs_reached_map[_s] = true;
        _bfs_queue.resize(0);
        _bfs_queue.push_back(_s);
        auto current = _bfs_queue.begin();
        while(current != _bfs_queue.end()) {
            const vertex & u = *current;
            for(auto && a : out_arcs(_graph, u)) {
                const vertex v = arc_target(_graph, a);
                if(_bfs_reached_map[v] ||
                   _capacity_map[a] == _carried_flow_map[a])
                    continue;
                _bfs_pred_arc[v] = a;
                _bfs_reached_map[v] = true;
                if(v == _t) return true;
                _bfs_queue.push_back(v);
            }
            for(auto && a : in_arcs(_graph, u)) {
                const vertex v = arc_source(_graph, a);
                if(_bfs_reached_map[v] || _carried_flow_map[a] == 0) continue;
                _bfs_pred_arc[v] = a;
                _bfs_reached_map[v] = true;
                if(v == _t) return true;
                _bfs_queue.push_back(v);
            }
            ++current;
        }
        return false;
    }

    void push_flow_on_found_path() {
        value_t pushed_flow = std::numeric_limits<value_t>::max();
        vertex v = _t;
        while(v != _s) {
            const arc a = _bfs_pred_arc[v];
            const vertex u = arc_source(_graph, a);
            if(v != u) {
                pushed_flow = std::min(pushed_flow,
                                       _capacity_map[a] - _carried_flow_map[a]);
                v = u;
            } else {
                pushed_flow = std::min(pushed_flow, _carried_flow_map[a]);
                v = arc_target(_graph, a);
            }
        }
        v = _t;
        while(v != _s) {
            const arc a = _bfs_pred_arc[v];
            const vertex u = arc_source(_graph, a);
            if(v != u) {
                _carried_flow_map[a] += pushed_flow;
                v = u;
            } else {
                _carried_flow_map[a] -= pushed_flow;
                v = arc_target(_graph, a);
            }
        }
    }

public:
    constexpr edmonds_karp & run() {
        while(find_unsaturated_path()) {
            push_flow_on_found_path();
        }
        return *this;
    }
    [[nodiscard]] constexpr value_t flow_value() const {
        value_t sum{0};
        for(auto && a : out_arcs(_graph, _s)) sum += _carried_flow_map[a];
        return sum;
    }
    [[nodiscard]] constexpr auto minimum_cut() const {
        if constexpr(std::ranges::viewable_range<out_arcs_range_t<Graph>>) {
            return std::views::join(std::views::transform(
                _bfs_queue, [this](const vertex_t<Graph> & v) {
                    return std::views::filter(
                        out_arcs(_graph, v), [this](const arc_t<Graph> & a) {
                            return !_bfs_reached_map[arc_target(_graph, a)];
                        });
                }));
        } else {
            return std::views::filter(
                arcs(_graph), [this](const arc_t<Graph> & a) {
                    return _bfs_reached_map[arc_source(_graph, a)] &&
                           !_bfs_reached_map[arc_target(_graph, a)];
                });
        }
    }
};

template <typename Graph, typename CapacityMap>
edmonds_karp(Graph &&,
             CapacityMap &&) -> edmonds_karp<views::graph_all_t<Graph>,
                                             maps::mapping_all_t<CapacityMap>>;

template <typename Graph, typename CapacityMap>
edmonds_karp(Graph &&, CapacityMap &&, const vertex_t<Graph> &,
             const vertex_t<Graph> &)
    -> edmonds_karp<views::graph_all_t<Graph>,
                    maps::mapping_all_t<CapacityMap>>;

}  // namespace melon
