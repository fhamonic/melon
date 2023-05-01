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

    vertex_map_t<G, std::pair<out_arcs_iterator_t<G>, out_arcs_range_t<G>>>
        _remaining_out_arcs;
    vertex_map_t<G, std::pair<in_arcs_iterator_t<G>, in_arcs_range_t<G>>>
        _remaining_in_arcs;

public:
    [[nodiscard]] constexpr dinitz(const G & g, const C & c)
        : _graph(g)
        , _capacity_map(c)
        , _carried_flow_map(create_arc_map<value_t>(g))
        , _vertex_rank_map(create_vertex_map<std::size_t>(g))
        , _remaining_out_arcs(
              create_vertex_map<
                  std::pair<out_arcs_iterator_t<G>, out_arcs_range_t<G>>>(g))
        , _remaining_in_arcs(
              create_vertex_map<
                  std::pair<in_arcs_iterator_t<G>, in_arcs_range_t<G>>>(g)) {
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
        for(auto && u : vertices(_graph.get())) {
            _remaining_out_arcs[u].second = out_arcs(_graph.get(), u);
            _remaining_in_arcs[u].second = in_arcs(_graph.get(), u);
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
            for(auto && a : in_arcs(_graph.get(), u)) {
                const vertex v = arc_source(_graph.get(), a);
                if(_vertex_rank_map[v] !=
                       std::numeric_limits<std::size_t>::max() ||
                   _capacity_map.get()[a] == _carried_flow_map[a])
                    continue;
                _vertex_rank_map[v] = _vertex_rank_map[u] + 1;
                _bfs_queue.push_back(v);
            }
            for(auto && a : out_arcs(_graph.get(), u)) {
                const vertex v = arc_target(_graph.get(), a);
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
        for(auto & current = _remaining_out_arcs[u].first;
            current != _remaining_out_arcs[u].second.end(); ++current) {
            const arc & a = *current;
            const vertex v = arc_target(_graph.get(), a);
            if(_vertex_rank_map[v] + 1 != _vertex_rank_map[u]) continue;
            if(_capacity_map.get()[a] == _carried_flow_map[a]) continue;
            const value_t pushed_flow = dfs_push_flow(
                v, std::min(max_incomming_flow,
                            _capacity_map.get()[a] - _carried_flow_map[a]));
            if(pushed_flow == 0) continue;
            _carried_flow_map[a] += pushed_flow;
            return pushed_flow;
        }
        for(auto & current = _remaining_in_arcs[u].first;
            current != _remaining_in_arcs[u].second.end(); ++current) {
            const arc & a = *current;
            const vertex v = arc_source(_graph.get(), a);
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
            for(auto && u : vertices(_graph.get())) {
                _remaining_out_arcs[u].first =
                    _remaining_out_arcs[u].second.begin();
                _remaining_in_arcs[u].first =
                    _remaining_in_arcs[u].second.begin();
            }
            while(dfs_push_flow(_s, std::numeric_limits<value_t>::max()) >
                  value_t{0})
                ;
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
                        in_arcs(_graph.get(), v), [this](const arc_t<G> & a) {
                            return _vertex_rank_map[arc_source(_graph.get(),
                                                               a)] ==
                                   std::numeric_limits<std::size_t>::max();
                        });
                }));
        } else {
            return std::views::filter(
                arcs(_graph.get()), [this](const arc_t<G> & a) {
                    return _vertex_rank_map[arc_source(_graph.get())] ==
                               std::numeric_limits<std::size_t>::max() &&
                           _vertex_rank_map[arc_target(_graph.get())] !=
                               std::numeric_limits<std::size_t>::max();
                });
        }
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_DINITZ_HPP
