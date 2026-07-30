#pragma once

#include <algorithm>
#include <cassert>
#include <limits>
#include <ranges>
#include <vector>

#include "melon/detail/borrowed_graph.hpp"
#include "melon/detail/consumable_view.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"

namespace melon {

// graph_view / mapping_view on the stored members: see dijkstra's head.
template <graph_view Graph, mapping_view<arc_t<Graph>> CapacityMap>
    requires outward_incidence_graph<Graph> &&
             inward_incidence_graph<Graph> && has_vertex_map<Graph> &&
             has_arc_map<Graph>
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
    arc_map_t<Graph, value_t> _carried_flow_map;
    std::vector<vertex> _bfs_queue;
    vertex_map_t<Graph, std::size_t> _vertex_rank_map;
    vertex_map_t<Graph, consumable_input_view_t<out_arcs_range_t<Graph>>>
        _remaining_out_arcs;
    vertex_map_t<Graph, consumable_input_view_t<in_arcs_range_t<Graph>>>
        _remaining_in_arcs;

public:
    // graph_storable_as / mapping_storable_as, so std::is_constructible
    // answers what the mem-initializers actually do -- see dijkstra's
    // constructor.
    template <typename G, typename M>
        requires graph_storable_as<G, Graph> &&
                 mapping_storable_as<M, CapacityMap>
    constexpr dinitz(G && g, M && c)
        : _graph(detail::store_graph<Graph>(std::forward<G>(g)))
        , _capacity_map(detail::store_mapping<CapacityMap>(std::forward<M>(c)))
        , _carried_flow_map(create_arc_map<value_t>(_graph))
        , _vertex_rank_map(create_vertex_map<std::size_t>(_graph))
        , _remaining_out_arcs(
              create_vertex_map<
                  consumable_input_view_t<out_arcs_range_t<Graph>>>(_graph))
        , _remaining_in_arcs(
              create_vertex_map<
                  consumable_input_view_t<in_arcs_range_t<Graph>>>(_graph)) {
        if constexpr(has_num_vertices<Graph>) {
            _bfs_queue.reserve(num_vertices(_graph));
        }
        reset();
    }

    template <typename G, typename M>
        requires graph_storable_as<G, Graph> &&
                 mapping_storable_as<M, CapacityMap>
    constexpr dinitz(G && g, M && c, const vertex & s, const vertex & t)
        : dinitz(std::forward<G>(g), std::forward<M>(c)) {
        set_source(s);
        set_target(t);
    }

    // The relocation policy is depth_first_search's: the per-vertex cursors
    // are re-askable -- out_arcs(_graph, v) / in_arcs(_graph, v) for exactly
    // the vertex they are stored under -- so after the members relocate,
    // _rebase_cursors() aims them at the *new* _graph and the _consumed
    // counters put them back where they were. Copy therefore no longer
    // requires borrowed_graph: rebasing is what makes a dinitz over an owned
    // subgraph honestly copyable. The one combination that cannot rebase -- a
    // non-borrowed graph handing out std-borrowed ranges, which have no
    // counter -- keeps copy constrained away.
    constexpr dinitz(const dinitz & o)
        requires std::copy_constructible<Graph> &&
                     std::copy_constructible<CapacityMap> &&
                     std::copy_constructible<
                         consumable_input_view_t<out_arcs_range_t<Graph>>> &&
                     std::copy_constructible<
                         consumable_input_view_t<in_arcs_range_t<Graph>>> &&
                     (borrowed_graph<Graph> ||
                      (!std::ranges::borrowed_range<out_arcs_range_t<Graph>> &&
                       !std::ranges::borrowed_range<in_arcs_range_t<Graph>>))
        : _graph(o._graph)
        , _capacity_map(o._capacity_map)
        , _s(o._s)
        , _t(o._t)
        , _carried_flow_map(o._carried_flow_map)
        , _bfs_queue(o._bfs_queue)
        , _vertex_rank_map(o._vertex_rank_map)
        , _remaining_out_arcs(o._remaining_out_arcs)
        , _remaining_in_arcs(o._remaining_in_arcs) {
        _rebase_cursors();
    }
    // Hand-written for the same reason as depth_first_search's move: a
    // memberwise move leaves the cached cursors' ranges pointing at the
    // moved-from object's _graph member.
    constexpr dinitz(dinitz && o)
        : _graph(std::move(o._graph))
        , _capacity_map(std::move(o._capacity_map))
        , _s(std::move(o._s))
        , _t(std::move(o._t))
        , _carried_flow_map(std::move(o._carried_flow_map))
        , _bfs_queue(std::move(o._bfs_queue))
        , _vertex_rank_map(std::move(o._vertex_rank_map))
        , _remaining_out_arcs(std::move(o._remaining_out_arcs))
        , _remaining_in_arcs(std::move(o._remaining_in_arcs)) {
        _rebase_cursors();
    }

