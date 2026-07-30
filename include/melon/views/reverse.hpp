#pragma once

#include <algorithm>
#include <ranges>

#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

// The seven direction-free members -- num_vertices, num_arcs, vertices, arcs
// and the four create_*_map overloads -- come from
// detail::graph_forwarding_interface, which graph_ref_view and
// graph_owning_view share. All that is left here is the eight accessors whose
// meaning actually crosses over, redeclared so they hide the base's.
// graph_view, not graph: the constructor routes through views::graph_all, so
// the stored type is always a view -- naming reverse_view<static_digraph> used
// to be a legal *type* whose constructor then hard-errored in the
// mem-initializer. std::ranges pins the precedent: transform_view<V, F>
// requires view<V>. The deduction guides below only ever produce graph_all_t
// types, so no factory-built code changes.
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
        requires detail::not_self<G, reverse_view> &&
                 graph_storable_as<G, Graph>
    constexpr explicit reverse_view(G && g)
        : _graph(detail::store_graph<Graph>(std::forward<G>(g))) {}

    constexpr reverse_view(const reverse_view &) = default;
    constexpr reverse_view(reverse_view &&) = default;

    constexpr reverse_view & operator=(const reverse_view &) = default;
    constexpr reverse_view & operator=(reverse_view &&) = default;

    // The adaptor shape, like std::ranges::filter_view / transform_view: hand
    // back a *copy* of the adapted view, available from a const lvalue only
    // when that copy is possible, and move it out of an rvalue. It used to
    // return `const Graph &`, which is the owning_view shape and belongs to a
    // view that stores its range for the caller rather than adapting it.
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
    // every arc's endpoints the wrong way round. Only reachable when the
    // wrapped graph carries its own arcs_entries -- otherwise there is no
    // member here and the CPO synthesises the entries from *this* view's
    // arc_source / arc_target, which are already swapped.
    [[nodiscard]] constexpr auto arcs_entries() const
        requires melon::cpo::has_own_arcs_entries<Graph>
    {
        return std::views::transform(
            melon::arcs_entries(_graph), [](auto && entry) {
                return std::make_pair(
                    entry.first,
                    std::make_pair(entry.second.second, entry.second.first));
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

// The adaptor object under the class's old name, so every existing call
// spelling still works: views::reverse(g) and g | views::reverse build
// exactly the same reverse_view<...> -- the pipe costs nothing by
// construction. The class/object split is the std::ranges shape
// (reverse_view / views::reverse).
struct reverse_fn : graph_adaptor_closure<reverse_fn> {
    template <typename G>
        requires requires(G && g) { reverse_view(std::forward<G>(g)); }
    [[nodiscard]] constexpr auto operator()(G && g) const {
        return reverse_view(std::forward<G>(g));
    }
};

inline constexpr reverse_fn reverse{};

}  // namespace views
}  // namespace melon
