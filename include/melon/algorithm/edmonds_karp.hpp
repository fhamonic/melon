#ifndef MELON_ALGORITHM_EDMONDS_KARP_HPP
#define MELON_ALGORITHM_EDMONDS_KARP_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"

namespace fhamonic {
namespace melon {

template <graph _Graph, input_mapping<arc_t<_Graph>> _CapacityMap>
    requires outward_incidence_graph<_Graph> &&
             inward_incidence_graph<_Graph> && has_vertex_map<_Graph> &&
             has_arc_map<_Graph>
class edmonds_karp {
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
    vertex_map_t<_Graph, bool> _bfs_reached_map;
    vertex_map_t<_Graph, arc> _bfs_pred_arc;

public:
    template <typename _G, typename _M>
    [[nodiscard]] constexpr edmonds_karp(_G && g, _M && c)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _capacity_map(views::mapping_all(std::forward<_M>(c)))
        , _carried_flow_map(create_arc_map<value_t>(_graph))
        , _bfs_reached_map(create_vertex_map<bool>(_graph))
        , _bfs_pred_arc(create_vertex_map<arc>(_graph)) {
        if constexpr(has_nb_vertices<_Graph>) {
            _bfs_queue.reserve(nb_vertices(_graph));
        }
        reset();
    }

    template <typename _G, typename _M>
    [[nodiscard]] constexpr edmonds_karp(_G && g, _M && c, const vertex & s,
                                         const vertex & t)
        : edmonds_karp(std::forward<_G>(g), std::forward<_M>(c)) {
        set_source(s);
        set_target(t);
    }

    [[nodiscard]] constexpr edmonds_karp(const edmonds_karp & bin) = default;
    [[nodiscard]] constexpr edmonds_karp(edmonds_karp && bin) = default;

    constexpr edmonds_karp & operator=(const edmonds_karp &) = default;
    constexpr edmonds_karp & operator=(edmonds_karp &&) = default;

    constexpr edmonds_karp & set_source(const vertex & s) noexcept {
        _s = s;
        return *this;
    }
    constexpr edmonds_karp & set_target(const vertex & t) noexcept {
        _t = t;
        return *this;
    }
    constexpr edmonds_karp & reset() noexcept {
        _carried_flow_map.fill(0);
        return *this;
    }

private:
    bool find_unsaturated_path() {
        const auto & out_arcs_range = out_arcs(_graph, _s);
        prefetch_range(out_arcs_range);
        prefetch_mapped_values(out_arcs_range, arc_targets_map(_graph));
        prefetch_mapped_values(out_arcs_range, _capacity_map);
        prefetch_mapped_values(out_arcs_range, _carried_flow_map);
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
    constexpr edmonds_karp & run() noexcept {
        while(find_unsaturated_path()) {
            push_flow_on_found_path();
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
                        out_arcs(_graph, v), [this](const arc_t<_Graph> & a) {
                            return !_bfs_reached_map[arc_target(_graph, a)];
                        });
                }));
        } else {
            return std::views::filter(
                arcs(_graph), [this](const arc_t<_Graph> & a) {
                    return _bfs_reached_map[arc_source(_graph)] &&
                           !_bfs_reached_map[arc_target(_graph)];
                });
        }
    }
};

template <typename _Graph, typename _LengthMap>
edmonds_karp(_Graph &&, _LengthMap &&)
    -> edmonds_karp<views::graph_all_t<_Graph>,
                    views::mapping_all_t<_LengthMap>>;

template <typename _Graph, typename _LengthMap>
edmonds_karp(_Graph &&, _LengthMap &&, const vertex_t<_Graph> &,
             const vertex_t<_Graph> &)
    -> edmonds_karp<views::graph_all_t<_Graph>,
                    views::mapping_all_t<_LengthMap>>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_EDMONDS_KARP_HPP
