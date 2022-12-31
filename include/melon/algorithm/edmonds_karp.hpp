#ifndef MELON_ALGORITHM_DIJKSTA_HPP
#define MELON_ALGORITHM_DIJKSTA_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/constexpr_ternary.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"
#include "melon/utility/traversal_iterator.hpp"
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
    arc_map_t<G, value_t> _capacity_left_map;

    std::vector<vertex> _bfs_queue;
    vertex_map_t<G, bool> _bfs_reached_map;
    vertex_map_t<G, arc> _bfs_pred_arc;
    arc_map_t<G, bool> _bfs_forward;

public:
    [[nodiscard]] constexpr edmonds_karp(const G & g, const C & c)
        : _graph(g)
        , _capacity_map(c)
        , _carried_flow_map(create_arc_map<value_t>(g))
        , _capacity_left_map(create_arc_map<value_t>(g))
        , _bfs_reached_map(create_vertex_map<bool>(g))
        , _bfs_pred_arc(create_vertex_map<arc>(g))
        , _bfs_forward(create_arc_map<bool>(g)) {
        _bfs_queue.reserve(g.nb_vertices());
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
        for(auto && a : arcs(_graph.get()))
            _capacity_left_map[a] = _capacity_map.get()[a];
        return *this;
    }

private:
    bool find_unsaturated_path() {
        _bfs_reached_map.fill(false);
        _bfs_queue.resize(0);
        _bfs_queue.push_back(_s);
        auto current = _bfs_queue.begin();
        while(current != _bfs_queue.end()) {
            const vertex & u = *current;
            for(auto && a : out_arcs(_graph.get(), u)) {
                if(_capacity_left_map[a] == 0) continue;
                const vertex v = arc_target(_graph.get(), a);
                if(_bfs_reached_map[v]) continue;
                _bfs_pred_arc[v] = a;
                _bfs_forward[a] = true;
                if(v == _t) return true;
                _bfs_reached_map[v] = true;
                _bfs_queue.push_back(v);
            }
            for(auto && a : in_arcs(_graph.get(), u)) {
                if(_carried_flow_map[a] == 0) continue;
                const vertex v = arc_source(_graph.get(), a);
                if(_bfs_reached_map[v]) continue;
                _bfs_pred_arc[v] = a;
                _bfs_forward[a] = false;
                if(v == _t) return true;
                _bfs_reached_map[v] = true;
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
            if(_bfs_forward[a]) {
                pushed_flow = std::min(pushed_flow, _capacity_left_map[a]);
                v = arc_source(_graph.get(), a);
            } else {
                pushed_flow = std::min(pushed_flow, _carried_flow_map[a]);
                v = arc_target(_graph.get(), a);
            }
        }
        v = _t;
        while(v != _s) {
            const arc a = _bfs_pred_arc[v];
            if(_bfs_forward[a]) {
                _carried_flow_map[a] += pushed_flow;
                _capacity_left_map[a] -= pushed_flow;
                v = arc_source(_graph.get(), a);
            } else {
                _carried_flow_map[a] -= pushed_flow;
                _capacity_left_map[a] += pushed_flow;
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
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_DIJKSTA_HPP
