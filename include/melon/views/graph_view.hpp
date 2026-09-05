#pragma once

#include <algorithm>
#include <concepts>
#include <memory>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"

namespace melon {

struct graph_view_base {};

template <typename T>
inline constexpr bool enable_graph_view = std::derived_from<T, graph_view_base>;

template <typename T>
concept graph_view = graph<T> && std::movable<T> && enable_graph_view<T>;

template <typename T>
concept undirected_graph_view =
    undirected_graph<T> && std::movable<T> && enable_graph_view<T>;

namespace detail {

// The storage every forwarding view is built on: a pointer for
// graph_ref_view, a value for graph_owning_view and the adaptors. The layers
// above forward through wrapped() and inherit these constructors up to the
// view on top, which adds its own and base(), which cannot serve here -- an
// adaptor's base() hands back a *copy* of the wrapped view,
// std::ranges::transform_view style, and only when it is copyable.
template <typename G, typename Stored>
class wrapped_graph : public graph_view_base {
protected:
    Stored _graph;

    wrapped_graph()
        requires std::default_initializable<Stored>
    = default;
    constexpr explicit wrapped_graph(Stored g) noexcept(
        std::is_nothrow_move_constructible_v<Stored>)
        : _graph(std::move(g)) {}

    [[nodiscard]] constexpr const G & wrapped() const noexcept {
        if constexpr(std::is_pointer_v<Stored>)
            return *_graph;
        else
            return _graph;
    }
};

// One definition of each forwarding member, shared by graph_ref_view,
// graph_owning_view, the with_*_maps views, views::reverse, the two protocol
// restrictions below and any user adaptor deriving from the public
// graph_view_interface family. Every accessor forwards straight through; a
// view that means something different by a direction -- views::reverse --
// redeclares just the members it crosses over, and name hiding does the rest.
// That also gets the availability right for free: reverse::out_arcs is
// constrained on has_in_arcs<Graph>, so reversing a graph with no
// in-incidence removes the member rather than silently falling back to the
// base's.
//
// Three layers, one per protocol half, chained by single inheritance rather
// than listed as sibling bases: MSVC lays out only the first of several empty
// bases at offset zero, so siblings would cost a pointer view a second word
// there. The arc and edge layers name arc_t<G> / edge_t<G> in their member
// declarations, which a type without that protocol cannot even spell, so each
// is present in the chain exactly when G models its protocol -- never gated
// member by member.

template <typename G, typename Base>
class vertex_layer : public Base {
private:
    using vertex = vertex_t<G>;

public:
    using Base::Base;

    [[nodiscard]] constexpr decltype(auto) num_vertices() const
        noexcept(noexcept(melon::num_vertices(std::declval<const G &>())))
        requires has_num_vertices<G>
    {
        return melon::num_vertices(this->wrapped());
    }

    [[nodiscard]] constexpr decltype(auto) vertices() const
        noexcept(noexcept(melon::vertices(std::declval<const G &>()))) {
        return melon::vertices(this->wrapped());
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
        return melon::is_valid_vertex(this->wrapped(), v);
    }

    template <typename T, typename Role = default_role>
        requires has_vertex_map<G, T, Role>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const
        noexcept(noexcept(
            melon::create_vertex_map<T, Role>(std::declval<const G &>()))) {
        return melon::create_vertex_map<T, Role>(this->wrapped());
    }
    template <typename T, typename Role = default_role>
        requires has_vertex_map<G, T, Role>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        const T & default_value) const
        noexcept(noexcept(melon::create_vertex_map<T, Role>(
            std::declval<const G &>(), default_value))) {
        return melon::create_vertex_map<T, Role>(this->wrapped(),
                                                 default_value);
    }
};

template <typename G, typename Base>
class arc_layer : public Base {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;

public:
    using Base::Base;

    [[nodiscard]] constexpr decltype(auto) num_arcs() const
        noexcept(noexcept(melon::num_arcs(std::declval<const G &>())))
        requires has_num_arcs<G>
    {
        return melon::num_arcs(this->wrapped());
    }

    [[nodiscard]] constexpr decltype(auto) arcs() const
        noexcept(noexcept(melon::arcs(std::declval<const G &>()))) {
        return melon::arcs(this->wrapped());
    }

