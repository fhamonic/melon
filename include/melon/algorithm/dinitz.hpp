#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include "melon/detail/consumable_view.hpp"
#include "melon/detail/fill.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"

namespace melon {

// numeric_limits must be genuinely specialized for the capacity type: the
// primary template's max() returns T{}, a zero infinity that makes the
// blocking-flow search spin forever. Capacities must also be non-negative,
// which no concept can check: a negative one lets an augmentation exceed the
// capacity it is bounded by, so run() converges on a non-flow.
// O(n^2 m), independent of the capacity values.
struct dinitz_roles {
    struct flow {};
    struct rank {};
    struct remaining_out_arcs {};
    struct remaining_in_arcs {};
};

template <graph_view Graph, mapping_view<arc_t<Graph>> CapacityMap>
    requires outward_incidence_graph<Graph> && inward_incidence_graph<Graph> &&
             has_vertex_map<Graph> && has_arc_map<Graph> &&
             std::numeric_limits<
                 mapped_value_t<CapacityMap, arc_t<Graph>>>::is_specialized
class dinitz {
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
    arc_map_t<Graph, value_t, dinitz_roles::flow> _carried_flow_map;
    std::vector<vertex> _bfs_queue;
    struct dfs_step {
        arc a;
        value_t entry_limit;
        bool forward;
    };
    std::vector<dfs_step> _dfs_path;
    vertex_map_t<Graph, std::size_t, dinitz_roles::rank> _vertex_rank_map;
    vertex_map_t<Graph,
                 detail::consumable_input_view_t<out_arcs_range_t<Graph>>,
                 dinitz_roles::remaining_out_arcs>
        _remaining_out_arcs;
    vertex_map_t<Graph, detail::consumable_input_view_t<in_arcs_range_t<Graph>>,
                 dinitz_roles::remaining_in_arcs>
        _remaining_in_arcs;

public:
    // ---- Construction -------------------------------------------------------

