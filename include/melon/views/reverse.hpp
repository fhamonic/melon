#pragma once

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <utility>

#include "melon/detail/not_self.hpp"
#include "melon/detail/specialization_of.hpp"
#include "melon/graph.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

// graph_view, not graph: the constructor routes through views::graph_all, so
// the stored type is always a view -- a non-view argument would make
// reverse_view<static_digraph> a legal *type* whose constructor hard-errors in
// the mem-initializer. std::ranges pins the precedent: transform_view<V, F>
// requires view<V>.
template <graph_view Graph>
class reverse_view
    : public detail::graph_forwarding_interface<reverse_view<Graph>, Graph> {
private:
    friend detail::graph_forwarding_interface<reverse_view<Graph>, Graph>;

    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    Graph _graph;

    [[nodiscard]] constexpr const Graph & _forwarding_base() const noexcept {
        return _graph;
    }

public:
    template <typename G>
        requires detail::not_self<G, reverse_view> && graph_for<G, Graph>
    constexpr explicit reverse_view(G && g)
        : _graph(views::graph_all(std::forward<G>(g))) {}

    reverse_view()
        requires std::default_initializable<Graph>
    = default;
    constexpr reverse_view(const reverse_view &) = default;
    constexpr reverse_view(reverse_view &&) = default;

    constexpr reverse_view & operator=(const reverse_view &) = default;
    constexpr reverse_view & operator=(reverse_view &&) = default;

    // The adaptor shape, like std::ranges::filter_view / transform_view: hand
    // back a *copy* of the adapted view, available from a const lvalue only
    // when that copy is possible, and move it out of an rvalue. Not
    // `const Graph &` -- that is the owning_view shape, which belongs to a view
    // that stores its range for the caller rather than adapting it.
    [[nodiscard]] constexpr Graph base() const &
        requires std::copy_constructible<Graph>
    {
        return _graph;
    }
    [[nodiscard]] constexpr Graph base() && { return std::move(_graph); }

    [[nodiscard]] constexpr vertex arc_source(const arc & a) const
        noexcept(noexcept(melon::arc_target(_graph, a)))
        requires has_arc_target<Graph>
    {
        return melon::arc_target(_graph, a);
    }
    [[nodiscard]] constexpr vertex arc_target(const arc & a) const
        noexcept(noexcept(melon::arc_source(_graph, a)))
        requires has_arc_source<Graph>
    {
        return melon::arc_source(_graph, a);
    }

    [[nodiscard]] constexpr decltype(auto) arc_sources_map() const
        noexcept(noexcept(melon::arc_targets_map(_graph)))
        requires has_arc_targets_map<Graph>
    {
        return melon::arc_targets_map(_graph);
    }
    [[nodiscard]] constexpr decltype(auto) arc_targets_map() const
        noexcept(noexcept(melon::arc_sources_map(_graph)))
        requires has_arc_sources_map<Graph>
    {
        return melon::arc_sources_map(_graph);
    }

    [[nodiscard]] constexpr decltype(auto) out_arcs(const vertex & u) const
        noexcept(noexcept(melon::in_arcs(_graph, u)))
        requires has_in_arcs<Graph>
    {
        return melon::in_arcs(_graph, u);
    }
    [[nodiscard]] constexpr decltype(auto) in_arcs(const vertex & u) const
        noexcept(noexcept(melon::out_arcs(_graph, u)))
        requires has_out_arcs<Graph>
    {
        return melon::out_arcs(_graph, u);
    }

    [[nodiscard]] constexpr decltype(auto) out_neighbors(const vertex & u) const
        noexcept(noexcept(melon::in_neighbors(_graph, u)))
        requires inward_adjacency_graph<Graph>
    {
        return melon::in_neighbors(_graph, u);
    }
    [[nodiscard]] constexpr decltype(auto) in_neighbors(const vertex & u) const
        noexcept(noexcept(melon::out_neighbors(_graph, u)))
        requires outward_adjacency_graph<Graph>
    {
        return melon::out_neighbors(_graph, u);
    }

    [[nodiscard]] constexpr decltype(auto) out_degree(const vertex & u) const
        noexcept(noexcept(melon::in_degree(_graph, u)))
        requires has_in_degree<Graph>
    {
        return melon::in_degree(_graph, u);
    }
    [[nodiscard]] constexpr decltype(auto) in_degree(const vertex & u) const
        noexcept(noexcept(melon::out_degree(_graph, u)))
        requires has_out_degree<Graph>
    {
        return melon::out_degree(_graph, u);
    }

    // Crossed over like the eight above, and for the same reason: the base's
    // version forwards the wrapped graph's entries untouched, which would name
    // every arc's endpoints the wrong way round. Unconditional, unlike the
    // interface's member: letting the CPO synthesise on *this* view instead
    // would capture the view's address and break the borrowed promise
    // forwarded below, where delegating to the wrapped graph aims the capture
    // at storage that outlives any copy of the view.
    [[nodiscard]] constexpr auto arcs_entries() const
        noexcept(noexcept(melon::arcs_entries(_graph))) {
        return std::views::transform(
            melon::arcs_entries(_graph), [](auto && entry) {
                return std::make_pair(
                    std::get<0>(entry),
                    std::make_pair(std::get<1>(std::get<1>(entry)),
                                   std::get<0>(std::get<1>(entry))));
            });
    }
};

template <typename G>
reverse_view(G &&) -> reverse_view<views::graph_all_t<G>>;

// reverse_view only forwards: its ranges are the wrapped graph's own, so it
// is borrowed exactly when that graph is.
template <typename G>
inline constexpr bool enable_borrowed_graph<reverse_view<G>> =
    enable_borrowed_graph<G>;

namespace views {

struct reverse_fn : graph_adaptor_closure<reverse_fn> {
    template <typename G>
        requires(!melon::detail::specialization_of<std::remove_cvref_t<G>,
                                                   reverse_view>) &&
                requires(G && g) { reverse_view(std::forward<G>(g)); }
    [[nodiscard]] constexpr auto operator()(G && g) const {
        return reverse_view(std::forward<G>(g));
    }
    // std::views::reverse's unwrap rule: reversing a reverse_view hands back
    // the graph it adapts instead of wrapping twice.
    template <typename G>
        requires melon::detail::specialization_of<std::remove_cvref_t<G>,
                                                  reverse_view> &&
                 requires(G && g) { std::forward<G>(g).base(); }
    [[nodiscard]] constexpr auto operator()(G && g) const {
        return std::forward<G>(g).base();
    }
};

inline constexpr reverse_fn reverse{};

}  // namespace views
}  // namespace melon
