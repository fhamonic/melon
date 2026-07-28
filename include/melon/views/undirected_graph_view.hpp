#pragma once

#include <algorithm>
#include <ranges>

#include "melon/detail/specialization_of.hpp"
#include "melon/undirected_graph.hpp"

namespace melon {

struct undirected_graph_view_base {};

template <typename T>
inline constexpr bool enable_undirected_graph_view =
    std::derived_from<T, undirected_graph_view_base>;

template <typename T>
concept undirected_graph_view =
    undirected_graph<T> && std::movable<T> && enable_undirected_graph_view<T>;

template <undirected_graph G>
    requires std::is_object_v<G>
class undirected_graph_ref_view : public undirected_graph_view_base {
private:
    using vertex = vertex_t<G>;
    using edge = edge_t<G>;

    G * _undirected_graph;

public:
    template <typename T>
        requires(!detail::specialization_of<T, undirected_graph_ref_view>)
    [[nodiscard]] constexpr explicit undirected_graph_ref_view(T && g)
        : _undirected_graph(
              std::addressof(static_cast<G &>(std::forward<T>(g)))) {}

    [[nodiscard]] constexpr undirected_graph_ref_view(
        const undirected_graph_ref_view &) = default;
    [[nodiscard]] constexpr undirected_graph_ref_view(
        undirected_graph_ref_view &&) = default;

    constexpr undirected_graph_ref_view & operator=(
        const undirected_graph_ref_view &) = default;
    constexpr undirected_graph_ref_view & operator=(
        undirected_graph_ref_view &&) = default;

    constexpr G & base() const { return *_undirected_graph; }

    [[nodiscard]] constexpr decltype(auto) num_vertices() const
        requires requires(G g) { melon::num_vertices(g); }
    {
        return melon::num_vertices(*_undirected_graph);
    }
    [[nodiscard]] constexpr decltype(auto) num_edges() const noexcept
        requires requires(G g) { melon::num_edges(g); }
    {
        return melon::num_edges(*_undirected_graph);
    }

    [[nodiscard]] constexpr decltype(auto) vertices() const noexcept {
        return melon::vertices(*_undirected_graph);
    }
    [[nodiscard]] constexpr decltype(auto) edges() const noexcept {
        return melon::edges(*_undirected_graph);
    }

    [[nodiscard]] constexpr std::pair<vertex, vertex> edge_endpoints(
        const edge & e) const noexcept {
        return melon::edge_endpoints(*_undirected_graph, e);
    }

    [[nodiscard]] constexpr decltype(auto) incidence(
        const vertex & u) const noexcept
        requires has_incidence<G>
    {
        return melon::incidence(*_undirected_graph, u);
    }

    template <typename T>
        requires has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const noexcept {
        return melon::create_vertex_map<T>(*_undirected_graph);
    }
    template <typename T>
        requires has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        T default_value) const noexcept {
        return melon::create_vertex_map<T>(*_undirected_graph, default_value);
    }

    template <typename T>
        requires has_edge_map<G>
    [[nodiscard]] constexpr decltype(auto) create_edge_map() const noexcept {
        return melon::create_edge_map<T>(*_undirected_graph);
    }
    template <typename T>
        requires has_edge_map<G>
    [[nodiscard]] constexpr decltype(auto) create_edge_map(
        const T & default_value) const noexcept {
        return melon::create_edge_map<T>(*_undirected_graph, default_value);
    }
};

template <typename UndirectedGraph>
undirected_graph_ref_view(UndirectedGraph &)
    -> undirected_graph_ref_view<UndirectedGraph>;

template <undirected_graph G>
    requires std::move_constructible<G>
class undirected_graph_owning_view : public undirected_graph_view_base {
private:
    using vertex = vertex_t<G>;
    using edge = edge_t<G>;

    G _undirected_graph;

public:
    constexpr undirected_graph_owning_view(G && g) noexcept(
        std::is_nothrow_move_constructible_v<G>)
        : _undirected_graph(std::move(g)) {}

    [[nodiscard]] undirected_graph_owning_view()
        requires std::default_initializable<G>
    = default;
    [[nodiscard]] constexpr undirected_graph_owning_view(
        const undirected_graph_owning_view &) = delete;
    [[nodiscard]] constexpr undirected_graph_owning_view(
        undirected_graph_owning_view &&) = default;

