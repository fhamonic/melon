#pragma once

#include <algorithm>
#include <cassert>
#include <ranges>
#include <stack>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/detail/consumable_view.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace melon {

struct depth_first_search_default_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    // Depth in the DFS tree, not a graph distance: it counts the arcs from
    // the source along the path DFS happened to take, so on the same graph it
    // changes with the order the out-arcs come in. Deliberately not called
    // store_distances -- breadth_first_search has a flag of that name whose
    // dist() is a genuine shortest-hop distance, and the two are not
    // interchangeable.
    static constexpr bool store_depth = false;
};

template <outward_adjacency_graph Graph,
          typename Traits = depth_first_search_default_traits>
    requires has_vertex_map<Graph>
class depth_first_search
    : public algorithm_view_interface<depth_first_search<Graph, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    static_assert(!Traits::store_pred_arcs || outward_incidence_graph<Graph>,
                  "storing predecessor arcs requires outward_incidence_graph.");

    using stack_range =
        std::conditional_t<Traits::store_pred_arcs, out_arcs_range_t<Graph>,
                           out_neighbors_range_t<Graph>>;

private:
    Graph _graph;
    std::vector<std::pair<vertex, consumable_view<stack_range>>> _stack;
    vertex_map_t<Graph, bool> _reached_map;

    [[no_unique_address]] vertex_map_if<Traits::store_pred_vertices, Graph,
                                        vertex> _pred_vertices_map;
    [[no_unique_address]] vertex_map_if<Traits::store_pred_arcs, Graph, arc>
        _pred_arcs_map;
    [[no_unique_address]] vertex_map_if<Traits::store_depth, Graph, int>
        _depth_map;

public:
    template <typename G>
    [[nodiscard]] constexpr explicit depth_first_search(G && g)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _stack()
        , _reached_map(create_vertex_map<bool>(_graph, false))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _depth_map(_graph) {
        if constexpr(has_num_vertices<Graph>) {
            _stack.reserve(num_vertices(_graph));
        }
    }

    template <typename G>
    [[nodiscard]] constexpr depth_first_search(G && g, const vertex & s)
        : depth_first_search(std::forward<G>(g)) {
        add_source(s);
    }

    template <typename... Args>
    [[nodiscard]] constexpr depth_first_search(Traits, Args &&... args)
        : depth_first_search(std::forward<Args>(args)...) {}

    [[nodiscard]] constexpr depth_first_search(const depth_first_search &) =
        default;
    [[nodiscard]] constexpr depth_first_search(depth_first_search &&) = default;

    constexpr depth_first_search & operator=(const depth_first_search &) =
        default;
    constexpr depth_first_search & operator=(depth_first_search &&) = default;

    constexpr depth_first_search & reset() noexcept {
        _stack.resize(0);
        _reached_map.fill(false);
        return *this;
    }
    constexpr depth_first_search & add_source(const vertex & s) noexcept {
        assert(!_reached_map[s]);
        if constexpr(Traits::store_pred_arcs)
            _stack.emplace_back(s, out_arcs(_graph, s));
        else
            _stack.emplace_back(s, out_neighbors(_graph, s));
        _reached_map[s] = true;
        if constexpr(Traits::store_pred_vertices) _pred_vertices_map[s] = s;
        if constexpr(Traits::store_depth) _depth_map[s] = 0;
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _stack.empty();
    }

    [[nodiscard]] constexpr vertex current() const noexcept {
        assert(!finished());
        return _stack.back().first;
    }

    constexpr void advance() noexcept {
        assert(!finished());
        do {
            if constexpr(Traits::store_pred_arcs) {
                for(auto & remaining_arcs = _stack.back().second;
                    !remaining_arcs.empty(); remaining_arcs.advance()) {
                    auto a = remaining_arcs.current();
                    auto w = arc_target(_graph, a);
                    if(_reached_map[w]) continue;
                    _reached_map[w] = true;
                    _pred_arcs_map[w] = a;
                    if constexpr(Traits::store_pred_vertices)
                        _pred_vertices_map[w] = _stack.back().first;
                    if constexpr(Traits::store_depth)
                        _depth_map[w] = _depth_map[_stack.back().first] + 1;
                    _stack.emplace_back(w, out_arcs(_graph, w));
                    remaining_arcs.advance();
                    return;
                }
            } else {
                for(auto & remaining_neighbors = _stack.back().second;
                    !remaining_neighbors.empty();
                    remaining_neighbors.advance()) {
                    auto w = remaining_neighbors.current();
                    if(_reached_map[w]) continue;
                    _reached_map[w] = true;
                    if constexpr(Traits::store_pred_vertices)
                        _pred_vertices_map[w] = _stack.back().first;
                    if constexpr(Traits::store_depth)
                        _depth_map[w] = _depth_map[_stack.back().first] + 1;
                    _stack.emplace_back(w, out_neighbors(_graph, w));
                    remaining_neighbors.advance();
                    return;
                }
            }
            _stack.pop_back();
        } while(!_stack.empty());
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _reached_map[u];
    }
    [[nodiscard]] constexpr auto reached_map() const noexcept {
        return views::mapping_all(_reached_map);
    }
    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const noexcept
        requires(Traits::store_pred_vertices)
    {
        assert(reached(u));
        return _pred_vertices_map[u];
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const noexcept
        requires(Traits::store_pred_arcs)
    {
        assert(reached(u));
        return _pred_arcs_map[u];
    }
    // Number of arcs from the source to u along the DFS tree -- equivalently,
    // the length of the pred_vertex chain up to the source. See store_depth:
    // this is not a shortest-path distance, and two runs over the same graph
    // can disagree if the out-arcs come in a different order.
    [[nodiscard]] constexpr int depth(const vertex & u) const noexcept
        requires(Traits::store_depth)
    {
        assert(reached(u));
        return _depth_map[u];
    }
};

template <typename Graph, typename Traits = depth_first_search_default_traits>
depth_first_search(Graph &&)
    -> depth_first_search<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits = depth_first_search_default_traits>
depth_first_search(Graph &&, const vertex_t<Graph> &)
    -> depth_first_search<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits>
depth_first_search(Traits, Graph &&)
    -> depth_first_search<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits>
depth_first_search(Traits, Graph &&, const vertex_t<Graph> &)
    -> depth_first_search<views::graph_all_t<Graph>, Traits>;

}  // namespace melon
