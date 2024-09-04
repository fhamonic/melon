#ifndef MELON_UNDIRECTED_GRAPH_VIEW_HPP
#define MELON_UNDIRECTED_GRAPH_VIEW_HPP

#include <algorithm>
#include <ranges>

#include "melon/undirected_graph.hpp"

namespace fhamonic {
namespace melon {

struct undirected_graph_view_base {};

template <typename _Tp>
inline constexpr bool enable_undirected_graph_view =
    std::derived_from<_Tp, undirected_graph_view_base>;

template <typename _Tp>
concept undirected_graph_view = undirected_graph<_Tp> && std::movable<_Tp> &&
                                enable_undirected_graph_view<_Tp>;

template <undirected_graph G>
    requires std::is_object_v<G>
class undirected_graph_ref_view : public undirected_graph_view_base {
private:
    using vertex = vertex_t<G>;
    using edge = edge_t<G>;

    G * _undirected_graph;

public:
    template <typename _Tp>
        requires(!__detail::__specialization_of<_Tp, undirected_graph_ref_view>)
    [[nodiscard]] constexpr explicit undirected_graph_ref_view(_Tp && g)
        : _undirected_graph(
              std::addressof(static_cast<G &>(std::forward<_Tp>(g)))) {}

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
    [[nodiscard]] constexpr decltype(auto) nb_edges() const noexcept
        requires requires(G g) { melon::nb_edges(g); }
    {
        return melon::nb_edges(*_undirected_graph);
    }

    [[nodiscard]] constexpr decltype(auto) vertices() const noexcept {
        return melon::vertices(*_undirected_graph);
    }
    [[nodiscard]] constexpr decltype(auto) edges() const noexcept {
        return melon::edges(*_undirected_graph);
    }

    [[nodiscard]] constexpr vertex edge_endpoints(
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

template <typename _UndirectedGraph>
undirected_graph_ref_view(_UndirectedGraph &)
    -> undirected_graph_ref_view<_UndirectedGraph>;

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
    [[nodiscard]] constexpr decltype(auto) nb_edges() const noexcept
        requires requires(G g) { melon::nb_edges(g); }
    {
        return melon::nb_edges(_undirected_graph);
    }

    [[nodiscard]] constexpr decltype(auto) vertices() const noexcept {
        return melon::vertices(_undirected_graph);
    }
    [[nodiscard]] constexpr decltype(auto) edges() const noexcept {
        return melon::edges(_undirected_graph);
    }

    [[nodiscard]] constexpr vertex edge_endpoints(
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
namespace __cust_access {
namespace __detail {
template <typename _UndirectedGraph>
concept __can_undirected_graph_ref_view =
    requires { undirected_graph_ref_view{std::declval<_UndirectedGraph>()}; };

template <typename _UndirectedGraph>
concept __can_undirected_graph_owning_view = requires {
    undirected_graph_owning_view{std::declval<_UndirectedGraph>()};
};
}  // namespace __detail

struct _UndirectedGraphAll {
    template <typename _UndirectedGraph>
    static constexpr bool _S_noexcept() {
        if constexpr(undirected_graph_view<std::decay_t<_UndirectedGraph>>)
            return std::is_nothrow_constructible_v<
                std::decay_t<_UndirectedGraph>, _UndirectedGraph>;
        else if constexpr(__detail::__can_undirected_graph_ref_view<
                              _UndirectedGraph>)
            return true;
        else
            return noexcept(
                undirected_graph_owning_view{std::declval<_UndirectedGraph>()});
    }

    template <undirected_graph _UndirectedGraph>
    constexpr auto operator() [[nodiscard]] (_UndirectedGraph && __g) const
        noexcept(_S_noexcept<_UndirectedGraph>()) {
        if constexpr(undirected_graph_view<std::decay_t<_UndirectedGraph>>)
            return std::forward<_UndirectedGraph>(__g);
        else if constexpr(__detail::__can_undirected_graph_ref_view<
                              _UndirectedGraph>)
            return undirected_graph_ref_view{
                std::forward<_UndirectedGraph>(__g)};
        else
            return undirected_graph_owning_view{
                std::forward<_UndirectedGraph>(__g)};
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_UndirectedGraphAll undirected_graph_all{};
}  // namespace __cust

template <undirected_graph _UndirectedGraph>
using undirected_graph_all_t =
    decltype(undirected_graph_all(std::declval<_UndirectedGraph>()));

}  // namespace views
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UNDIRECTED_GRAPH_VIEW_HPP