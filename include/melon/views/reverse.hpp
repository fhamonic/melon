#ifndef MELON_VIEWS_REVERSE_HPP
#define MELON_VIEWS_REVERSE_HPP

#include <algorithm>
#include <ranges>

#include "melon/graph.hpp"
#include "melon/views/graph_view.hpp"

namespace fhamonic {
namespace melon {
namespace views {

template <graph _Graph>
class reverse : public graph_view_base {
private:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;

    _Graph _graph;

public:
    template <typename _G>
    [[nodiscard]] constexpr explicit reverse(_G && g)
        : _graph(views::graph_all(std::forward<_G>(g))) {}

    [[nodiscard]] constexpr reverse(const reverse &) = default;
    [[nodiscard]] constexpr reverse(reverse &&) = default;

    constexpr reverse & operator=(const reverse &) = default;
    constexpr reverse & operator=(reverse &&) = default;

    [[nodiscard]] constexpr decltype(auto) num_vertices() const
        requires requires(_Graph g) { melon::num_vertices(g); }
    {
        return melon::num_vertices(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) num_arcs() const noexcept
        requires requires(_Graph g) { melon::num_arcs(g); }
    {
        return melon::num_arcs(_graph);
    }

    [[nodiscard]] constexpr decltype(auto) vertices() const noexcept {
        return melon::vertices(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) arcs() const noexcept {
        return melon::arcs(_graph);
    }

    [[nodiscard]] constexpr vertex arc_source(arc a) const noexcept
        requires has_arc_target<_Graph>
    {
        return melon::arc_target(_graph, a);
    }
    [[nodiscard]] constexpr vertex arc_target(arc a) const noexcept
        requires has_arc_source<_Graph>
    {
        return melon::arc_source(_graph, a);
    }

    [[nodiscard]] constexpr decltype(auto) sources_map() const noexcept
        requires has_arc_target<_Graph>
    {
        return melon::arc_targets_map(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) targets_map() const noexcept
        requires has_arc_source<_Graph>
    {
        return melon::arc_sources_map(_graph);
    }

    [[nodiscard]] constexpr decltype(auto) out_arcs(
        const vertex u) const noexcept
        requires has_in_arcs<_Graph>
    {
        return melon::in_arcs(_graph, u);
    }
    [[nodiscard]] constexpr decltype(auto) in_arcs(
        const vertex u) const noexcept
        requires has_out_arcs<_Graph>
    {
        return melon::out_arcs(_graph, u);
    }

    [[nodiscard]] constexpr decltype(auto) out_neighbors(
        const vertex u) const noexcept
        requires inward_adjacency_graph<_Graph>
    {
        return melon::in_neighbors(_graph, u);
    }
    [[nodiscard]] constexpr decltype(auto) in_neighbors(
        const vertex u) const noexcept
        requires outward_adjacency_graph<_Graph>
    {
        return melon::out_neighbors(_graph, u);
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
    [[nodiscard]] constexpr decltype(auto) create_arc_map() const noexcept {
        return melon::create_arc_map<T>(_graph);
    }
    template <typename T>
        requires has_arc_map<_Graph>
    [[nodiscard]] constexpr decltype(auto) create_arc_map(
        T default_value) const noexcept {
        return melon::create_arc_map<T>(_graph, default_value);
    }
};

template <typename _G>
reverse(_G &&) -> reverse<views::graph_all_t<_G>>;

}  // namespace views
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_VIEWS_REVERSE_HPP