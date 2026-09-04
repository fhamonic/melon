#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <ranges>
#include <utility>
#include <vector>

#include "melon/detail/fill.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"

namespace melon {

// numeric_limits must be genuinely specialized for the capacity type: the
// primary template's max() returns T{}, a zero infinity that makes the
// augmenting-path loop spin forever. Capacities must also be non-negative,
// which no concept can check: a negative one lets an augmentation exceed the
// capacity it is bounded by, so run() converges on a non-flow.
// O(n m^2), independent of the capacity values.
struct edmonds_karp_roles {
    struct flow {};
    struct bfs_reached {};
    struct bfs_pred_arc {};
};

template <graph_view Graph, mapping_view<arc_t<Graph>> CapacityMap>
    requires outward_incidence_graph<Graph> && inward_incidence_graph<Graph> &&
             has_vertex_map<Graph> && has_arc_map<Graph> &&
             std::numeric_limits<
                 mapped_value_t<CapacityMap, arc_t<Graph>>>::is_specialized
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
    // Unconditional, not `#ifndef NDEBUG`: melon is header-only, so a layout
    // that depends on NDEBUG differs between translation units of one program
    // -- an ODR violation no test and no sanitizer sees.
    bool _source_set;
    bool _target_set;
    bool _converged;
    arc_map_t<Graph, value_t, edmonds_karp_roles::flow> _carried_flow_map;
    std::vector<vertex> _bfs_queue;
    vertex_map_t<Graph, bool, edmonds_karp_roles::bfs_reached> _bfs_reached_map;
    vertex_map_t<Graph, arc, edmonds_karp_roles::bfs_pred_arc> _bfs_pred_arc;

public:
    // ---- Construction -------------------------------------------------------

    // Leaves the terminals unset -- run(), flow_value() and minimum_cut() all
    // read them, so set_source() and set_target() must be called first.
    template <graph_for<Graph> G, mapping_for<CapacityMap> CM>
    constexpr edmonds_karp(G && g, CM && cm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _capacity_map(maps::mapping_all(std::forward<CM>(cm)))
        , _source_set(false)
        , _target_set(false)
        , _converged(false)
        , _carried_flow_map(
              create_arc_map<value_t, edmonds_karp_roles::flow>(_graph))
        , _bfs_reached_map(
              create_vertex_map<bool, edmonds_karp_roles::bfs_reached>(_graph))
        , _bfs_pred_arc(
              create_vertex_map<arc, edmonds_karp_roles::bfs_pred_arc>(
                  _graph)) {
        if constexpr(has_num_vertices<Graph>) {
            _bfs_queue.reserve(num_vertices(_graph));
        }
        reset();
    }

    template <graph_for<Graph> G, mapping_for<CapacityMap> CM>
    constexpr edmonds_karp(G && g, CM && cm, const vertex & s, const vertex & t)
        : edmonds_karp(std::forward<G>(g), std::forward<CM>(cm)) {
        set_source(s);
        set_target(t);
    }

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    constexpr edmonds_karp(const edmonds_karp &) = delete;
    constexpr edmonds_karp(edmonds_karp &&) = default;

    constexpr edmonds_karp & operator=(const edmonds_karp &) = delete;
    constexpr edmonds_karp & operator=(edmonds_karp &&) = default;

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

    constexpr edmonds_karp & set_source(const vertex & s) {
        _s = s;
        _source_set = true;
        _converged = false;
        return *this;
    }
    constexpr edmonds_karp & set_target(const vertex & t) {
        _t = t;
        _target_set = true;
        _converged = false;
        return *this;
    }
    constexpr edmonds_karp & reset() {
        detail::fill(_carried_flow_map, arcs(_graph), value_t{0});
        _converged = false;
        return *this;
    }

private:
    bool find_unsaturated_path() {
        const auto & out_arcs_range = out_arcs(_graph, _s);
        detail::prefetch_keys_and_values(out_arcs_range,
                                         arc_targets_map(_graph), _capacity_map,
                                         _carried_flow_map);
        detail::fill(_bfs_reached_map, vertices(_graph), false);
        _bfs_reached_map[_s] = true;
        _bfs_queue.resize(0);
        _bfs_queue.push_back(_s);
        // The vertex is taken by value, not by reference: the callers pass
        // _bfs_queue elements, and the push_backs below reallocate _bfs_queue
        // mid-call in the no-reserve arm -- a reference parameter dangles
        // inside the running call.
        const auto visit = [this](const vertex u) {
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
            return false;
        };
        // The iterator walk is only legal because the constructor reserved
        // num_vertices slots, so visit's push_backs never reallocate under
        // `current`. Without num_vertices there is no reserve and a growing
        // queue relocates, hence the index arm.
        if constexpr(has_num_vertices<Graph>) {
            auto current = _bfs_queue.begin();
            while(current != _bfs_queue.end()) {
                if(visit(*current)) return true;
                ++current;
            }
        } else {
            for(std::size_t i = 0; i < _bfs_queue.size(); ++i) {
                if(visit(_bfs_queue[i])) return true;
            }
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
    // ---- Execution ----------------------------------------------------------

    constexpr edmonds_karp & run() {
        assert(_source_set && _target_set);
        assert(_s != _t);
        while(find_unsaturated_path()) {
            push_flow_on_found_path();
        }
        _converged = true;
        return *this;
    }

    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] constexpr value_t flow_value() const {
        assert(_source_set);
        value_t sum{0};
        for(auto && a : out_arcs(_graph, _s)) sum += _carried_flow_map[a];
        return sum;
    }

    // The flow carried by `a`: zero after reset(), part of a maximum flow
    // once run() has converged, and of a valid intermediate flow between the
    // two -- every augmentation preserves conservation.
    [[nodiscard]] constexpr value_t flow(const arc & a) const
        noexcept(noexcept(_carried_flow_map[a])) {
        return _carried_flow_map[a];
    }
    // Refers into the algorithm, like every melon map view: valid while this
    // object lives and stays put.
    [[nodiscard]] constexpr auto flows_map() const & noexcept(
        noexcept(maps::mapping_all(_carried_flow_map))) {
        return maps::mapping_all(_carried_flow_map);
    }
    // Terminal, like std::move(alg).base(): the member left behind is valid but
    // empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto flows_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_carried_flow_map)))) {
        return maps::mapping_all(std::move(_carried_flow_map));
    }

    // Precondition: run() has converged. It reads the reachability the
    // *final*, failed augmenting search left behind; before that the maps hold
    // either uninitialised memory or an intermediate residual reachability,
    // which is a cut of no particular graph.
    [[nodiscard]] constexpr auto minimum_cut() const {
        assert(_converged);
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