    // Delegated to the wrapped graph even when the CPO would synthesise the
    // entries there: synthesising on the *view* instead would capture the view
    // object's address, and a borrowed view's promise -- ranges independent of
    // the view object -- must cover this range too. Delegation aims the
    // synthesis capture at the wrapped graph's storage and hands own entries
    // through untouched.
    [[nodiscard]] constexpr decltype(auto) arcs_entries() const
        noexcept(noexcept(melon::arcs_entries(std::declval<const G &>()))) {
        return melon::arcs_entries(this->wrapped());
    }

    [[nodiscard]] constexpr decltype(auto) is_valid_arc(const arc & a) const
        noexcept(noexcept(melon::is_valid_arc(std::declval<const G &>(), a)))
        requires has_is_valid_arc<G>
    {
        return melon::is_valid_arc(this->wrapped(), a);
    }

    [[nodiscard]] constexpr decltype(auto) out_degree(const vertex & u) const
        noexcept(noexcept(melon::out_degree(std::declval<const G &>(), u)))
        requires has_out_degree<G>
    {
        return melon::out_degree(this->wrapped(), u);
    }
    [[nodiscard]] constexpr decltype(auto) in_degree(const vertex & u) const
        noexcept(noexcept(melon::in_degree(std::declval<const G &>(), u)))
        requires has_in_degree<G>
    {
        return melon::in_degree(this->wrapped(), u);
    }

    [[nodiscard]] constexpr vertex arc_source(const arc & a) const
        noexcept(noexcept(melon::arc_source(std::declval<const G &>(), a)))
        requires has_arc_source<G>
    {
        return melon::arc_source(this->wrapped(), a);
    }
    [[nodiscard]] constexpr vertex arc_target(const arc & a) const
        noexcept(noexcept(melon::arc_target(std::declval<const G &>(), a)))
        requires has_arc_target<G>
    {
        return melon::arc_target(this->wrapped(), a);
    }

    [[nodiscard]] constexpr decltype(auto) arc_sources_map() const
        noexcept(noexcept(melon::arc_sources_map(std::declval<const G &>())))
        requires has_arc_sources_map<G>
    {
        return melon::arc_sources_map(this->wrapped());
    }
    [[nodiscard]] constexpr decltype(auto) arc_targets_map() const
        noexcept(noexcept(melon::arc_targets_map(std::declval<const G &>())))
        requires has_arc_targets_map<G>
    {
        return melon::arc_targets_map(this->wrapped());
    }

    [[nodiscard]] constexpr decltype(auto) out_arcs(const vertex & u) const
        noexcept(noexcept(melon::out_arcs(std::declval<const G &>(), u)))
        requires has_out_arcs<G>
    {
        return melon::out_arcs(this->wrapped(), u);
    }
    [[nodiscard]] constexpr decltype(auto) in_arcs(const vertex & u) const
        noexcept(noexcept(melon::in_arcs(std::declval<const G &>(), u)))
        requires has_in_arcs<G>
    {
        return melon::in_arcs(this->wrapped(), u);
    }

    [[nodiscard]] constexpr decltype(auto) out_neighbors(const vertex & u) const
        noexcept(noexcept(melon::out_neighbors(std::declval<const G &>(), u)))
        requires outward_adjacency_graph<G>
    {
        return melon::out_neighbors(this->wrapped(), u);
    }
    [[nodiscard]] constexpr decltype(auto) in_neighbors(const vertex & u) const
        noexcept(noexcept(melon::in_neighbors(std::declval<const G &>(), u)))
        requires inward_adjacency_graph<G>
    {
        return melon::in_neighbors(this->wrapped(), u);
    }

    template <typename T, typename Role = default_role>
        requires has_arc_map<G, T, Role>
    [[nodiscard]] constexpr decltype(auto) create_arc_map() const noexcept(
        noexcept(melon::create_arc_map<T, Role>(std::declval<const G &>()))) {
        return melon::create_arc_map<T, Role>(this->wrapped());
    }
    template <typename T, typename Role = default_role>
        requires has_arc_map<G, T, Role>
    [[nodiscard]] constexpr decltype(auto) create_arc_map(
        const T & default_value) const
        noexcept(noexcept(melon::create_arc_map<T, Role>(
            std::declval<const G &>(), default_value))) {
        return melon::create_arc_map<T, Role>(this->wrapped(), default_value);
    }
};

template <typename G, typename Base>
class edge_layer : public Base {
private:
    using vertex = vertex_t<G>;
    using edge = edge_t<G>;

public:
    using Base::Base;

