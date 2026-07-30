#pragma once

#include <algorithm>
#include <ranges>

#include "melon/detail/borrowed_graph.hpp"
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

// The ten members undirected_graph_ref_view and undirected_graph_owning_view
// each used to spell out in full. Same arrangement as
// detail::graph_forwarding_interface: the derived class supplies
// `const G & _forwarding_base() const` and befriends this, and nothing else.
// There is no direction policy here -- an undirected graph has none.
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

    // See graph_forwarding_interface: the default value goes in by const
    // reference, like the CPO that forwards it here.
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

    // See graph_ref_view: std::ranges::ref_view::base().
    [[nodiscard]] constexpr G & base() const noexcept {
        return *_undirected_graph;
    }
};

template <typename UndirectedGraph>
undirected_graph_ref_view(UndirectedGraph &)
    -> undirected_graph_ref_view<UndirectedGraph>;

// See graph_ref_view's specialisation.
template <typename G>
inline constexpr bool enable_borrowed_graph<undirected_graph_ref_view<G>> =
    true;

// See graph_owning_view for the is_object_v conjunct: without it the class
// was a legal type for a reference argument whose constructor then
// hard-errored.
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

    // See graph_owning_view: the four overloads of
    // std::ranges::owning_view::base().
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

// Derives the same closure base as the directed adaptors: its operator|
// is constrained on invocability, so `ug | views::undirected_graph_all`
// works for exactly the operands the call form accepts.
struct undirected_graph_all_fn
    : views::graph_adaptor_closure<undirected_graph_all_fn> {
private:
    // See graph_all_fn::pass_through.
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
            // See graph_all_fn::is_noexcept.
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

// The undirected twin of graph_storable_as (views/graph_view.hpp): see
// mapping_storable_as (mapping.hpp) for why constructors are constrained on
// it, and graph_storable_as for why there is no direct-construction fallback.
template <typename UG, typename Stored>
concept undirected_graph_storable_as =
    requires(UG && ug) { views::undirected_graph_all(std::forward<UG>(ug)); } &&
    std::constructible_from<Stored, views::undirected_graph_all_t<UG>>;

namespace detail {

// See store_mapping (mapping.hpp). Fully qualified: inside melon::detail a
// bare `views::` finds melon::detail::views (the concat shim).
template <typename Stored, typename UG>
    requires undirected_graph_storable_as<UG, Stored>
[[nodiscard]] constexpr Stored store_undirected_graph(UG && ug) {
    return Stored(melon::views::undirected_graph_all(std::forward<UG>(ug)));
}

}  // namespace detail

}  // namespace melon
