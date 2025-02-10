#ifndef MELON_ALGORITHM_DINITZ_HPP
#define MELON_ALGORITHM_DINITZ_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

#include "melon/detail/consumable_view.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"

namespace fhamonic {
namespace melon {

template <graph _Graph, input_mapping<arc_t<_Graph>> _CapacityMap>
    requires outward_incidence_graph<_Graph> &&
             inward_incidence_graph<_Graph> && has_vertex_map<_Graph> &&
             has_arc_map<_Graph>
class dinitz {
private:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;
    using value_t = mapped_value_t<_CapacityMap, arc_t<_Graph>>;

private:
    _Graph _graph;
    _CapacityMap _capacity_map;
    vertex _s;
    vertex _t;
    arc_map_t<_Graph, value_t> _carried_flow_map;
    std::vector<vertex> _bfs_queue;
    vertex_map_t<_Graph, std::size_t> _vertex_rank_map;
    vertex_map_t<_Graph, consumable_view_t<out_arcs_range_t<_Graph>>>
        _remaining_out_arcs;
    vertex_map_t<_Graph, consumable_view_t<in_arcs_range_t<_Graph>>>
        _remaining_in_arcs;

public:
    template <typename _G, typename _M>
    [[nodiscard]] constexpr dinitz(_G && g, _M && c)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _capacity_map(views::mapping_all(std::forward<_M>(c)))
        , _carried_flow_map(create_arc_map<value_t>(_graph))
        , _vertex_rank_map(create_vertex_map<std::size_t>(_graph))
        , _remaining_out_arcs(
              create_vertex_map<consumable_view_t<out_arcs_range_t<_Graph>>>(
                  _graph))
        , _remaining_in_arcs(
              create_vertex_map<consumable_view_t<in_arcs_range_t<_Graph>>>(
                  _graph)) {
        if constexpr(has_num_vertices<_Graph>) {
            _bfs_queue.reserve(num_vertices(_graph));
        }
        reset();
    }

    template <typename _G, typename _M>
    [[nodiscard]] constexpr dinitz(_G && g, _M && c, const vertex & s,
                                   const vertex & t)
        : dinitz(std::forward<_G>(g), std::forward<_M>(c)) {
        set_source(s);
        set_target(t);
    }

    [[nodiscard]] constexpr dinitz(const dinitz &) = default;
    [[nodiscard]] constexpr dinitz(dinitz &&) = default;

    constexpr dinitz & operator=(const dinitz &) = default;
    constexpr dinitz & operator=(dinitz &&) = default;

    constexpr dinitz & set_source(const vertex & s) noexcept {
        _s = s;
        return *this;
    }

    constexpr dinitz & set_target(const vertex & t) noexcept {
        _t = t;
        return *this;
    }

    constexpr dinitz & reset() noexcept {
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
    constexpr dinitz & run() noexcept {
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

    constexpr value_t flow_value() noexcept {
        value_t sum{0};
        for(auto && a : out_arcs(_graph, _s)) sum += _carried_flow_map[a];
        return sum;
    }

    constexpr auto minimum_cut() noexcept {
        if constexpr(std::ranges::viewable_range<out_arcs_range_t<_Graph>>) {
            return std::views::join(std::views::transform(
                _bfs_queue, [this](const vertex_t<_Graph> & v) {
                    return std::views::filter(
                        in_arcs(_graph, v), [this](const arc_t<_Graph> & a) {
                            return _vertex_rank_map[arc_source(_graph, a)] ==
                                   std::numeric_limits<std::size_t>::max();
                        });
                }));
        } else {
            return std::views::filter(
                arcs(_graph), [this](const arc_t<_Graph> & a) {
                    return _vertex_rank_map[arc_source(_graph, a)] ==
                               std::numeric_limits<std::size_t>::max() &&
                           _vertex_rank_map[arc_target(_graph, a)] !=
                               std::numeric_limits<std::size_t>::max();
                });
        }
    }
};

template <typename _Graph, typename _LengthMap>
dinitz(_Graph &&, _LengthMap &&)
    -> dinitz<views::graph_all_t<_Graph>, views::mapping_all_t<_LengthMap>>;

template <typename _Graph, typename _LengthMap>
dinitz(_Graph &&, _LengthMap &&, const vertex_t<_Graph> &,
       const vertex_t<_Graph> &)
    -> dinitz<views::graph_all_t<_Graph>, views::mapping_all_t<_LengthMap>>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_DINITZ_HPP
