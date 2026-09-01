#pragma once

#include <algorithm>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

#include "melon/borrowed_graph.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/undirected_graph.hpp"

namespace melon {

struct undirected_graph_view_base {};

template <typename T>
inline constexpr bool enable_undirected_graph_view =
    std::derived_from<T, undirected_graph_view_base>;

template <typename T>
concept undirected_graph_view =
    undirected_graph<T> && std::movable<T> && enable_undirected_graph_view<T>;

namespace detail {

template <typename Derived, typename G>
class undirected_graph_forwarding_interface
    : public undirected_graph_view_base {
private:
    using vertex = vertex_t<G>;
    using edge = edge_t<G>;

    [[nodiscard]] constexpr const G & _wrapped() const noexcept {
        return static_cast<const Derived &>(*this)._forwarding_base();
    }

public:
    [[nodiscard]] constexpr decltype(auto) num_vertices() const
        noexcept(noexcept(melon::num_vertices(std::declval<const G &>())))
        requires has_num_vertices<G>
    {
        return melon::num_vertices(_wrapped());
    }
    [[nodiscard]] constexpr decltype(auto) num_edges() const
        noexcept(noexcept(melon::num_edges(std::declval<const G &>())))
        requires has_num_edges<G>
    {
        return melon::num_edges(_wrapped());
    }

    [[nodiscard]] constexpr decltype(auto) vertices() const
        noexcept(noexcept(melon::vertices(std::declval<const G &>()))) {
        return melon::vertices(_wrapped());
    }
    [[nodiscard]] constexpr decltype(auto) edges() const
        noexcept(noexcept(melon::edges(std::declval<const G &>()))) {
        return melon::edges(_wrapped());
    }

    [[nodiscard]] constexpr std::pair<vertex, vertex> edge_endpoints(
        const edge & e) const
        noexcept(noexcept(melon::edge_endpoints(std::declval<const G &>(),
                                                e))) {
        return melon::edge_endpoints(_wrapped(), e);
    }

    [[nodiscard]] constexpr decltype(auto) incidence(const vertex & u) const
        noexcept(noexcept(melon::incidence(std::declval<const G &>(), u)))
        requires has_incidence<G>
    {
        return melon::incidence(_wrapped(), u);
    }

    // Forwarded rather than left to the CPO: the fallback sizes the incidence
    // range through the view, throwing away an O(1) member, and disappears
    // altogether where that range is not sized.
    [[nodiscard]] constexpr decltype(auto) degree(const vertex & u) const
        noexcept(noexcept(melon::degree(std::declval<const G &>(), u)))
        requires has_degree<G>
    {
        return melon::degree(_wrapped(), u);
    }

    template <typename T>
        requires has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const noexcept(
        noexcept(melon::create_vertex_map<T>(std::declval<const G &>()))) {
        return melon::create_vertex_map<T>(_wrapped());
    }
    template <typename T>
        requires has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        const T & default_value) const
        noexcept(noexcept(melon::create_vertex_map<T>(std::declval<const G &>(),
                                                      default_value))) {
        return melon::create_vertex_map<T>(_wrapped(), default_value);
    }

    template <typename T>
        requires has_edge_map<G>
    [[nodiscard]] constexpr decltype(auto) create_edge_map() const noexcept(
        noexcept(melon::create_edge_map<T>(std::declval<const G &>()))) {
        return melon::create_edge_map<T>(_wrapped());
    }
    template <typename T>
        requires has_edge_map<G>
    [[nodiscard]] constexpr decltype(auto) create_edge_map(
        const T & default_value) const
        noexcept(noexcept(melon::create_edge_map<T>(std::declval<const G &>(),
                                                    default_value))) {
        return melon::create_edge_map<T>(_wrapped(), default_value);
    }
};

}  // namespace detail

template <undirected_graph G>
    requires std::is_object_v<G>
