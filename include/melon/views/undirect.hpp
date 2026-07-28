#pragma once

#include <algorithm>
#include <ranges>

#include "melon/detail/concat_view.hpp"
#include "melon/graph.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {
namespace views {

template <graph Graph>
    requires outward_incidence_graph<Graph> && inward_incidence_graph<Graph>
class undirect : public undirected_graph_view_base {
private:
    using vertex = vertex_t<Graph>;
    using edge = arc_t<Graph>;

    Graph _graph;

public:
    template <typename G>
    [[nodiscard]] constexpr explicit undirect(G && g)
        : _graph(views::graph_all(std::forward<G>(g))) {}

    [[nodiscard]] constexpr undirect(const undirect &) = default;
    [[nodiscard]] constexpr undirect(undirect &&) = default;

    constexpr undirect & operator=(const undirect &) = default;
    constexpr undirect & operator=(undirect &&) = default;

    [[nodiscard]] constexpr decltype(auto) num_vertices() const
        requires requires(Graph g) { melon::num_vertices(g); }
    {
        return melon::num_vertices(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) num_edges() const noexcept
        requires requires(Graph g) { melon::num_arcs(g); }
    {
        return melon::num_arcs(_graph);
    }

    [[nodiscard]] constexpr decltype(auto) vertices() const noexcept {
        return melon::vertices(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) edges() const noexcept {
        return melon::arcs(_graph);
    }

    [[nodiscard]] constexpr std::pair<vertex, vertex> edge_endpoints(
        const edge & e) const noexcept
        requires has_arc_target<Graph> && has_arc_source<Graph>
    {
        return {melon::arc_source(_graph, e), melon::arc_target(_graph, e)};
    }

    [[nodiscard]] constexpr decltype(auto) incidence(
        const vertex & u) const noexcept
        requires outward_incidence_graph<Graph> && inward_incidence_graph<Graph>
    {
        return detail::views::concat(
            std::views::transform(melon::out_arcs(_graph, u),
                                  [&](auto && a) -> std::pair<edge, vertex> {
                                      return {a, melon::arc_target(_graph, a)};
                                  }),
            std::views::transform(melon::in_arcs(_graph, u),
                                  [&](auto && a) -> std::pair<edge, vertex> {
                                      return {a, melon::arc_source(_graph, a)};
                                  }));
    }

    [[nodiscard]] constexpr decltype(auto) adjacency(
        const vertex & u) const noexcept
        requires outward_adjacency_graph<Graph> && inward_adjacency_graph<Graph>
    {
        return detail::views::concat(melon::out_neighbors(_graph, u),
                                     melon::in_neighbors(_graph, u));
    }

    template <typename T>
        requires has_vertex_map<Graph>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const noexcept {
        return melon::create_vertex_map<T>(_graph);
    }
    template <typename T>
        requires has_vertex_map<Graph>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        T default_value) const noexcept {
        return melon::create_vertex_map<T>(_graph, default_value);
    }

    template <typename T>
        requires has_arc_map<Graph>
    [[nodiscard]] constexpr decltype(auto) create_edge_map() const noexcept {
        return melon::create_arc_map<T>(_graph);
    }
    template <typename T>
        requires has_arc_map<Graph>
    [[nodiscard]] constexpr decltype(auto) create_edge_map(
        T default_value) const noexcept {
        return melon::create_arc_map<T>(_graph, default_value);
    }
};

template <typename G>
undirect(G &&) -> undirect<views::graph_all_t<G>>;

}  // namespace views
}  // namespace melon
