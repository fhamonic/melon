#ifndef MELON_ALGORITHM_DINITZ_HPP
#define MELON_ALGORITHM_DINITZ_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

#include "melon/graph.hpp"
#include "melon/utility/value_map.hpp"

namespace fhamonic {
namespace melon {

template <graph G, input_value_map<arc_t<G>> C>
    requires outward_incidence_graph<G> && inward_incidence_graph<G> &&
             has_vertex_map<G> && has_arc_map<G>
class dinitz {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;
    using value_t = mapped_value_t<C, arc_t<G>>;

private:
    std::reference_wrapper<const G> _graph;
    std::reference_wrapper<const C> _capacity_map;

    vertex _s;
    vertex _t;
    arc_map_t<G, value_t> _carried_flow_map;

    std::vector<vertex> _vertices_buffer;
    vertex_map_t<G, std::size_t> _vertex_level_map;
    vertex_map_t<G, bool> _dfs_reached_map;
    vertex_map_t<G, arc> _dfs_pred_arc_map;
    arc_map_t<G, bool> _level_graph_forward;
    arc_map_t<G, bool> _level_graph_backward;

public:
    [[nodiscard]] constexpr dinitz(const G & g, const C & c)
        : _graph(g)
        , _capacity_map(c)
        , _carried_flow_map(create_arc_map<value_t>(g))
        , _vertex_level_map(create_vertex_map<std::size_t>(g))
        , _dfs_reached_map(create_vertex_map<bool>(g))
        , _dfs_pred_arc_map(create_vertex_map<arc>(g))
        , _level_graph_forward(create_arc_map<bool>(g))
        , _level_graph_backward(create_arc_map<bool>(g)) {
        _vertices_buffer.reserve(g.nb_vertices());
        reset();
    }

    [[nodiscard]] constexpr dinitz(const G & g, const C & c, const vertex & s,
                                   const vertex & t)
        : dinitz(g, c) {
        set_source(s);
        set_target(t);
    }

    [[nodiscard]] constexpr dinitz(const dinitz & bin) = default;
    [[nodiscard]] constexpr dinitz(dinitz && bin) = default;

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
        return *this;
    }

private:
    bool find_level_graph() {
        _vertex_level_map.fill(std::numeric_limits<std::size_t>::max());
        _vertex_level_map[_s] = 0;
        _vertices_buffer.resize(0);
        _vertices_buffer.push_back(_s);
        auto current = _vertices_buffer.begin();
        while(current != _vertices_buffer.end()) {
            const vertex & u = *current;
            for(auto && a : out_arcs(_graph.get(), u)) {
                if(_capacity_map.get()[a] == _carried_flow_map[a]) {
                    _level_graph_forward[a] = false;
                    continue;
                }
                const vertex v = arc_target(_graph.get(), a);
                if(_vertex_level_map[v] !=
                   std::numeric_limits<std::size_t>::max()) {
                    _level_graph_forward[a] =
                        (_vertex_level_map[v] == _vertex_level_map[u] + 1);
                    continue;
                }
                _vertex_level_map[v] = _vertex_level_map[u] + 1;
                _level_graph_forward[a] = true;
                _vertices_buffer.push_back(v);
            }
            for(auto && a : in_arcs(_graph.get(), u)) {
                if(_carried_flow_map[a] == 0) {
                    _level_graph_backward[a] = false;
                    continue;
                }
                const vertex v = arc_source(_graph.get(), a);
                if(_vertex_level_map[v] !=
                   std::numeric_limits<std::size_t>::max()) {
                    _level_graph_backward[a] =
                        (_vertex_level_map[v] == _vertex_level_map[u] + 1);
                    continue;
                }
                _vertex_level_map[v] = _vertex_level_map[u] + 1;
                _level_graph_backward[a] = true;
                _vertices_buffer.push_back(v);
            }
            ++current;
        }
        return _vertex_level_map[_t] != std::numeric_limits<std::size_t>::max();
    }

    bool find_unsaturated_path() {
        _dfs_reached_map.fill(false);
        _dfs_reached_map[_s] = true;
        _vertices_buffer.resize(0);
        _vertices_buffer.push_back(_s);
        while(_vertices_buffer.size() > 0) {
            const vertex u = _vertices_buffer.back();
            _vertices_buffer.pop_back();
            for(auto && a : out_arcs(_graph.get(), u)) {
                if(!_level_graph_forward[a]) continue;
                const vertex v = arc_target(_graph.get(), a);
                if(_dfs_reached_map[v]) continue;
                _dfs_reached_map[v] = true;
                _dfs_pred_arc_map[v] = a;
                if(v == _t) return true;
                _vertices_buffer.push_back(v);
            }
            for(auto && a : in_arcs(_graph.get(), u)) {
                if(!_level_graph_backward[a]) continue;
                const vertex v = arc_source(_graph.get(), a);
                if(_dfs_reached_map[v]) continue;
                _dfs_reached_map[v] = true;
                _dfs_pred_arc_map[v] = a;
                if(v == _t) return true;
                _vertices_buffer.push_back(v);
            }
        }
        return false;
    }

    void push_flow_on_found_path() {
        value_t pushed_flow = std::numeric_limits<value_t>::max();
        vertex v = _t;
        while(v != _s) {
            const arc a = _dfs_pred_arc_map[v];
            if(_level_graph_forward[a]) {
                pushed_flow = std::min(
                    pushed_flow, _capacity_map.get()[a] - _carried_flow_map[a]);
                v = arc_source(_graph.get(), a);
            } else {
                pushed_flow = std::min(pushed_flow, _carried_flow_map[a]);
                v = arc_target(_graph.get(), a);
            }
        }
        v = _t;
        while(v != _s) {
            const arc a = _dfs_pred_arc_map[v];
            if(_level_graph_forward[a]) {
                _carried_flow_map[a] += pushed_flow;
                _level_graph_forward[a] =
                    (_carried_flow_map[a] < _capacity_map.get()[a]);
                v = arc_source(_graph.get(), a);
            } else {
                _carried_flow_map[a] -= pushed_flow;
                _level_graph_backward[a] = (_carried_flow_map[a] > 0);
                v = arc_target(_graph.get(), a);
            }
        }
    }

public:
    constexpr dinitz & run() noexcept {
        while(find_level_graph()) {
            while(find_unsaturated_path()) {
                push_flow_on_found_path();
            }
        }
        return *this;
    }
    constexpr value_t flow_value() noexcept {
        value_t sum{0};
        for(auto && a : out_arcs(_graph.get(), _s)) sum += _carried_flow_map[a];
        return sum;
    }
    constexpr auto minimum_cut() noexcept {
        if constexpr(std::ranges::viewable_range<out_arcs_range_t<G>>) {
            return std::views::join(std::views::transform(
                _vertices_buffer, [this](const vertex_t<G> & v) {
                    return std::views::filter(
                        out_arcs(_graph.get(), v), [this](const arc_t<G> & a) {
                            return !_dfs_reached_map[arc_target(_graph.get(),
                                                                a)];
                        });
                }));
        } else {
            return std::views::filter(
                arcs(_graph.get()), [this](const arc_t<G> & a) {
                    return _dfs_reached_map[arc_source(_graph.get())] &&
                           !_dfs_reached_map[arc_target(_graph.get())];
                });
        }
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_DINITZ_HPP
