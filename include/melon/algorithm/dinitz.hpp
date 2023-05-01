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

    std::vector<vertex> _bfs_queue;
    vertex_map_t<G, std::size_t> _vertex_rank_map;

public:
    [[nodiscard]] constexpr dinitz(const G & g, const C & c)
        : _graph(g)
        , _capacity_map(c)
        , _carried_flow_map(create_arc_map<value_t>(g))
        , _vertex_rank_map(create_vertex_map<std::size_t>(g)) {
        _bfs_queue.reserve(g.nb_vertices());
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
    bool bfs_rank_vertices() {
        _vertex_rank_map.fill(std::numeric_limits<std::size_t>::max());
        _vertex_rank_map[_s] = 0;
        _bfs_queue.resize(0);
        _bfs_queue.push_back(_s);
        auto current = _bfs_queue.begin();
        while(current != _bfs_queue.end()) {
            const vertex & u = *current;
            for(auto && a : out_arcs(_graph.get(), u)) {
                if(_capacity_map.get()[a] == _carried_flow_map[a]) continue;
                const vertex v = arc_target(_graph.get(), a);
                if(_vertex_rank_map[v] !=
                   std::numeric_limits<std::size_t>::max())
                    continue;
                _vertex_rank_map[v] = _vertex_rank_map[u] + 1;
                _bfs_queue.push_back(v);
            }
            for(auto && a : in_arcs(_graph.get(), u)) {
                if(_carried_flow_map[a] == 0) continue;
                const vertex v = arc_source(_graph.get(), a);
                if(_vertex_rank_map[v] !=
                   std::numeric_limits<std::size_t>::max())
                    continue;
                _vertex_rank_map[v] = _vertex_rank_map[u] + 1;
                _bfs_queue.push_back(v);
            }
            ++current;
        }
        return _vertex_rank_map[_t] != std::numeric_limits<std::size_t>::max();
    }

    value_t dfs_push_flow(const vertex u, const value_t max_incomming_flow) {
        if(u == _t) return max_incomming_flow;
        value_t total_pushed_flow = 0;
        for(auto && a : out_arcs(_graph.get(), u)) {
            const vertex v = arc_target(_graph.get(), a);
            if(_vertex_rank_map[v] != _vertex_rank_map[u] + 1) continue;
            if(_capacity_map.get()[a] == _carried_flow_map[a]) continue;
            value_t pushed_flow = dfs_push_flow(
                v, std::min(max_incomming_flow,
                            _capacity_map.get()[a] - _carried_flow_map[a]));
            _carried_flow_map[a] += pushed_flow;
            total_pushed_flow += pushed_flow;
        }
        for(auto && a : in_arcs(_graph.get(), u)) {
            const vertex v = arc_source(_graph.get(), a);
            if(_vertex_rank_map[v] != _vertex_rank_map[u] + 1) continue;
            if(_carried_flow_map[a] == 0) continue;
            value_t pushed_flow = dfs_push_flow(
                v, std::min(max_incomming_flow, _carried_flow_map[a]));
            _carried_flow_map[a] -= pushed_flow;
            total_pushed_flow += pushed_flow;
        }
        return total_pushed_flow;
    }

public:
    constexpr dinitz & run() noexcept {
        while(bfs_rank_vertices()) {
            dfs_push_flow(_s, std::numeric_limits<value_t>::max());
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
                _bfs_queue, [this](const vertex_t<G> & v) {
                    return std::views::filter(
                        out_arcs(_graph.get(), v), [this](const arc_t<G> & a) {
                            return _vertex_rank_map[arc_target(_graph.get(),
                                                               a)] ==
                                   std::numeric_limits<std::size_t>::max();
                        });
                }));
        } else {
            return std::views::filter(
                arcs(_graph.get()), [this](const arc_t<G> & a) {
                    return _vertex_rank_map[arc_source(_graph.get())] !=
                               std::numeric_limits<std::size_t>::max() &&
                           _vertex_rank_map[arc_target(_graph.get())] ==
                               std::numeric_limits<std::size_t>::max();
                });
        }
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_DINITZ_HPP
