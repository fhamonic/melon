#ifndef MELON_VIEWS_UNDIRECT_HPP
#define MELON_VIEWS_UNDIRECT_HPP

#include <algorithm>
#include <ranges>

#include "melon/graph.hpp"
#include "melon/views/graph_view.hpp"

namespace fhamonic {
namespace melon {
namespace views {

template <graph _Graph>
    requires outward_incidence_graph<_Graph> && inward_incidence_graph<_Graph>
class undirect : public undirected_graph_view_base {
private:
    using vertex = vertex_t<_Graph>;
    using edge = arc_t<_Graph>;

    _Graph _graph;

public:
    template <typename _G>
    [[nodiscard]] constexpr explicit undirect(_G && g)
        : _graph(views::graph_all(std::forward<_G>(g))) {}

    [[nodiscard]] constexpr undirect(const undirect &) = default;
    [[nodiscard]] constexpr undirect(undirect &&) = default;

    constexpr undirect & operator=(const undirect &) = default;
    constexpr undirect & operator=(undirect &&) = default;

    [[nodiscard]] constexpr decltype(auto) num_vertices() const
        requires requires(_Graph g) { melon::num_vertices(g); }
    {
        return melon::num_vertices(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) num_edges() const noexcept
        requires requires(_Graph g) { melon::num_arcs(g); }
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
        requires has_arc_target<_Graph> && has_arc_source<_Graph>
    {
        return {melon::arc_source(_graph, e), melon::arc_target(_graph, e)};
    }

    [[nodiscard]] constexpr decltype(auto) incidence(
        const vertex & u) const noexcept
        requires outward_incidence_graph<_Graph> &&
                 inward_incidence_graph<_Graph>
    {
        return std::views::concat(
            std::views::transform(
                melon::out_arcs(_graph, u),
                [&](auto && a) -> std::pair<edge, vertex> {
                    return {a, melon::arc_target(_graph, a)};
                }),
            std::views::transform(
                melon::in_arcs(_graph, u),
                [&](auto && a) -> std::pair<edge, vertex> {
                    return {a, melon::arc_source(_graph, a)};
                }));
    }

    template <typename T>
        requires has_vertex_map<_Graph>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const noexcept {
        return melon::create_vertex_map<T>(_graph);
    }
    template <typename T>
        requires has_vertex_map<_Graph>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        T default_value) const noexcept {
        return melon::create_vertex_map<T>(_graph, default_value);
    }

    template <typename T>
        requires has_arc_map<_Graph>
    [[nodiscard]] constexpr decltype(auto) create_edge_map() const noexcept {
        return melon::create_arc_map<T>(_graph);
    }
    template <typename T>
        requires has_arc_map<_Graph>
    [[nodiscard]] constexpr decltype(auto) create_edge_map(
        T default_value) const noexcept {
        return melon::create_arc_map<T>(_graph, default_value);
    }
};

template <typename _G>
undirect(_G &&) -> undirect<views::graph_all_t<_G>>;

}  // namespace views
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_VIEWS_UNDIRECT_HPP