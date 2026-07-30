#pragma once

#include <algorithm>
#include <concepts>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "melon/detail/borrowed_graph.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"

namespace melon {

struct graph_view_base {};

template <typename T>
inline constexpr bool enable_graph_view = std::derived_from<T, graph_view_base>;

template <typename T>
concept graph_view = graph<T> && std::movable<T> && enable_graph_view<T>;

namespace detail {

// The sixteen members that graph_ref_view, graph_owning_view and
// views::reverse each used to spell out in full: three near-identical
// hundred-line blocks whose only differences were how the wrapped graph is
// reached and, for reverse, which direction each accessor means.
//
// Keeping them apart is what let the 1.0.0 noexcept sweep touch 67 members
// across four files and still miss one -- num_vertices was the single member of
// every view carrying no specification at all, three lines above a num_arcs
// that carried a conditional one. Here there is one definition of each.
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
    // Private here, as on every melon graph type: vertex_t<T> / arc_t<T> are
    // the supported way to name a graph's handle types.
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
    // when the CPO would synthesise one: synthesising it here and forwarding
    // *that* would stack a transform on a transform for every container in the
    // library, all of which reach arcs_entries through the fallback.
    // Without this member at all, `graph<G>` -- which is defined in terms of
    // arcs_entries -- came back false for every view over a graph whose
    // arcs_entries is its only arc protocol, so views::graph_all silently
    // turned such a graph into a non-graph and every algorithm rejected it.
    // It also downgraded mutable_digraph's own arcs_entries to the fallback.
    [[nodiscard]] constexpr decltype(auto) arcs_entries() const
        noexcept(noexcept(melon::arcs_entries(std::declval<const G &>())))
        requires cpo::has_own_arcs_entries<G>
    {
        return melon::arcs_entries(_wrapped());
    }

    // A view is read-only, so it forwards the *question* without forwarding
    // remove_vertex / remove_arc. views::subgraph consults these to decide
    // whether a handle is still live in the graph underneath it; with them
    // missing it could only ever consult its own filter, and reported a vertex
    // removed from a mutable_digraph as valid.
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

    // The default value goes in by const reference, like the CPO that forwards
    // it here; by value cost one extra copy per call, which is what four of
    // these eight signatures used to do across the four view headers.
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

// A handle to a graph that lives elsewhere. Every accessor comes from
// graph_forwarding_interface; all this class adds is the pointer.
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

    // The std::ranges::ref_view bindable-test, exactly as mapping_ref_view
    // spells it: convertible_to<T, G &> alone accepted a temporary whose
    // conversion sequence materialises a G (a type with both operator G&()
    // and operator G()), leaving _graph aimed at an object that dies at the
    // end of the full-expression.
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

// The same accessors over a graph the view owns.
// is_object_v alongside move_constructible: move_constructible<G &> is true,
// so without it graph_owning_view<SD &> was a legal *type* whose only
// constructor then hard-errored -- the class name promised owning while the
// parameter could not be owned. mapping_owning_view already carries the same
// conjunct.
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

    // The four overloads of std::ranges::owning_view::base(): the const &&
    // one was missing, so `std::move(std::as_const(v)).base()` bound to the
    // const & overload and handed back an lvalue reference.
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
    // re-entered subgraph_fn's *binding* overload and "succeeded" with a
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
    // The constructible_from is what maps::mapping_all_fn already carries and
    // this one did not: graph_view<> only asks for movable, so an lvalue
    // graph_owning_view -- whose copy constructor is deleted -- took this
    // branch and hard-errored inside the body. That happens while *checking*
    // `requires { views::graph_all(g); }`, i.e. outside the immediate context,
    // so the candidate could not even be removed.
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
            // make throwing, so an unconditional `true` was a terminate-bomb.
            // mapping_all_fn already spells it this way.
            return noexcept(graph_ref_view{std::declval<Graph>()});
        else
            return noexcept(graph_owning_view{std::declval<Graph>()});
    }

public:
    // Constrained for the same reason mapping_all_fn is: so that
    // `requires { views::graph_all(g); }` and graph_all_t<G> answer false
    // instead of hard-erroring.
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

// The graph twin of mapping_storable_as (mapping.hpp): "this constructor
// argument can become the stored member", through views::graph_all -- the
// ref-or-owning view types CTAD deduces. Stored member types are always graph
// views (the algorithms' class heads require it, like the view adaptors), so
// there is no direct-construction fallback: value ownership is spelled
// graph_owning_view. See mapping_storable_as for why the constructors must be
// constrained on it.
template <typename G, typename Stored>
concept graph_storable_as =
    requires(G && g) { views::graph_all(std::forward<G>(g)); } &&
    std::constructible_from<Stored, views::graph_all_t<G>>;

namespace detail {

// The construction graph_storable_as promises. Fully qualified: inside
// melon::detail a bare `views::` finds melon::detail::views (the concat
// shim), not melon::views.
template <typename Stored, typename G>
    requires graph_storable_as<G, Stored>
[[nodiscard]] constexpr Stored store_graph(G && g) {
    return Stored(melon::views::graph_all(std::forward<G>(g)));
}

}  // namespace detail

}  // namespace melon