    constexpr dinitz & operator=(const dinitz & o)
        requires std::copyable<Graph> && std::copyable<CapacityMap> &&
                     std::copyable<
                         consumable_input_view_t<out_arcs_range_t<Graph>>> &&
                     std::copyable<
                         consumable_input_view_t<in_arcs_range_t<Graph>>> &&
                     (borrowed_graph<Graph> ||
                      (!std::ranges::borrowed_range<out_arcs_range_t<Graph>> &&
                       !std::ranges::borrowed_range<in_arcs_range_t<Graph>>))
    {
        if(this == std::addressof(o)) return *this;
        _graph = o._graph;
        _capacity_map = o._capacity_map;
        _s = o._s;
        _t = o._t;
        _carried_flow_map = o._carried_flow_map;
        _bfs_queue = o._bfs_queue;
        _vertex_rank_map = o._vertex_rank_map;
        _remaining_out_arcs = o._remaining_out_arcs;
        _remaining_in_arcs = o._remaining_in_arcs;
        _rebase_cursors();
        return *this;
    }
    // See the move constructor.
    constexpr dinitz & operator=(dinitz && o) {
        if(this == std::addressof(o)) return *this;
        _graph = std::move(o._graph);
        _capacity_map = std::move(o._capacity_map);
        _s = std::move(o._s);
        _t = std::move(o._t);
        _carried_flow_map = std::move(o._carried_flow_map);
        _bfs_queue = std::move(o._bfs_queue);
        _vertex_rank_map = std::move(o._vertex_rank_map);
        _remaining_out_arcs = std::move(o._remaining_out_arcs);
        _remaining_in_arcs = std::move(o._remaining_in_arcs);
        _rebase_cursors();
        return *this;
    }

private:
    // See depth_first_search::_rebase_stack. Every vertex's cursor is
    // rebased, including ones the current phase never seeded: rebasing a
    // default-constructed cursor merely re-points it at a fresh range, and
    // bfs_rank_vertices() re-seeds every cursor it will read anyway.
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

    constexpr dinitz & set_source(const vertex & s) {
        _s = s;
        return *this;
    }

    constexpr dinitz & set_target(const vertex & t) {
        _t = t;
        return *this;
    }

    constexpr dinitz & reset() {
        _carried_flow_map.fill(0);
        for(auto && u : vertices(_graph)) {
            _remaining_out_arcs[u] = out_arcs(_graph, u);
            _remaining_in_arcs[u] = in_arcs(_graph, u);
        }
        return *this;
    }

private:
    bool bfs_rank_vertices() {
        _vertex_rank_map.fill(std::numeric_limits<std::size_t>::max());
        _vertex_rank_map[_t] = 0;
        _bfs_queue.resize(0);
        _bfs_queue.push_back(_t);
        auto current = _bfs_queue.begin();
        while(current != _bfs_queue.end()) {
            const vertex & u = *current;
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
            ++current;
        }
        return _vertex_rank_map[_s] != std::numeric_limits<std::size_t>::max();
    }

    value_t dfs_push_flow(const vertex u, const value_t max_incomming_flow) {
        if(max_incomming_flow == 0 || u == _t) return max_incomming_flow;
        for(; !_remaining_out_arcs[u].empty();
            _remaining_out_arcs[u].advance()) {
            const arc & a = _remaining_out_arcs[u].current();
            const vertex v = arc_target(_graph, a);
            if(_vertex_rank_map[v] + 1 != _vertex_rank_map[u]) continue;
            if(_capacity_map[a] == _carried_flow_map[a]) continue;
            const value_t pushed_flow = dfs_push_flow(
                v, std::min(max_incomming_flow,
                            _capacity_map[a] - _carried_flow_map[a]));
            if(pushed_flow == 0) continue;
            _carried_flow_map[a] += pushed_flow;
            return pushed_flow;
        }
        for(; !_remaining_in_arcs[u].empty(); _remaining_in_arcs[u].advance()) {
            const arc & a = _remaining_in_arcs[u].current();
            const vertex v = arc_source(_graph, a);
            if(_vertex_rank_map[v] + 1 != _vertex_rank_map[u]) continue;
            if(_carried_flow_map[a] == 0) continue;
            const value_t pushed_flow = dfs_push_flow(
                v, std::min(max_incomming_flow, _carried_flow_map[a]));
            if(pushed_flow == 0) continue;
            _carried_flow_map[a] -= pushed_flow;
            return pushed_flow;
        }
        return value_t{0};
    }

public:
    constexpr dinitz & run() {
        while(bfs_rank_vertices()) {
            for(auto && u : vertices(_graph)) {
                _remaining_out_arcs[u] = out_arcs(_graph, u);
                _remaining_in_arcs[u] = in_arcs(_graph, u);
            }
            while(dfs_push_flow(_s, std::numeric_limits<value_t>::max()) >
                  value_t{0});
        }
        return *this;
    }

    [[nodiscard]] constexpr value_t flow_value() const {
        value_t sum{0};
        for(auto && a : out_arcs(_graph, _s)) sum += _carried_flow_map[a];
        return sum;
    }

    [[nodiscard]] constexpr auto minimum_cut() const {
        // in_arcs, not out_arcs: the branch has to test the range it then
        // builds the view over. edmonds_karp tests and uses out_arcs.
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
