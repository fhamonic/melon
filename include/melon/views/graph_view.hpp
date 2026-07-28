#pragma once

#include <algorithm>
#include <ranges>

#include "melon/graph.hpp"

namespace melon {

struct graph_view_base {};

template <typename T>
inline constexpr bool enable_graph_view = std::derived_from<T, graph_view_base>;

template <typename T>
concept graph_view = graph<T> && std::movable<T> && enable_graph_view<T>;

template <graph G>
    requires std::is_object_v<G>
class graph_ref_view : public graph_view_base {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;

    G * _graph;

public:
    template <typename T>
        requires(!detail::specialization_of<T, graph_ref_view>)
    [[nodiscard]] constexpr explicit graph_ref_view(T && g)
        : _graph(std::addressof(static_cast<G &>(std::forward<T>(g)))) {}

    [[nodiscard]] constexpr graph_ref_view(const graph_ref_view &) = default;
    [[nodiscard]] constexpr graph_ref_view(graph_ref_view &&) = default;

    constexpr graph_ref_view & operator=(const graph_ref_view &) = default;
    constexpr graph_ref_view & operator=(graph_ref_view &&) = default;

    constexpr G & base() const { return *_graph; }

    [[nodiscard]] constexpr decltype(auto) num_vertices() const
        requires requires(G g) { melon::num_vertices(g); }
    {
        return melon::num_vertices(*_graph);
    }
    [[nodiscard]] constexpr decltype(auto) num_arcs() const noexcept
        requires requires(G g) { melon::num_arcs(g); }
    {
        return melon::num_arcs(*_graph);
    }

    [[nodiscard]] constexpr decltype(auto) vertices() const noexcept {
        return melon::vertices(*_graph);
    }
    [[nodiscard]] constexpr decltype(auto) arcs() const noexcept {
        return melon::arcs(*_graph);
    }

    [[nodiscard]] constexpr vertex arc_source(const arc & a) const noexcept
        requires has_arc_source<G>
    {
        return melon::arc_source(*_graph, a);
    }
    [[nodiscard]] constexpr vertex arc_target(const arc & a) const noexcept
        requires has_arc_target<G>
    {
        return melon::arc_target(*_graph, a);
    }

    [[nodiscard]] constexpr decltype(auto) sources_map() const noexcept
        requires has_arc_source<G>
    {
        return melon::arc_sources_map(*_graph);
    }
    [[nodiscard]] constexpr decltype(auto) targets_map() const noexcept
        requires has_arc_target<G>
    {
        return melon::arc_targets_map(*_graph);
    }

    [[nodiscard]] constexpr decltype(auto) out_arcs(
        const vertex & u) const noexcept
        requires has_out_arcs<G>
    {
        return melon::out_arcs(*_graph, u);
    }
    [[nodiscard]] constexpr decltype(auto) in_arcs(
        const vertex & u) const noexcept
        requires has_in_arcs<G>
    {
        return melon::in_arcs(*_graph, u);
    }

    [[nodiscard]] constexpr decltype(auto) out_neighbors(
        const vertex & u) const noexcept
        requires outward_adjacency_graph<G>
    {
        return melon::out_neighbors(*_graph, u);
    }
    [[nodiscard]] constexpr decltype(auto) in_neighbors(
        const vertex & u) const noexcept
        requires inward_adjacency_graph<G>
    {
        return melon::in_neighbors(*_graph, u);
    }

    template <typename T>
        requires has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const noexcept {
        return melon::create_vertex_map<T>(*_graph);
    }
    template <typename T>
        requires has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        T default_value) const noexcept {
        return melon::create_vertex_map<T>(*_graph, default_value);
    }

    template <typename T>
        requires has_arc_map<G>
    [[nodiscard]] constexpr decltype(auto) create_arc_map() const noexcept {
        return melon::create_arc_map<T>(*_graph);
    }
    template <typename T>
        requires has_arc_map<G>
    [[nodiscard]] constexpr decltype(auto) create_arc_map(
        const T & default_value) const noexcept {
        return melon::create_arc_map<T>(*_graph, default_value);
    }
};

template <typename Graph>
graph_ref_view(Graph &) -> graph_ref_view<Graph>;

template <graph G>
    requires std::move_constructible<G>
class graph_owning_view : public graph_view_base {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;

    G _graph;

public:
    constexpr graph_owning_view(G && g) noexcept(
        std::is_nothrow_move_constructible_v<G>)
        : _graph(std::move(g)) {}