    [[nodiscard]] constexpr decltype(auto) num_edges() const
        noexcept(noexcept(melon::num_edges(std::declval<const G &>())))
        requires has_num_edges<G>
    {
        return melon::num_edges(this->wrapped());
    }

    [[nodiscard]] constexpr decltype(auto) edges() const
        noexcept(noexcept(melon::edges(std::declval<const G &>()))) {
        return melon::edges(this->wrapped());
    }

    [[nodiscard]] constexpr std::pair<vertex, vertex> edge_endpoints(
        const edge & e) const
        noexcept(noexcept(melon::edge_endpoints(std::declval<const G &>(),
                                                e))) {
        return melon::edge_endpoints(this->wrapped(), e);
    }

    [[nodiscard]] constexpr decltype(auto) incidence(const vertex & u) const
        noexcept(noexcept(melon::incidence(std::declval<const G &>(), u)))
        requires has_incidence<G>
    {
        return melon::incidence(this->wrapped(), u);
    }

    // Forwarded rather than left to the CPO: the fallback sizes the incidence
    // range through the view, throwing away an O(1) member, and disappears
    // altogether where that range is not sized.
    [[nodiscard]] constexpr decltype(auto) degree(const vertex & u) const
        noexcept(noexcept(melon::degree(std::declval<const G &>(), u)))
        requires has_degree<G>
    {
        return melon::degree(this->wrapped(), u);
    }

    template <typename T, typename Role = default_role>
        requires has_edge_map<G, T, Role>
    [[nodiscard]] constexpr decltype(auto) create_edge_map() const noexcept(
        noexcept(melon::create_edge_map<T, Role>(std::declval<const G &>()))) {
        return melon::create_edge_map<T, Role>(this->wrapped());
    }
    template <typename T, typename Role = default_role>
        requires has_edge_map<G, T, Role>
    [[nodiscard]] constexpr decltype(auto) create_edge_map(
        const T & default_value) const
        noexcept(noexcept(melon::create_edge_map<T, Role>(
            std::declval<const G &>(), default_value))) {
        return melon::create_edge_map<T, Role>(this->wrapped(), default_value);
    }
};

template <typename G, typename Stored>
using vertex_base = vertex_layer<G, wrapped_graph<G, Stored>>;

}  // namespace detail

// The forwarding chains, melon's counterpart of std::ranges::view_interface:
// derive an adaptor from one of them, storing the adapted graph as `Stored`
// -- the graph type itself by default, a pointer for a ref-view shape -- and
// every protocol member the wrapped type has is forwarded; redeclare only
// the members the adaptor changes. The protected `wrapped()` reads the stored
// graph, `_graph` is the storage itself. A layer whose protocol G does not
// model is left out rather than stubbed: the conditionals name the layer
// *type* only, which instantiates nothing.
//
// graph_view_interface forwards every half G models. The two restricted
// chains are for an adaptor that changes the vertex set (the experimental
// add_virtual_vertices): forwarding the other half unchanged there would list
// edges over vertices the adaptor no longer has.
template <has_vertices G, typename Stored = G>
using directed_graph_view_interface =
    detail::arc_layer<G, detail::vertex_base<G, Stored>>;

template <has_vertices G, typename Stored = G>
using undirected_graph_view_interface =
    detail::edge_layer<G, detail::vertex_base<G, Stored>>;

template <has_vertices G, typename Stored = G>
using graph_view_interface = std::conditional_t<
    undirected_graph<G>,
    detail::edge_layer<
        G,
        std::conditional_t<graph<G>, directed_graph_view_interface<G, Stored>,
                           detail::vertex_base<G, Stored>>>,
    std::conditional_t<graph<G>, directed_graph_view_interface<G, Stored>,
                       detail::vertex_base<G, Stored>>>;

namespace detail {}  // namespace detail

// Forwards every half of the protocol the wrapped type models: a type that is
// both a graph and an undirected_graph stays both through the wrapper, and an
// algorithm's constraint -- not the wrapper -- decides which half it reads.
// has_vertices, the one CPO both protocols share, is all the chain needs.
template <has_vertices G>
    requires std::is_object_v<G>
