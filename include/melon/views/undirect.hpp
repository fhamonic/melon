#pragma once

#include <algorithm>
#include <ranges>

#include "melon/detail/concat_view.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

// graph_view, not graph: see reverse_view -- the constructor routes through
// views::graph_all, so the stored type is always a view, and a non-view
// argument made the class a legal type whose constructor hard-errored.
template <graph_view Graph>
    requires outward_incidence_graph<Graph> && inward_incidence_graph<Graph>
class undirect_view : public undirected_graph_view_base {
private:
    using vertex = vertex_t<Graph>;
    using edge = arc_t<Graph>;

    Graph _graph;

    // What incidence() captures. Its lambdas used to capture `this` (which is
    // what `[&]` does to reach a member), so the ranges they return refer back
    // into the undirect_view object -- and relocating it left reading freed
    // memory, although enable_borrowed_graph below promised the opposite. A
    // copy of the view has no such tie; the graph it names lives elsewhere.
    //
    // The branch is the trait's own condition -- borrowed *and* copyable --
    // not copy_constructible alone. The copy only buys anything where the
    // trait ends up true, and there the wrapped view is a handle (a
    // graph_ref_view is one pointer). A copyable non-borrowed view -- a
    // filtered subgraph carrying its filter maps by value -- was deep-copied
    // into both lambdas on every call, buying a borrowedness the trait
    // (correctly) refused to report; capturing `this` there is free.
    static constexpr bool _lambdas_capture_a_copy =
        enable_borrowed_graph<Graph> && std::copy_constructible<Graph>;

    [[nodiscard]] constexpr auto _capture() const
        noexcept(std::is_nothrow_copy_constructible_v<Graph> ||
                 !_lambdas_capture_a_copy) {
        if constexpr(_lambdas_capture_a_copy)
            return _graph;
        else
            return this;
    }
    [[nodiscard]] static constexpr const Graph & _graph_of(
        const Graph & g) noexcept {
        return g;
    }
    [[nodiscard]] static constexpr const Graph & _graph_of(
        const undirect_view * self) noexcept {
        return self->_graph;
    }

public:
    template <typename G>
        requires detail::not_self<G, undirect_view> && graph_for<G, Graph>
    constexpr explicit undirect_view(G && g)
        : _graph(views::graph_all(std::forward<G>(g))) {}

    // The std adaptor shape (transform_view &co.): default-constructible
    // exactly when the wrapped view is.
    undirect_view()
        requires std::default_initializable<Graph>
    = default;
    constexpr undirect_view(const undirect_view &) = default;
    constexpr undirect_view(undirect_view &&) = default;

    constexpr undirect_view & operator=(const undirect_view &) = default;
    constexpr undirect_view & operator=(undirect_view &&) = default;

    // The adaptor shape, like std::ranges::filter_view: a copy of the adapted
    // directed view from a const lvalue, moved out of an rvalue.
    [[nodiscard]] constexpr Graph base() const &
        requires std::copy_constructible<Graph>
    {
        return _graph;
    }
    [[nodiscard]] constexpr Graph base() && { return std::move(_graph); }

    // Conditional, like num_arcs / num_edges below: this member was the one
    // that carried no specification at all, silently dropping a guarantee
    // static_digraph does give. Constrained on has_num_vertices, the concept
    // graph.hpp already defines, rather than an inline requires-expression.
    [[nodiscard]] constexpr decltype(auto) num_vertices() const
        noexcept(noexcept(melon::num_vertices(_graph)))
        requires has_num_vertices<Graph>
    {
        return melon::num_vertices(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) num_edges() const
        noexcept(noexcept(melon::num_arcs(_graph)))
        requires has_num_arcs<Graph>
    {
        return melon::num_arcs(_graph);
    }

    [[nodiscard]] constexpr decltype(auto) vertices() const
        noexcept(noexcept(melon::vertices(_graph))) {
        return melon::vertices(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) edges() const
        noexcept(noexcept(melon::arcs(_graph))) {
        return melon::arcs(_graph);
    }

    [[nodiscard]] constexpr std::pair<vertex, vertex> edge_endpoints(
        const edge & e) const noexcept(noexcept(melon::arc_source(_graph, e)) &&
                                       noexcept(melon::arc_target(_graph, e)))
        requires has_arc_target<Graph> && has_arc_source<Graph>
    {
        return {melon::arc_source(_graph, e), melon::arc_target(_graph, e)};
    }

    // Not noexcept: builds a concat of two transform_views over the wrapped
    // graph's incidence ranges, each of which may allocate or throw.
    //
    // The lambdas capture _capture() by value rather than `this`: see the
    // helper. Same size for the borrowed case -- a graph_ref_view is one
    // pointer, exactly what `this` was -- and one indirection shorter.
    [[nodiscard]] constexpr decltype(auto) incidence(const vertex & u) const
        requires outward_incidence_graph<Graph> && inward_incidence_graph<Graph>
    {
        return detail::views::concat(
            std::views::transform(
                melon::out_arcs(_graph, u),
                [g = _capture()](auto && a) -> std::pair<edge, vertex> {
                    return {a, melon::arc_target(_graph_of(g), a)};
                }),
            std::views::transform(
                melon::in_arcs(_graph, u),
                [g = _capture()](auto && a) -> std::pair<edge, vertex> {
                    return {a, melon::arc_source(_graph_of(g), a)};
                }));
    }

    template <typename T>
        requires has_vertex_map<Graph>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const
        noexcept(noexcept(melon::create_vertex_map<T>(_graph))) {
        return melon::create_vertex_map<T>(_graph);
    }
    template <typename T>
        requires has_vertex_map<Graph>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        const T & default_value) const
        noexcept(noexcept(melon::create_vertex_map<T>(_graph, default_value))) {
        return melon::create_vertex_map<T>(_graph, default_value);
    }

    template <typename T>
        requires has_arc_map<Graph>
    [[nodiscard]] constexpr decltype(auto) create_edge_map() const
        noexcept(noexcept(melon::create_arc_map<T>(_graph))) {
        return melon::create_arc_map<T>(_graph);
    }
    template <typename T>
        requires has_arc_map<Graph>
    [[nodiscard]] constexpr decltype(auto) create_edge_map(
        const T & default_value) const
        noexcept(noexcept(melon::create_arc_map<T>(_graph, default_value))) {
        return melon::create_arc_map<T>(_graph, default_value);
    }
};

template <typename G>
undirect_view(G &&) -> undirect_view<views::graph_all_t<G>>;

// incidence() builds a concat of transform_views whose lambdas hold a *copy*
// of the wrapped view, so nothing in them refers to the undirect_view object
// itself and it is borrowed exactly when the graph it wraps is. This is the
// same condition as _lambdas_capture_a_copy inside the class, and it must
// stay that way: it is the condition under which the lambdas capture the view
// instead of `this`, and a user specialising the trait for a move-only graph
// handle would otherwise get the promise back without the property.
template <typename G>
inline constexpr bool enable_borrowed_graph<undirect_view<G>> =
    enable_borrowed_graph<G> && std::copy_constructible<G>;

namespace views {

// The adaptor object under the class's old name: views::undirect(g) and
// g | views::undirect build exactly the same undirect_view<...>. See
// views::reverse.
struct undirect_fn : graph_adaptor_closure<undirect_fn> {
    template <typename G>
        requires requires(G && g) { undirect_view(std::forward<G>(g)); }
    [[nodiscard]] constexpr auto operator()(G && g) const {
        return undirect_view(std::forward<G>(g));
    }
};

inline constexpr undirect_fn undirect{};

}  // namespace views
}  // namespace melon
