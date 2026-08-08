#pragma once

#include <algorithm>
#include <concepts>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "melon/borrowed_graph.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"

namespace melon {

struct graph_view_base {};

template <typename T>
inline constexpr bool enable_graph_view = std::derived_from<T, graph_view_base>;

template <typename T>
concept graph_view = graph<T> && std::movable<T> && enable_graph_view<T>;

namespace detail {

// One definition of each forwarding member, shared by graph_ref_view,
// graph_owning_view and views::reverse.
//
// A derived class supplies `const G & _forwarding_base() const` and befriends
// this class. Every accessor here forwards straight through; a view that means
// something different by a direction -- views::reverse -- redeclares just the
// members it crosses over, and name hiding does the rest. That also gets the
// availability right for free: reverse::out_arcs is constrained on
// has_in_arcs<Graph>, so reversing a graph with no in-incidence removes the
// member rather than silently falling back to the base's.

template <typename Derived, typename G>
class graph_forwarding_interface : public graph_view_base {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;

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
    [[nodiscard]] constexpr decltype(auto) num_arcs() const
        noexcept(noexcept(melon::num_arcs(std::declval<const G &>())))
        requires has_num_arcs<G>
    {
        return melon::num_arcs(_wrapped());
    }

    [[nodiscard]] constexpr decltype(auto) vertices() const
        noexcept(noexcept(melon::vertices(std::declval<const G &>()))) {
        return melon::vertices(_wrapped());
    }
    [[nodiscard]] constexpr decltype(auto) arcs() const
        noexcept(noexcept(melon::arcs(std::declval<const G &>()))) {
        return melon::arcs(_wrapped());
    }

    // Forwarded only when the wrapped graph carries its own arcs_entries, not
    // when the CPO would synthesise one: forwarding a synthesised one stacks a
    // transform on a transform for every container in the library, all of which
    // reach arcs_entries through the fallback. The member cannot be dropped
    // either -- `graph<G>` is defined in terms of arcs_entries, so a view over
    // a graph whose arcs_entries is its only arc protocol would silently stop
    // modeling `graph` and every algorithm would reject it.
    [[nodiscard]] constexpr decltype(auto) arcs_entries() const
        noexcept(noexcept(melon::arcs_entries(std::declval<const G &>())))
        requires cpo::has_own_arcs_entries<G>
    {
        return melon::arcs_entries(_wrapped());
    }

    // A view is read-only, so it forwards the *question* without forwarding
    // remove_vertex / remove_arc. views::subgraph consults these to decide
    // whether a handle is still live in the graph underneath it; without them
    // it could only consult its own filter, and would report a vertex removed
    // from a mutable_digraph as valid.
    [[nodiscard]] constexpr decltype(auto) is_valid_vertex(
        const vertex & v) const
        noexcept(noexcept(melon::is_valid_vertex(std::declval<const G &>(), v)))
        requires has_is_valid_vertex<G>
    {
        return melon::is_valid_vertex(_wrapped(), v);
    }
    [[nodiscard]] constexpr decltype(auto) is_valid_arc(const arc & a) const
        noexcept(noexcept(melon::is_valid_arc(std::declval<const G &>(), a)))
        requires has_is_valid_arc<G>
    {
        return melon::is_valid_arc(_wrapped(), a);
    }

    [[nodiscard]] constexpr decltype(auto) out_degree(const vertex & u) const
        noexcept(noexcept(melon::out_degree(std::declval<const G &>(), u)))
        requires has_out_degree<G>
    {
        return melon::out_degree(_wrapped(), u);
    }
    [[nodiscard]] constexpr decltype(auto) in_degree(const vertex & u) const
        noexcept(noexcept(melon::in_degree(std::declval<const G &>(), u)))
        requires has_in_degree<G>
    {
        return melon::in_degree(_wrapped(), u);
    }

    [[nodiscard]] constexpr vertex arc_source(const arc & a) const
        noexcept(noexcept(melon::arc_source(std::declval<const G &>(), a)))
        requires has_arc_source<G>
    {
        return melon::arc_source(_wrapped(), a);
    }
    [[nodiscard]] constexpr vertex arc_target(const arc & a) const
        noexcept(noexcept(melon::arc_target(std::declval<const G &>(), a)))
        requires has_arc_target<G>
    {
        return melon::arc_target(_wrapped(), a);
    }