class graph_ref_view : public graph_view_interface<G, G *> {
private:
    using base_type = graph_view_interface<G, G *>;

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
        : base_type(std::addressof(static_cast<G &>(std::forward<T>(g)))) {}

    constexpr graph_ref_view(const graph_ref_view &) = default;
    constexpr graph_ref_view(graph_ref_view &&) = default;

    constexpr graph_ref_view & operator=(const graph_ref_view &) = default;
    constexpr graph_ref_view & operator=(graph_ref_view &&) = default;

    // Shallow const, mirroring std::ranges::ref_view::base(): a const view
    // still hands the graph out mutable.
    [[nodiscard]] constexpr G & base() const noexcept { return *this->_graph; }
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
template <has_vertices G>
    requires std::move_constructible<G> && std::is_object_v<G>
class graph_owning_view : public graph_view_interface<G, G> {
private:
    using base_type = graph_view_interface<G, G>;
    using base_type::_graph;

public:
    constexpr graph_owning_view(G && g) noexcept(
        std::is_nothrow_move_constructible_v<G>)
        : base_type(std::move(g)) {}

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

    // closure | closure, itself a closure.
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

    // The result must not itself be a closure: piping a non-graph into a
    // bound closure re-enters the adaptor's *binding* overload and would
    // "succeed" with a closure over garbage instead of failing at the pipe.
    // Asked of the result rather than of the argument (melon::graph) so the
    // same closure serves the undirected adaptors.
    //
    // Both overloads hand the adaptor *prvalue* copies of the bound
    // arguments (moves, for an rvalue closure), never lvalue references to
    // them: an lvalue would make mapping_all wrap a reference into this
    // closure object, and the view would dangle the moment the closure
    // died. A closure is therefore self-contained, like std's -- so the
    // type it builds does not depend on the closure's value category.
    template <typename G>
        requires std::invocable<const Fn &, G, Args...> &&
                 (!adaptor_closure<
                     std::invoke_result_t<const Fn &, G, Args...>>)
    [[nodiscard]] constexpr auto operator()(G && g) const & {
        return std::apply(
            [&g, this](const Args &... args) {
                return _fn(std::forward<G>(g), Args(args)...);
            },
            _args);
    }
    template <typename G>
        requires std::invocable<const Fn &, G, Args...> &&
                 (!adaptor_closure<
                     std::invoke_result_t<const Fn &, G, Args...>>)
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

// A const rvalue cannot be moved from, so without the exclusion the owning
// branch deduces graph_owning_view<const G> and silently deep-copies -- and
// the result is not even a graph_view (const G is not movable).
// std::views::all pins the precedent: const rvalues are not viewable.
template <typename Graph>
concept can_graph_owning_view =
    (!std::is_const_v<std::remove_reference_t<Graph>>) &&
    requires { graph_owning_view{std::declval<Graph>()}; };
}  // namespace detail

struct graph_all_fn : graph_adaptor_closure<graph_all_fn> {
private:
    // Any melon view passes through, whatever protocol it models -- the two
    // view concepts are not consulted, so a view over a vertices-only type is
    // not wrapped a second time. constructible_from as well: the view concepts
    // only ask for movable, so without it an lvalue graph_owning_view -- whose
    // copy constructor is deleted -- takes this branch and hard-errors inside
    // the body. That happens while *checking* `requires { views::graph_all(g);
    // }`, i.e. outside the immediate context, so the candidate cannot even be
    // removed.
    template <typename Graph>
    static constexpr bool pass_through =
        enable_graph_view<std::decay_t<Graph>> &&
        std::movable<std::decay_t<Graph>> &&
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
    template <typename Graph>
        requires has_vertices<std::remove_cvref_t<Graph>> &&
                 (pass_through<Graph> || detail::can_graph_ref_view<Graph> ||
                  detail::can_graph_owning_view<Graph>)
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

template <typename Graph>
    requires has_vertices<std::remove_cvref_t<Graph>>
using graph_all_t = decltype(graph_all(std::declval<Graph>()));

}  // namespace views

template <typename G, typename Graph>
concept graph_for = std::constructible_from<Graph, views::graph_all_t<G>>;

// ---- Protocol restrictions --------------------------------------------------

// Hides the edge half of a type modelling both protocols, so that only
// `graph` is left to satisfy: what a function template overloaded on
// graph_view and undirected_graph_view needs to stop being ambiguous for such
// a type. Not a conversion -- views::undirect is the one that *makes* edges
// out of arcs.
template <graph_view Graph>
class as_directed_view : public directed_graph_view_interface<Graph, Graph> {
private:
    using base_type = directed_graph_view_interface<Graph, Graph>;
    using base_type::_graph;

public:
    template <typename G>
        requires detail::not_self<G, as_directed_view> && graph_for<G, Graph>
    constexpr explicit as_directed_view(G && g)
        : base_type(views::graph_all(std::forward<G>(g))) {}