    // Leaves the terminals unset -- run(), flow_value() and minimum_cut() all
    // read them, so set_source() and set_target() must be called first.
    template <graph_for<Graph> G, mapping_for<CapacityMap> CM>
    constexpr dinitz(G && g, CM && cm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _capacity_map(maps::mapping_all(std::forward<CM>(cm)))
        , _source_set(false)
        , _target_set(false)
        , _converged(false)
        , _carried_flow_map(create_arc_map<value_t, dinitz_roles::flow>(_graph))
        , _vertex_rank_map(
              create_vertex_map<std::size_t, dinitz_roles::rank>(_graph))
        , _remaining_out_arcs(
              create_vertex_map<
                  detail::consumable_input_view_t<out_arcs_range_t<Graph>>,
                  dinitz_roles::remaining_out_arcs>(_graph))
        , _remaining_in_arcs(
              create_vertex_map<
                  detail::consumable_input_view_t<in_arcs_range_t<Graph>>,
                  dinitz_roles::remaining_in_arcs>(_graph)) {
        if constexpr(has_num_vertices<Graph>) {
            _bfs_queue.reserve(num_vertices(_graph));
        }
        reset();
    }

    template <graph_for<Graph> G, mapping_for<CapacityMap> CM>
    constexpr dinitz(G && g, CM && cm, const vertex & s, const vertex & t)
        : dinitz(std::forward<G>(g), std::forward<CM>(cm)) {
        set_source(s);
        set_target(t);
    }

    // Move-only; see the melon::traversal_algorithm concept.
    // Hand-written because a memberwise move leaves the cached cursors' ranges
    // pointing at the moved-from object's _graph member.
    constexpr dinitz(const dinitz &) = delete;
    constexpr dinitz(dinitz && o)
        : _graph(std::move(o._graph))
        , _capacity_map(std::move(o._capacity_map))
        , _s(std::move(o._s))
        , _t(std::move(o._t))
        , _source_set(o._source_set)
        , _target_set(o._target_set)
        , _converged(o._converged)
        , _carried_flow_map(std::move(o._carried_flow_map))
        , _bfs_queue(std::move(o._bfs_queue))
        , _dfs_path(std::move(o._dfs_path))
        , _vertex_rank_map(std::move(o._vertex_rank_map))
        , _remaining_out_arcs(std::move(o._remaining_out_arcs))
        , _remaining_in_arcs(std::move(o._remaining_in_arcs)) {
        _rebase_cursors();
    }

    constexpr dinitz & operator=(const dinitz &) = delete;
    constexpr dinitz & operator=(dinitz && o) {
        if(this == std::addressof(o)) return *this;
        _graph = std::move(o._graph);
        _capacity_map = std::move(o._capacity_map);
        _s = std::move(o._s);
        _t = std::move(o._t);
        _source_set = o._source_set;
        _target_set = o._target_set;
        _converged = o._converged;
        _carried_flow_map = std::move(o._carried_flow_map);
        _bfs_queue = std::move(o._bfs_queue);
        _dfs_path = std::move(o._dfs_path);
        _vertex_rank_map = std::move(o._vertex_rank_map);
        _remaining_out_arcs = std::move(o._remaining_out_arcs);
        _remaining_in_arcs = std::move(o._remaining_in_arcs);
        _rebase_cursors();
        return *this;
    }

private:
    constexpr void _rebase_cursors() {
        if constexpr(!borrowed_graph<Graph>) {
            if constexpr(!std::ranges::borrowed_range<out_arcs_range_t<Graph>>)
                for(const vertex & v : vertices(_graph))
                    _remaining_out_arcs[v].rebase(out_arcs(_graph, v));
            if constexpr(!std::ranges::borrowed_range<in_arcs_range_t<Graph>>)
                for(const vertex & v : vertices(_graph))
                    _remaining_in_arcs[v].rebase(in_arcs(_graph, v));
        }
    }

public:
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

    constexpr dinitz & set_source(const vertex & s) {
        _s = s;
        _source_set = true;
        _converged = false;
        return *this;
    }

    constexpr dinitz & set_target(const vertex & t) {
        _t = t;
        _target_set = true;
        _converged = false;
        return *this;
    }

    constexpr dinitz & reset() {
        _converged = false;
        detail::fill(_carried_flow_map, arcs(_graph), value_t{0});
        for(auto && u : vertices(_graph)) {
            _remaining_out_arcs[u] = out_arcs(_graph, u);
            _remaining_in_arcs[u] = in_arcs(_graph, u);
        }
        return *this;
    }

private:
    bool bfs_rank_vertices() {
        detail::fill(_vertex_rank_map, vertices(_graph),
                     std::numeric_limits<std::size_t>::max());
        _vertex_rank_map[_t] = 0;
        _bfs_queue.resize(0);
        _bfs_queue.push_back(_t);
        // The vertex is taken by value, not by reference: the callers pass
        // _bfs_queue elements, and the push_backs below reallocate _bfs_queue
        // mid-call in the no-reserve arm -- a reference parameter dangles
        // inside the running call.
        const auto visit = [this](const vertex u) {
            for(auto && a : in_arcs(_graph, u)) {
                const vertex v = arc_source(_graph, a);
                if(_vertex_rank_map[v] !=
                       std::numeric_limits<std::size_t>::max() ||
                   _capacity_map[a] == _carried_flow_map[a])
                    continue;
                _vertex_rank_map[v] = _vertex_rank_map[u] + 1;
                _bfs_queue.push_back(v);
            }
            for(auto && a : out_arcs(_graph, u)) {
                const vertex v = arc_target(_graph, a);
                if(_vertex_rank_map[v] !=
                       std::numeric_limits<std::size_t>::max() ||
                   _carried_flow_map[a] == 0)
                    continue;
                _vertex_rank_map[v] = _vertex_rank_map[u] + 1;
                _bfs_queue.push_back(v);
            }
        };
        // The iterator walk is only legal because the constructor reserved
        // num_vertices slots, so visit's push_backs never reallocate under
        // `current`. Without num_vertices there is no reserve and a growing
        // queue relocates, hence the index arm.
        if constexpr(has_num_vertices<Graph>) {
            auto current = _bfs_queue.begin();
            while(current != _bfs_queue.end()) {
                visit(*current);
                ++current;
            }
        } else {
            for(std::size_t i = 0; i < _bfs_queue.size(); ++i) {
                visit(_bfs_queue[i]);
            }
        }
        return _vertex_rank_map[_s] != std::numeric_limits<std::size_t>::max();
    }

    // An explicit path stack, not recursion: one call frame per level-graph
    // level would put the whole augmenting path on the call stack, which
    // overflows on graphs carrying very long paths. The per-vertex cursors
    // are the rest of the frame state and already live in the maps: on a
    // successful push every cursor on the path stays put, on a dead end the
    // parent's cursor advances past the arc that led there -- exactly the
    // recursion's continue/return split.
    value_t dfs_push_flow(const vertex source, value_t limit) {
        if(limit == value_t{0}) return limit;
        _dfs_path.resize(0);
        vertex u = source;
        for(;;) {
            if(u == _t) {
                for(const dfs_step & step : _dfs_path) {
                    if(step.forward)
                        _carried_flow_map[step.a] += limit;
                    else
                        _carried_flow_map[step.a] -= limit;
                }
                return limit;
            }
            bool descended = false;
            for(; !_remaining_out_arcs[u].empty();
                _remaining_out_arcs[u].advance()) {
                const arc & a = _remaining_out_arcs[u].current();
                const vertex v = arc_target(_graph, a);
                if(_vertex_rank_map[v] + 1 != _vertex_rank_map[u]) continue;
                if(_capacity_map[a] == _carried_flow_map[a]) continue;
                _dfs_path.push_back({a, limit, true});
                limit =
                    std::min(limit, _capacity_map[a] - _carried_flow_map[a]);
                u = v;
                descended = true;
                break;
            }
            if(descended) continue;
            for(; !_remaining_in_arcs[u].empty();
                _remaining_in_arcs[u].advance()) {
                const arc & a = _remaining_in_arcs[u].current();
                const vertex v = arc_source(_graph, a);
                if(_vertex_rank_map[v] + 1 != _vertex_rank_map[u]) continue;
                if(_carried_flow_map[a] == value_t{0}) continue;
                _dfs_path.push_back({a, limit, false});
                limit = std::min(limit, _carried_flow_map[a]);
                u = v;
                descended = true;
                break;
            }
            if(descended) continue;
            if(_dfs_path.empty()) return value_t{0};
            const dfs_step step = _dfs_path.back();
            _dfs_path.pop_back();
            limit = step.entry_limit;
            if(step.forward) {
                u = arc_source(_graph, step.a);
                _remaining_out_arcs[u].advance();
            } else {
                u = arc_target(_graph, step.a);
                _remaining_in_arcs[u].advance();
            }
        }
    }

public:
    // ---- Execution ----------------------------------------------------------

    constexpr dinitz & run() {
        assert(_source_set && _target_set);
        assert(_s != _t);
        while(bfs_rank_vertices()) {
            for(auto && u : vertices(_graph)) {
                _remaining_out_arcs[u] = out_arcs(_graph, u);
                _remaining_in_arcs[u] = in_arcs(_graph, u);
            }
            while(dfs_push_flow(_s, std::numeric_limits<value_t>::max()) >
                  value_t{0});
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
    // two -- every blocking-flow phase preserves conservation.
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

    // Precondition: run() has converged. It reads the ranks the *final*,
    // failed BFS left behind; before that the map holds either uninitialised
    // memory or an intermediate residual ranking, which is a cut of no
    // particular graph.
    [[nodiscard]] constexpr auto minimum_cut() const {
        assert(_converged);
        if constexpr(std::ranges::viewable_range<in_arcs_range_t<Graph>>) {
            return std::views::join(std::views::transform(
                _bfs_queue, [this](const vertex_t<Graph> & v) {
                    return std::views::filter(
                        in_arcs(_graph, v), [this](const arc_t<Graph> & a) {
                            return _vertex_rank_map[arc_source(_graph, a)] ==
                                   std::numeric_limits<std::size_t>::max();
                        });
                }));
        } else {
            return std::views::filter(
                arcs(_graph), [this](const arc_t<Graph> & a) {
                    return _vertex_rank_map[arc_source(_graph, a)] ==
                               std::numeric_limits<std::size_t>::max() &&
                           _vertex_rank_map[arc_target(_graph, a)] !=
                               std::numeric_limits<std::size_t>::max();
                });
        }
    }
};

template <typename Graph, typename CapacityMap>
dinitz(Graph &&, CapacityMap &&)
    -> dinitz<views::graph_all_t<Graph>, maps::mapping_all_t<CapacityMap>>;

template <typename Graph, typename CapacityMap>
dinitz(Graph &&, CapacityMap &&, const vertex_t<Graph> &,
       const vertex_t<Graph> &)
    -> dinitz<views::graph_all_t<Graph>, maps::mapping_all_t<CapacityMap>>;

}  // namespace melon