    [[nodiscard]] constexpr decltype(auto) arc_sources_map() const
        noexcept(noexcept(melon::arc_sources_map(std::declval<const G &>())))
        requires has_arc_sources_map<G>
    {
        return melon::arc_sources_map(_wrapped());
    }
    [[nodiscard]] constexpr decltype(auto) arc_targets_map() const
        noexcept(noexcept(melon::arc_targets_map(std::declval<const G &>())))
        requires has_arc_targets_map<G>
    {
        return melon::arc_targets_map(_wrapped());
    }

    [[nodiscard]] constexpr decltype(auto) out_arcs(const vertex & u) const
        noexcept(noexcept(melon::out_arcs(std::declval<const G &>(), u)))
        requires has_out_arcs<G>
    {
        return melon::out_arcs(_wrapped(), u);
    }
    [[nodiscard]] constexpr decltype(auto) in_arcs(const vertex & u) const
        noexcept(noexcept(melon::in_arcs(std::declval<const G &>(), u)))
        requires has_in_arcs<G>
    {
        return melon::in_arcs(_wrapped(), u);
    }

    [[nodiscard]] constexpr decltype(auto) out_neighbors(const vertex & u) const
        noexcept(noexcept(melon::out_neighbors(std::declval<const G &>(), u)))
        requires outward_adjacency_graph<G>
    {
        return melon::out_neighbors(_wrapped(), u);
    }
    [[nodiscard]] constexpr decltype(auto) in_neighbors(const vertex & u) const
        noexcept(noexcept(melon::in_neighbors(std::declval<const G &>(), u)))
        requires inward_adjacency_graph<G>
    {
        return melon::in_neighbors(_wrapped(), u);
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
        requires has_arc_map<G>
    [[nodiscard]] constexpr decltype(auto) create_arc_map() const noexcept(
        noexcept(melon::create_arc_map<T>(std::declval<const G &>()))) {
        return melon::create_arc_map<T>(_wrapped());
    }
    template <typename T>
        requires has_arc_map<G>
    [[nodiscard]] constexpr decltype(auto) create_arc_map(
        const T & default_value) const
        noexcept(noexcept(melon::create_arc_map<T>(std::declval<const G &>(),
                                                   default_value))) {
        return melon::create_arc_map<T>(_wrapped(), default_value);
    }
};

}  // namespace detail

template <graph G>
    requires std::is_object_v<G>
class graph_ref_view
    : public detail::graph_forwarding_interface<graph_ref_view<G>, G> {
private:
    friend detail::graph_forwarding_interface<graph_ref_view<G>, G>;

    G * _graph;

    [[nodiscard]] constexpr const G & _forwarding_base() const noexcept {
        return *_graph;
    }

    // The std::ranges::ref_view bindable-test: convertible_to<T, G &> alone
    // accepts a temporary whose conversion sequence materialises a G (a type
    // with both operator G&() and operator G()), leaving _graph aimed at an
    // object that dies at the end of the full-expression.
    static void bindable_test(G &);
    static void bindable_test(G &&) = delete;

public:
    template <typename T>
        requires detail::not_self<T, graph_ref_view> &&
                 std::convertible_to<T, G &> &&
                 requires { bindable_test(std::declval<T>()); }
    constexpr explicit graph_ref_view(T && g) noexcept(
        noexcept(static_cast<G &>(std::declval<T>())))
        : _graph(std::addressof(static_cast<G &>(std::forward<T>(g)))) {}

    constexpr graph_ref_view(const graph_ref_view &) = default;
    constexpr graph_ref_view(graph_ref_view &&) = default;

    constexpr graph_ref_view & operator=(const graph_ref_view &) = default;
    constexpr graph_ref_view & operator=(graph_ref_view &&) = default;

    // std::ranges::ref_view::base(): one overload, returning the referenced
    // graph by lvalue reference from a const member. Constness here is shallow,
    // like the view itself.
    [[nodiscard]] constexpr G & base() const noexcept { return *_graph; }
};

template <typename Graph>
graph_ref_view(Graph &) -> graph_ref_view<Graph>;

// A bare pointer to a graph that lives elsewhere: relocating the view cannot
// invalidate a range obtained through it. graph_owning_view is deliberately
// left false -- it embeds the graph, so its ranges point into the view.
template <typename G>
inline constexpr bool enable_borrowed_graph<graph_ref_view<G>> = true;

// is_object_v alongside move_constructible: move_constructible<G &> is true,
// so without it graph_owning_view<SD &> is a legal *type* whose only
// constructor then hard-errors -- the class name promises owning while the
// parameter cannot be owned.
template <graph G>
    requires std::move_constructible<G> && std::is_object_v<G>
class graph_owning_view
    : public detail::graph_forwarding_interface<graph_owning_view<G>, G> {
private:
    friend detail::graph_forwarding_interface<graph_owning_view<G>, G>;

    G _graph;

    [[nodiscard]] constexpr const G & _forwarding_base() const noexcept {
        return _graph;
    }

public:
    constexpr graph_owning_view(G && g) noexcept(
        std::is_nothrow_move_constructible_v<G>)
        : _graph(std::move(g)) {}

    graph_owning_view()
        requires std::default_initializable<G>
    = default;
    constexpr graph_owning_view(const graph_owning_view &) = delete;
    constexpr graph_owning_view(graph_owning_view &&) = default;

    constexpr graph_owning_view & operator=(const graph_owning_view &) = delete;
    constexpr graph_owning_view & operator=(graph_owning_view &&) = default;

    // The four overloads of std::ranges::owning_view::base(). Without the
    // const && one, `std::move(std::as_const(v)).base()` binds to the const &
    // overload and hands back an lvalue reference.
    [[nodiscard]] constexpr G & base() & noexcept { return _graph; }
    [[nodiscard]] constexpr const G & base() const & noexcept { return _graph; }
    [[nodiscard]] constexpr G && base() && noexcept {
        return std::move(_graph);
    }
    [[nodiscard]] constexpr const G && base() const && noexcept {
        return std::move(_graph);
    }
};

namespace views {

template <typename Derived>
    requires std::is_class_v<Derived> &&
             std::same_as<Derived, std::remove_cv_t<Derived>>
struct graph_adaptor_closure;

namespace detail {

// The CRTP contract, checked structurally: T derives from
// graph_adaptor_closure<T>. It is what lets the two operator| overloads
// below tell an adaptor from the graph being adapted.
template <typename D>
void adaptor_closure_test(const graph_adaptor_closure<D> &)
    requires std::derived_from<D, graph_adaptor_closure<D>>;

template <typename T>
concept adaptor_closure =
    requires(const std::remove_cvref_t<T> & t) { adaptor_closure_test(t); };

template <typename C1, typename C2>
class composed_adaptor_closure;

}  // namespace detail

// CRTP base giving an adaptor object the std::ranges pipe syntax:
// deriving D from graph_adaptor_closure<D> enables `g | d` for everything
// d(g) accepts, and `d | e` for a second closure e.
// std::ranges::range_adaptor_closure cannot be reused for this: its
// operator| is constrained on std::ranges::range, which a graph is not.
// Constrained on invocability rather than on melon::graph, so the same
// base serves the undirected adaptors; each adaptor's own operator()
// carries the graph constraint.
template <typename Derived>
    requires std::is_class_v<Derived> &&
             std::same_as<Derived, std::remove_cv_t<Derived>>
struct graph_adaptor_closure {
    // g | closure. Self deduces the closure's value category, so piping an
    // rvalue closure can move its bound state into the view it builds.
    template <typename G, typename Self>
        requires std::same_as<std::remove_cvref_t<Self>, Derived> &&
                 (!detail::adaptor_closure<G>) && std::invocable<Self, G>
    [[nodiscard]] friend constexpr auto operator|(
        G && g, Self && self) noexcept(std::is_nothrow_invocable_v<Self, G>) {
        return std::forward<Self>(self)(std::forward<G>(g));
    }