    as_directed_view()
        requires std::default_initializable<Graph>
    = default;
    constexpr as_directed_view(const as_directed_view &) = default;
    constexpr as_directed_view(as_directed_view &&) = default;

    constexpr as_directed_view & operator=(const as_directed_view &) = default;
    constexpr as_directed_view & operator=(as_directed_view &&) = default;

    [[nodiscard]] constexpr Graph base() const &
        requires std::copy_constructible<Graph>
    {
        return _graph;
    }
    [[nodiscard]] constexpr Graph base() && { return std::move(_graph); }
};

template <typename G>
as_directed_view(G &&) -> as_directed_view<views::graph_all_t<G>>;

// Only forwards: its ranges are the wrapped graph's own, so it is borrowed
// exactly when that graph is.
template <typename G>
inline constexpr bool enable_borrowed_graph<as_directed_view<G>> =
    enable_borrowed_graph<G>;

// The mirror image: the arc half hidden, `undirected_graph` left.
template <undirected_graph_view Graph>
class as_undirected_view
    : public undirected_graph_view_interface<Graph, Graph> {
private:
    using base_type = undirected_graph_view_interface<Graph, Graph>;
    using base_type::_graph;

public:
    template <typename G>
        requires detail::not_self<G, as_undirected_view> && graph_for<G, Graph>
    constexpr explicit as_undirected_view(G && g)
        : base_type(views::graph_all(std::forward<G>(g))) {}

    as_undirected_view()
        requires std::default_initializable<Graph>
    = default;
    constexpr as_undirected_view(const as_undirected_view &) = default;
    constexpr as_undirected_view(as_undirected_view &&) = default;

    constexpr as_undirected_view & operator=(const as_undirected_view &) =
        default;
    constexpr as_undirected_view & operator=(as_undirected_view &&) = default;

    [[nodiscard]] constexpr Graph base() const &
        requires std::copy_constructible<Graph>
    {
        return _graph;
    }
    [[nodiscard]] constexpr Graph base() && { return std::move(_graph); }
};

template <typename G>
as_undirected_view(G &&) -> as_undirected_view<views::graph_all_t<G>>;

template <typename G>
inline constexpr bool enable_borrowed_graph<as_undirected_view<G>> =
    enable_borrowed_graph<G>;

namespace views {

// The identity on a type that has nothing to hide: wrapping it anyway would
// stack a layer whose only effect is the size of the algorithm storing it.
struct as_directed_fn : graph_adaptor_closure<as_directed_fn> {
    template <typename G>
        requires graph<std::remove_cvref_t<G>> &&
                 requires(G && g) { graph_all(std::forward<G>(g)); }
    [[nodiscard]] constexpr auto operator()(G && g) const {
        if constexpr(undirected_graph<std::remove_cvref_t<G>>)
            return as_directed_view(std::forward<G>(g));
        else
            return graph_all(std::forward<G>(g));
    }
};

inline constexpr as_directed_fn as_directed{};

struct as_undirected_fn : graph_adaptor_closure<as_undirected_fn> {
    template <typename G>
        requires undirected_graph<std::remove_cvref_t<G>> &&
                 requires(G && g) { graph_all(std::forward<G>(g)); }
    [[nodiscard]] constexpr auto operator()(G && g) const {
        if constexpr(graph<std::remove_cvref_t<G>>)
            return as_undirected_view(std::forward<G>(g));
        else
            return graph_all(std::forward<G>(g));
    }
};

inline constexpr as_undirected_fn as_undirected{};

}  // namespace views
}  // namespace melon