    [[nodiscard]] graph_owning_view()
        requires std::default_initializable<G>
    = default;
    [[nodiscard]] constexpr graph_owning_view(const graph_owning_view &) =
        delete;
    [[nodiscard]] constexpr graph_owning_view(graph_owning_view &&) = default;

    constexpr graph_owning_view & operator=(const graph_owning_view &) = delete;
    constexpr graph_owning_view & operator=(graph_owning_view &&) = default;

    [[nodiscard]] constexpr G & base() & noexcept { return _graph; }

    [[nodiscard]] constexpr const G & base() const & noexcept { return _graph; }

    [[nodiscard]] constexpr G && base() && noexcept {
        return std::move(_graph);
    }

    [[nodiscard]] constexpr decltype(auto) num_vertices() const
        requires requires(G g) { melon::num_vertices(g); }
    {
        return melon::num_vertices(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) num_arcs() const noexcept
        requires requires(G g) { melon::num_arcs(g); }
    {
        return melon::num_arcs(_graph);
    }

    [[nodiscard]] constexpr decltype(auto) vertices() const noexcept {
        return melon::vertices(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) arcs() const noexcept {
        return melon::arcs(_graph);
    }

    [[nodiscard]] constexpr vertex arc_source(const arc & a) const noexcept
        requires has_arc_source<G>
    {
        return melon::arc_source(_graph, a);
    }
    [[nodiscard]] constexpr vertex arc_target(const arc & a) const noexcept
        requires has_arc_target<G>
    {
        return melon::arc_target(_graph, a);
    }

    [[nodiscard]] constexpr decltype(auto) sources_map() const noexcept
        requires has_arc_source<G>
    {
        return melon::arc_sources_map(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) targets_map() const noexcept
        requires has_arc_target<G>
    {
        return melon::arc_targets_map(_graph);
    }

    [[nodiscard]] constexpr decltype(auto) out_arcs(
        const vertex & u) const noexcept
        requires has_out_arcs<G>
    {
        return melon::out_arcs(_graph, u);
    }
    [[nodiscard]] constexpr decltype(auto) in_arcs(
        const vertex & u) const noexcept
        requires has_in_arcs<G>
    {
        return melon::in_arcs(_graph, u);
    }

    [[nodiscard]] constexpr decltype(auto) out_neighbors(
        const vertex & u) const noexcept
        requires outward_adjacency_graph<G>
    {
        return melon::out_neighbors(_graph, u);
    }
    [[nodiscard]] constexpr decltype(auto) in_neighbors(
        const vertex & u) const noexcept
        requires inward_adjacency_graph<G>
    {
        return melon::in_neighbors(_graph, u);
    }

    template <typename T>
        requires has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const noexcept {
        return melon::create_vertex_map<T>(_graph);
    }
    template <typename T>
        requires has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        T default_value) const noexcept {
        return melon::create_vertex_map<T>(_graph, default_value);
    }

    template <typename T>
        requires has_arc_map<G>
    [[nodiscard]] constexpr decltype(auto) create_arc_map() const noexcept {
        return melon::create_arc_map<T>(_graph);
    }
    template <typename T>
        requires has_arc_map<G>
    [[nodiscard]] constexpr decltype(auto) create_arc_map(
        const T & default_value) const noexcept {
        return melon::create_arc_map<T>(_graph, default_value);
    }
};

namespace views {
namespace cpo {
namespace detail {
template <typename Graph>
concept can_graph_ref_view =
    requires { graph_ref_view{std::declval<Graph>()}; };

template <typename Graph>
concept can_graph_owning_view =
    requires { graph_owning_view{std::declval<Graph>()}; };
}  // namespace detail

struct graph_all_fn {
    template <typename Graph>
    static constexpr bool is_noexcept() {
        if constexpr(graph_view<std::decay_t<Graph>>)
            return std::is_nothrow_constructible_v<std::decay_t<Graph>, Graph>;
        else if constexpr(detail::can_graph_ref_view<Graph>)
            return true;
        else
            return noexcept(graph_owning_view{std::declval<Graph>()});
    }

    template <graph Graph>
    constexpr auto operator() [[nodiscard]] (Graph && g) const
        noexcept(is_noexcept<Graph>()) {
        if constexpr(graph_view<std::decay_t<Graph>>)
            return std::forward<Graph>(g);
        else if constexpr(detail::can_graph_ref_view<Graph>)
            return graph_ref_view{std::forward<Graph>(g)};
        else
            return graph_owning_view{std::forward<Graph>(g)};
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::graph_all_fn graph_all{};
}  // namespace cust

template <graph Graph>
using graph_all_t = decltype(graph_all(std::declval<Graph>()));

}  // namespace views
}  // namespace melon