class undirected_graph_ref_view
    : public detail::undirected_graph_forwarding_interface<
          undirected_graph_ref_view<G>, G> {
private:
    friend detail::undirected_graph_forwarding_interface<
        undirected_graph_ref_view<G>, G>;

    G * _undirected_graph;

    [[nodiscard]] constexpr const G & _forwarding_base() const noexcept {
        return *_undirected_graph;
    }

    // See graph_ref_view: the std::ranges::ref_view bindable-test, so a
    // temporary whose conversion materialises a G is rejected instead of
    // leaving the view dangling.
    static void bindable_test(G &);
    static void bindable_test(G &&) = delete;

public:
    template <typename T>
        requires detail::not_self<T, undirected_graph_ref_view> &&
                 std::convertible_to<T, G &> &&
                 requires { bindable_test(std::declval<T>()); }
    constexpr explicit undirected_graph_ref_view(T && g) noexcept(
        noexcept(static_cast<G &>(std::declval<T>())))
        : _undirected_graph(
              std::addressof(static_cast<G &>(std::forward<T>(g)))) {}

    constexpr undirected_graph_ref_view(const undirected_graph_ref_view &) =
        default;
    constexpr undirected_graph_ref_view(undirected_graph_ref_view &&) = default;

    constexpr undirected_graph_ref_view & operator=(
        const undirected_graph_ref_view &) = default;
    constexpr undirected_graph_ref_view & operator=(
        undirected_graph_ref_view &&) = default;

    [[nodiscard]] constexpr G & base() const noexcept {
        return *_undirected_graph;
    }
};

template <typename UndirectedGraph>
undirected_graph_ref_view(UndirectedGraph &)
    -> undirected_graph_ref_view<UndirectedGraph>;

// A bare pointer to a graph that lives elsewhere: relocating the view cannot
// invalidate a range obtained through it. undirected_graph_owning_view is
// deliberately left false -- it embeds the graph, so its ranges point into the
// view.
template <typename G>
inline constexpr bool enable_borrowed_graph<undirected_graph_ref_view<G>> =
    true;

// is_object_v alongside move_constructible, as in graph_owning_view: without
// it the class is a legal *type* for a reference argument whose only
// constructor then hard-errors.
template <undirected_graph G>
    requires std::move_constructible<G> && std::is_object_v<G>
class undirected_graph_owning_view
    : public detail::undirected_graph_forwarding_interface<
          undirected_graph_owning_view<G>, G> {
private:
    friend detail::undirected_graph_forwarding_interface<
        undirected_graph_owning_view<G>, G>;

    G _undirected_graph;

    [[nodiscard]] constexpr const G & _forwarding_base() const noexcept {
        return _undirected_graph;
    }

public:
    constexpr undirected_graph_owning_view(G && g) noexcept(
        std::is_nothrow_move_constructible_v<G>)
        : _undirected_graph(std::move(g)) {}

    undirected_graph_owning_view()
        requires std::default_initializable<G>
    = default;
    constexpr undirected_graph_owning_view(
        const undirected_graph_owning_view &) = delete;
    constexpr undirected_graph_owning_view(undirected_graph_owning_view &&) =
        default;

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
    [[nodiscard]] constexpr const G && base() const && noexcept {
        return std::move(_undirected_graph);
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

struct undirected_graph_all_fn
    : views::graph_adaptor_closure<undirected_graph_all_fn> {
private:
    // constructible_from as well as the view concept, which only asks for
    // movable: without it an lvalue owning view -- copy constructor deleted --
    // takes this branch and hard-errors inside the body, outside the immediate
    // context, so the candidate cannot even be removed.
    template <typename UndirectedGraph>
    static constexpr bool pass_through =
        undirected_graph_view<std::decay_t<UndirectedGraph>> &&
        std::constructible_from<std::decay_t<UndirectedGraph>, UndirectedGraph>;

    template <typename UndirectedGraph>
    static constexpr bool is_noexcept() {
        if constexpr(pass_through<UndirectedGraph>)
            return std::is_nothrow_constructible_v<
                std::decay_t<UndirectedGraph>, UndirectedGraph>;
        else if constexpr(detail::can_undirected_graph_ref_view<
                              UndirectedGraph>)
            // Measured, not assumed: the converting constructor's
            // static_cast<G &> can be made throwing by a user conversion
            // operator, so an unconditional `true` is a terminate-bomb.
            return noexcept(
                undirected_graph_ref_view{std::declval<UndirectedGraph>()});
        else
            return noexcept(
                undirected_graph_owning_view{std::declval<UndirectedGraph>()});
    }

public:
    template <undirected_graph UndirectedGraph>
        requires pass_through<UndirectedGraph> ||
                 detail::can_undirected_graph_ref_view<UndirectedGraph> ||
                 detail::can_undirected_graph_owning_view<UndirectedGraph>
    constexpr auto operator() [[nodiscard]] (UndirectedGraph && g) const
        noexcept(is_noexcept<UndirectedGraph>()) {
        if constexpr(pass_through<UndirectedGraph>)
            return std::decay_t<UndirectedGraph>(
                std::forward<UndirectedGraph>(g));
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

template <typename UG, typename UndirectedGraph>
concept undirected_graph_for =
    std::constructible_from<UndirectedGraph, views::undirected_graph_all_t<UG>>;

}  // namespace melon