    constexpr undirected_graph_owning_view & operator=(
        const undirected_graph_owning_view &) = delete;
    constexpr undirected_graph_owning_view & operator=(
        undirected_graph_owning_view &&) = default;

    [[nodiscard]] constexpr G & base() & noexcept { return _undirected_graph; }

    [[nodiscard]] constexpr const G & base() const & noexcept {
        return _undirected_graph;
    }

    [[nodiscard]] constexpr G && base() && noexcept {
        return std::move(_undirected_graph);
    }

    [[nodiscard]] constexpr decltype(auto) num_vertices() const
        requires requires(G g) { melon::num_vertices(g); }
    {
        return melon::num_vertices(_undirected_graph);
    }
    [[nodiscard]] constexpr decltype(auto) num_edges() const noexcept
        requires requires(G g) { melon::num_edges(g); }
    {
        return melon::num_edges(_undirected_graph);
    }

    [[nodiscard]] constexpr decltype(auto) vertices() const noexcept {
        return melon::vertices(_undirected_graph);
    }
    [[nodiscard]] constexpr decltype(auto) edges() const noexcept {
        return melon::edges(_undirected_graph);
    }

    [[nodiscard]] constexpr std::pair<vertex, vertex> edge_endpoints(
        const edge & e) const noexcept {
        return melon::edge_endpoints(_undirected_graph, e);
    }

    [[nodiscard]] constexpr decltype(auto) incidence(
        const vertex & u) const noexcept
        requires has_incidence<G>
    {
        return melon::incidence(_undirected_graph, u);
    }

    template <typename T>
        requires has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const noexcept {
        return melon::create_vertex_map<T>(_undirected_graph);
    }
    template <typename T>
        requires has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        T default_value) const noexcept {
        return melon::create_vertex_map<T>(_undirected_graph, default_value);
    }

    template <typename T>
        requires has_edge_map<G>
    [[nodiscard]] constexpr decltype(auto) create_edge_map() const noexcept {
        return melon::create_edge_map<T>(_undirected_graph);
    }
    template <typename T>
        requires has_edge_map<G>
    [[nodiscard]] constexpr decltype(auto) create_edge_map(
        const T & default_value) const noexcept {
        return melon::create_edge_map<T>(_undirected_graph, default_value);
    }
};

namespace views {
namespace cpo {
namespace detail {
template <typename UndirectedGraph>
concept can_undirected_graph_ref_view =
    requires { undirected_graph_ref_view{std::declval<UndirectedGraph>()}; };

template <typename UndirectedGraph>
concept can_undirected_graph_owning_view =
    requires { undirected_graph_owning_view{std::declval<UndirectedGraph>()}; };
}  // namespace detail

struct undirected_graph_all_fn {
    template <typename UndirectedGraph>
    static constexpr bool is_noexcept() {
        if constexpr(undirected_graph_view<std::decay_t<UndirectedGraph>>)
            return std::is_nothrow_constructible_v<
                std::decay_t<UndirectedGraph>, UndirectedGraph>;
        else if constexpr(detail::can_undirected_graph_ref_view<
                              UndirectedGraph>)
            return true;
        else
            return noexcept(
                undirected_graph_owning_view{std::declval<UndirectedGraph>()});
    }

    template <undirected_graph UndirectedGraph>
    constexpr auto operator() [[nodiscard]] (UndirectedGraph && g) const
        noexcept(is_noexcept<UndirectedGraph>()) {
        if constexpr(undirected_graph_view<std::decay_t<UndirectedGraph>>)
            return std::forward<UndirectedGraph>(g);
        else if constexpr(detail::can_undirected_graph_ref_view<
                              UndirectedGraph>)
            return undirected_graph_ref_view{std::forward<UndirectedGraph>(g)};
        else
            return undirected_graph_owning_view{
                std::forward<UndirectedGraph>(g)};
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::undirected_graph_all_fn undirected_graph_all{};
}  // namespace cust

template <undirected_graph UndirectedGraph>
using undirected_graph_all_t =
    decltype(undirected_graph_all(std::declval<UndirectedGraph>()));

}  // namespace views
}  // namespace melon