    // closure | closure, itself a closure: `g | (c1 | c2)` and
    // `(g | c1) | c2` build the same type.
    template <typename Self, typename Other>
        requires std::same_as<std::remove_cvref_t<Self>, Derived> &&
                 detail::adaptor_closure<Other>
    [[nodiscard]] friend constexpr auto operator|(Self && self,
                                                  Other && other) {
        return detail::composed_adaptor_closure<std::remove_cvref_t<Self>,
                                                std::remove_cvref_t<Other>>(
            std::forward<Self>(self), std::forward<Other>(other));
    }
};

namespace detail {

template <typename C1, typename C2>
class composed_adaptor_closure
    : public graph_adaptor_closure<composed_adaptor_closure<C1, C2>> {
private:
    [[no_unique_address]] C1 _first;
    [[no_unique_address]] C2 _second;

public:
    constexpr composed_adaptor_closure(C1 first, C2 second) noexcept(
        std::is_nothrow_move_constructible_v<C1> &&
        std::is_nothrow_move_constructible_v<C2>)
        : _first(std::move(first)), _second(std::move(second)) {}

    template <typename G>
        requires std::invocable<const C1 &, G> &&
                 std::invocable<const C2 &, std::invoke_result_t<const C1 &, G>>
    [[nodiscard]] constexpr auto operator()(G && g) const & {
        return _second(_first(std::forward<G>(g)));
    }
    template <typename G>
        requires std::invocable<C1, G> &&
                 std::invocable<C2, std::invoke_result_t<C1, G>>
    [[nodiscard]] constexpr auto operator()(G && g) && {
        return std::move(_second)(std::move(_first)(std::forward<G>(g)));
    }
};

// The bound form of a multi-argument adaptor -- what views::subgraph(vf)
// hands back while it waits for the graph, like the closures
// std::views::filter(pred) returns. Bound arguments are stored decayed;
// applying an rvalue closure moves them into the view, applying an lvalue
// one copies them, so a closure is reusable.
template <typename Fn, typename... Args>
class adaptor_partial
    : public graph_adaptor_closure<adaptor_partial<Fn, Args...>> {
private:
    [[no_unique_address]] Fn _fn;
    [[no_unique_address]] std::tuple<Args...> _args;

public:
    template <typename... Ts>
        requires(std::constructible_from<Args, Ts> && ...)
    constexpr explicit adaptor_partial(Ts &&... args)
        : _fn(), _args(std::forward<Ts>(args)...) {}

    // Constrained on melon::graph as well as invocability: without the
    // graph conjunct, piping a non-graph into a bound subgraph closure
    // re-enters subgraph_fn's *binding* overload and "succeeds" with a
    // closure over garbage instead of failing at the pipe.
    //
    // Both overloads hand the adaptor *prvalue* copies of the bound
    // arguments (moves, for an rvalue closure), never lvalue references to
    // them: an lvalue would make mapping_all wrap a reference into this
    // closure object, and the view would dangle the moment the closure
    // died. A closure is therefore self-contained, like std's -- so the
    // type it builds does not depend on the closure's value category.
    template <typename G>
        requires graph<std::remove_cvref_t<G>> &&
                 std::invocable<const Fn &, G, Args...>
    [[nodiscard]] constexpr auto operator()(G && g) const & {
        return std::apply(
            [&g, this](const Args &... args) {
                return _fn(std::forward<G>(g), Args(args)...);
            },
            _args);
    }
    template <typename G>
        requires graph<std::remove_cvref_t<G>> &&
                 std::invocable<const Fn &, G, Args...>
    [[nodiscard]] constexpr auto operator()(G && g) && {
        return std::apply(
            [&g, this](Args &... args) {
                return _fn(std::forward<G>(g), std::move(args)...);
            },
            _args);
    }
};

}  // namespace detail

namespace cpo {
namespace detail {
template <typename Graph>
concept can_graph_ref_view =
    requires { graph_ref_view{std::declval<Graph>()}; };

template <typename Graph>
concept can_graph_owning_view =
    requires { graph_owning_view{std::declval<Graph>()}; };
}  // namespace detail

struct graph_all_fn : graph_adaptor_closure<graph_all_fn> {
private:
    // constructible_from as well as graph_view: graph_view<> only asks for
    // movable, so without it an lvalue graph_owning_view -- whose copy
    // constructor is deleted -- takes this branch and hard-errors inside the
    // body. That happens while *checking* `requires { views::graph_all(g); }`,
    // i.e. outside the immediate context, so the candidate cannot even be
    // removed.
    template <typename Graph>
    static constexpr bool pass_through =
        graph_view<std::decay_t<Graph>> &&
        std::constructible_from<std::decay_t<Graph>, Graph>;

    template <typename Graph>
    static constexpr bool is_noexcept() {
        if constexpr(pass_through<Graph>)
            return std::is_nothrow_constructible_v<std::decay_t<Graph>, Graph>;
        else if constexpr(detail::can_graph_ref_view<Graph>)
            // Measured, not assumed: graph_ref_view's converting constructor
            // performs a static_cast<G &> that a user conversion operator can
            // make throwing, so an unconditional `true` is a terminate-bomb.
            return noexcept(graph_ref_view{std::declval<Graph>()});
        else
            return noexcept(graph_owning_view{std::declval<Graph>()});
    }

public:
    // Constrained so that `requires { views::graph_all(g); }` and
    // graph_all_t<G> answer false instead of hard-erroring.
    template <graph Graph>
        requires pass_through<Graph> || detail::can_graph_ref_view<Graph> ||
                 detail::can_graph_owning_view<Graph>
    constexpr auto operator() [[nodiscard]] (Graph && g) const
        noexcept(is_noexcept<Graph>()) {
        if constexpr(pass_through<Graph>)
            return std::decay_t<Graph>(std::forward<Graph>(g));
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

template <typename G, typename Graph>
concept graph_for = std::constructible_from<Graph, views::graph_all_t<G>>;

}  // namespace melon
