#ifndef MELON_ALGORITHM_EDMONDS_KARP_HPP
#define MELON_ALGORITHM_EDMONDS_KARP_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/utility/value_map.hpp"

namespace fhamonic {
namespace melon {

template <graph G, input_value_map<arc_t<G>> C>
    requires outward_incidence_graph<G> && inward_incidence_graph<G> &&
             has_vertex_map<G> && has_arc_map<G>
class edmonds_karp {
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
    vertex_map_t<G, bool> _bfs_reached_map;
    vertex_map_t<G, arc> _bfs_pred_arc;

public:
    [[nodiscard]] constexpr edmonds_karp(const G & g, const C & c)
        : _graph(g)
        , _capacity_map(c)
        , _carried_flow_map(create_arc_map<value_t>(g))
        , _bfs_reached_map(create_vertex_map<bool>(g))
        , _bfs_pred_arc(create_vertex_map<arc>(g)) {
        if constexpr(has_nb_vertices<G>) {
            _bfs_queue.reserve(nb_vertices(g));
        }
        reset();
    }

    [[nodiscard]] constexpr edmonds_karp(const G & g, const C & c,
                                         const vertex & s, const vertex & t)
        : edmonds_karp(g, c) {
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
        const auto & out_arcs_range = out_arcs(_graph.get(), _s);
        prefetch_range(out_arcs_range);
        prefetch_mapped_values(out_arcs_range, arc_targets_map(_graph.get()));
        prefetch_mapped_values(out_arcs_range, _capacity_map.get());
        prefetch_mapped_values(out_arcs_range, _carried_flow_map);
        _bfs_reached_map.fill(false);
        _bfs_reached_map[_s] = true;
        _bfs_queue.resize(0);
        _bfs_queue.push_back(_s);
        auto current = _bfs_queue.begin();
        while(current != _bfs_queue.end()) {
            const vertex & u = *current;
            for(auto && a : out_arcs(_graph.get(), u)) {
                const vertex v = arc_target(_graph.get(), a);
                if(_bfs_reached_map[v] ||
                   _capacity_map.get()[a] == _carried_flow_map[a])
                    continue;
                _bfs_pred_arc[v] = a;
                _bfs_reached_map[v] = true;
                if(v == _t) return true;
                _bfs_queue.push_back(v);
            }
            for(auto && a : in_arcs(_graph.get(), u)) {
                const vertex v = arc_source(_graph.get(), a);
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
            const vertex u = arc_source(_graph.get(), a);
            if(v != u) {
                pushed_flow = std::min(
                    pushed_flow, _capacity_map.get()[a] - _carried_flow_map[a]);
                v = u;
            } else {
                pushed_flow = std::min(pushed_flow, _carried_flow_map[a]);
                v = arc_target(_graph.get(), a);
            }
        }
        v = _t;
        while(v != _s) {
            const arc a = _bfs_pred_arc[v];
            const vertex u = arc_source(_graph.get(), a);
            if(v != u) {
                _carried_flow_map[a] += pushed_flow;
                v = u;
            } else {
                _carried_flow_map[a] -= pushed_flow;
                v = arc_target(_graph.get(), a);
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
        for(auto && a : out_arcs(_graph.get(), _s)) sum += _carried_flow_map[a];
        return sum;
    }
    constexpr auto minimum_cut() noexcept {
        if constexpr(std::ranges::viewable_range<out_arcs_range_t<G>>) {
            return std::views::join(std::views::transform(
                _bfs_queue, [this](const vertex_t<G> & v) {
                    return std::views::filter(
                        out_arcs(_graph.get(), v), [this](const arc_t<G> & a) {
                            return !_bfs_reached_map[arc_target(_graph.get(),
                                                                a)];
                        });
                }));
        } else {
            return std::views::filter(
                arcs(_graph.get()), [this](const arc_t<G> & a) {
                    return _bfs_reached_map[arc_source(_graph.get())] &&
                           !_bfs_reached_map[arc_target(_graph.get())];
                });
        }
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_EDMONDS_KARP_HPP
