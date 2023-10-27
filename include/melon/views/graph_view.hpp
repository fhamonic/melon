#ifndef MELON_GRAPH_VIEW_HPP
#define MELON_GRAPH_VIEW_HPP

#include <algorithm>
#include <ranges>

#include "melon/graph.hpp"

namespace fhamonic {
namespace melon {

namespace views {

struct graph_view_base {};

template <typename _Tp>
inline constexpr bool enable_graph_view =
    std::derived_from<_Tp, graph_view_base>;

template <typename _Tp>
concept graph_view = graph<_Tp> && std::movable<_Tp> && enable_graph_view<_Tp>;

template <graph G>
    requires std::is_object_v<G>
class graph_ref_view : public graph_view_base {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;

    G * _graph;

public:
    template <typename _Tp>
        requires(!__detail::__specialization_of<_Tp, graph_ref_view>)
    [[nodiscard]] constexpr explicit graph_ref_view(const _Tp & g)
        : _graph(std::addressof(static_cast<G &>(std::forward<_Tp>(g)))) {}

    [[nodiscard]] constexpr graph_ref_view(const graph_ref_view &) = default;
    [[nodiscard]] constexpr graph_ref_view(graph_ref_view &&) = default;

    constexpr graph_ref_view & operator=(const graph_ref_view &) = default;
    constexpr graph_ref_view & operator=(graph_ref_view &&) = default;

    constexpr G & base() const { return *_graph; }

    [[nodiscard]] constexpr decltype(auto) nb_vertices() const
        requires requires(G g) { melon::nb_vertices(g); }
    {
        return melon::nb_vertices(*_graph);
    }
    [[nodiscard]] constexpr decltype(auto) nb_arcs() const noexcept
        requires requires(G g) { melon::nb_arcs(g); }
    {
        return melon::nb_arcs(*_graph);
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

template <typename _Graph>
graph_ref_view(_Graph &) -> graph_ref_view<_Graph>;

template <graph G>
    requires std::movable<G>
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
        default;
    [[nodiscard]] constexpr graph_owning_view(graph_owning_view &&) = default;

    constexpr graph_owning_view & operator=(const graph_owning_view &) =
        default;
    constexpr graph_owning_view & operator=(graph_owning_view &&) = default;

    // constexpr const G & base() const { return _graph; }
    constexpr G && base() && noexcept { return std::move(_graph); }

    [[nodiscard]] constexpr decltype(auto) nb_vertices() const
        requires requires(G g) { melon::nb_vertices(g); }
    {
        return melon::nb_vertices(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) nb_arcs() const noexcept
        requires requires(G g) { melon::nb_arcs(g); }
    {
        return melon::nb_arcs(_graph);
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

namespace __cust_access {
namespace __detail {
template <typename _Graph>
concept __can_graph_ref_view =
    requires { graph_ref_view{std::declval<_Graph>()}; };

template <typename _Graph>
concept __can_graph_owning_view =
    requires { graph_owning_view{std::declval<_Graph>()}; };
}  // namespace __detail

struct _GraphAll {
    template <typename _Graph>
    static constexpr bool _S_noexcept() {
        if constexpr(graph_view<std::decay_t<_Graph>>)
            return std::is_nothrow_constructible_v<std::decay_t<_Graph>,
                                                   _Graph>;
        else if constexpr(__detail::__can_graph_ref_view<_Graph>)
            return true;
        else
            return noexcept(graph_owning_view{std::declval<_Graph>()});
    }

    template <graph G>
        requires graph<G> || __detail::__can_graph_ref_view<G> ||
                 __detail::__can_graph_owning_view<G>
    constexpr auto operator() [[nodiscard]] (G && __r) const
        noexcept(_S_noexcept<G>()) {
        if constexpr(graph_view<std::decay_t<G>>)
            return std::forward<G>(__r);
        else if constexpr(__detail::__can_graph_ref_view<G>)
            return graph_ref_view{std::forward<G>(__r)};
        else
            return graph_owning_view{std::forward<G>(__r)};
    }

    static constexpr bool _S_has_simple_call_op = true;
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_GraphAll graph_all{};
}  // namespace __cust

template <graph _Graph>
using graph_all_t = decltype(graph_all(std::declval<_Graph>()));

}  // namespace views
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_GRAPH_VIEW_HPP