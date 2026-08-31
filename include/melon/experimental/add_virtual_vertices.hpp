#pragma once

// EXPERIMENTAL — ships and is tested, but carries no stability guarantee:
// anything in melon::experimental may change or disappear in any release,
// including a patch release.

// Augments a graph with `count` virtual vertices, without copying the graph
// and without touching its arcs: the virtual ids extend the vertex id space
// past its largest id, the vertex-map factory sizes its maps to cover them,
// and every virtual vertex has empty incidence. This is the artificial-
// vertex half of supersource/supersink constructions, lifted to the graph
// layer; virtual arcs are a separate concern (unify_sources bundles a root
// with per-source arcs).
//
// Constrained to integral vertex ids, which extend by arithmetic; a generic
// id type would need a variant-shaped id and tagged maps, which is a
// different design, deliberately deferred. Ids must be non-negative but need
// not be dense: the first virtual id is one past the largest vertex id,
// found by an O(n) pass at construction, and the vertex maps span
// [0, largest id + count] -- a fragmented id space costs memory in
// proportion, exactly as the graph's own maps do. A freed id above every
// live one can be re-minted as a virtual id: distinctness is from the LIVE
// vertices, which is all any consumer may hold. Arc ids are untouched and
// carry no constraint.
//
// Stacking two of these composes with no special handling: the outer view's
// construction pass sees the inner's virtual ids and mints past them.

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>

#include "melon/container/static_map.hpp"
#include "melon/detail/concat_view.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {
namespace experimental {

template <graph_view Graph>
    requires std::integral<vertex_t<Graph>>
class add_virtual_vertices_view : public detail::graph_forwarding_interface<
                                      add_virtual_vertices_view<Graph>, Graph> {
private:
    friend detail::graph_forwarding_interface<add_virtual_vertices_view<Graph>,
                                              Graph>;

    using vertex = vertex_t<Graph>;

    Graph _graph;
    vertex _first_virtual = vertex(0);
    std::size_t _count = 0;

    [[nodiscard]] constexpr const Graph & _forwarding_base() const noexcept {
        return _graph;
    }
    [[nodiscard]] constexpr vertex _virtual_sentinel() const {
        return static_cast<vertex>(_first_virtual +
                                   static_cast<vertex>(_count));
    }

public:
    // not_self first: without it, asking whether the view is constructible
    // from its own lvalue (as graph_all's pass-through test does when two of
    // these stack) re-enters graph_for through graph_all_t, and constraint
    // satisfaction depends on itself.
    template <typename G>
        requires detail::not_self<G, add_virtual_vertices_view> &&
                     graph_for<G, Graph>
    constexpr explicit add_virtual_vertices_view(G && g,
                                                 const std::size_t count = 1)
        : _graph(melon::views::graph_all(std::forward<G>(g))), _count(count) {
        for(auto && v : melon::vertices(_graph)) {
            if constexpr(std::is_signed_v<vertex>) assert(v >= vertex(0));
            assert(
                static_cast<std::size_t>(v) <
                static_cast<std::size_t>(std::numeric_limits<vertex>::max()) -
                    count);
            _first_virtual =
                std::max(_first_virtual, static_cast<vertex>(v + 1));
        }
    }

    add_virtual_vertices_view()
        requires std::default_initializable<Graph>
    = default;
    constexpr add_virtual_vertices_view(const add_virtual_vertices_view &) =
        default;
    constexpr add_virtual_vertices_view(add_virtual_vertices_view &&) = default;

    constexpr add_virtual_vertices_view & operator=(
        const add_virtual_vertices_view &) = default;
    constexpr add_virtual_vertices_view & operator=(
        add_virtual_vertices_view &&) = default;

    // Reference-returning like graph_owning_view, not value-returning like
    // unify_sources: an algorithm that stores this view must hand the
    // wrapped graph back by reference through its own base(), which a value
    // return cannot do for a move-only owning wrapper.
    [[nodiscard]] constexpr Graph & base() & noexcept { return _graph; }
    [[nodiscard]] constexpr const Graph & base() const & noexcept {
        return _graph;
    }
    [[nodiscard]] constexpr Graph && base() && noexcept {
        return std::move(_graph);
    }
    [[nodiscard]] constexpr const Graph && base() const && noexcept {
        return std::move(_graph);
    }

    // ---- The augmentation's own vocabulary ----------------------------------

    [[nodiscard]] constexpr std::size_t num_virtual_vertices() const noexcept {
        return _count;
    }
    [[nodiscard]] constexpr auto virtual_vertices() const {
        return std::views::iota(_first_virtual, _virtual_sentinel());
    }
    [[nodiscard]] constexpr vertex virtual_vertex(const std::size_t i) const {
        assert(i < _count);
        return static_cast<vertex>(_first_virtual + static_cast<vertex>(i));
    }

    // ---- graph interface ----------------------------------------------------

    [[nodiscard]] constexpr auto num_vertices() const
        requires has_num_vertices<Graph>
    {
        return static_cast<std::size_t>(melon::num_vertices(_graph)) + _count;
    }

    // The base ids need not be dense, so the base range is enumerated as-is
    // and the virtual piece concatenated -- unlike unify_sources, whose
    // density precondition lets one iota cover everything.
    [[nodiscard]] constexpr auto vertices() const {
        return detail::views::concat(
            std::views::all(melon::vertices(_graph)),
            std::views::iota(_first_virtual, _virtual_sentinel()));
    }

    [[nodiscard]] constexpr bool is_valid_vertex(const vertex & u) const
        requires has_is_valid_vertex<Graph>
    {
        return (_first_virtual <= u && u < _virtual_sentinel()) ||
               melon::is_valid_vertex(_graph, u);
    }

    // A virtual vertex answers its empty incidence as a default-constructed
    // base range: one return type for every vertex. Default-construction
    // meaning empty is a property the graphs' ranges uphold -- iota and
    // pointer subranges value-initialize empty, and mutable_digraph's
    // intrusive iterators default to at-end for exactly this idiom. An id
    // below _first_virtual that is not a live vertex is forwarded, where it
    // is out of the base's own contract.
    [[nodiscard]] constexpr auto out_arcs(const vertex & u) const
        requires has_out_arcs<Graph> &&
                 std::default_initializable<
                     std::views::all_t<out_arcs_range_t<Graph>>>
    {
        using range = std::views::all_t<out_arcs_range_t<Graph>>;
        if(u >= _first_virtual) {
            assert(u < _virtual_sentinel());
            return range();
        }
        return std::views::all(melon::out_arcs(_graph, u));
    }
    [[nodiscard]] constexpr auto in_arcs(const vertex & u) const
        requires has_in_arcs<Graph> &&
                 std::default_initializable<
                     std::views::all_t<in_arcs_range_t<Graph>>>
    {
        using range = std::views::all_t<in_arcs_range_t<Graph>>;
        if(u >= _first_virtual) {
            assert(u < _virtual_sentinel());
            return range();
        }
        return std::views::all(melon::in_arcs(_graph, u));
    }
    [[nodiscard]] constexpr auto out_neighbors(const vertex & u) const
        requires outward_adjacency_graph<Graph> &&
                 std::default_initializable<
                     std::views::all_t<decltype(melon::out_neighbors(
                         std::declval<const Graph &>(),
                         std::declval<const vertex &>()))>>
    {
        using range = std::views::all_t<decltype(melon::out_neighbors(
            std::declval<const Graph &>(), std::declval<const vertex &>()))>;
        if(u >= _first_virtual) {
            assert(u < _virtual_sentinel());
            return range();
        }
        return std::views::all(melon::out_neighbors(_graph, u));
    }
    [[nodiscard]] constexpr auto in_neighbors(const vertex & u) const
        requires inward_adjacency_graph<Graph> &&
                 std::default_initializable<
                     std::views::all_t<decltype(melon::in_neighbors(
                         std::declval<const Graph &>(),
                         std::declval<const vertex &>()))>>
    {
        using range = std::views::all_t<decltype(melon::in_neighbors(
            std::declval<const Graph &>(), std::declval<const vertex &>()))>;
        if(u >= _first_virtual) {
            assert(u < _virtual_sentinel());
            return range();
        }
        return std::views::all(melon::in_neighbors(_graph, u));
    }

    [[nodiscard]] constexpr auto out_degree(const vertex & u) const
        requires has_out_degree<Graph>
    {
        if(u >= _first_virtual) {
            assert(u < _virtual_sentinel());
            return std::size_t{0};
        }
        return static_cast<std::size_t>(melon::out_degree(_graph, u));
    }
    [[nodiscard]] constexpr auto in_degree(const vertex & u) const
        requires has_in_degree<Graph>
    {
        if(u >= _first_virtual) {
            assert(u < _virtual_sentinel());
            return std::size_t{0};
        }
        return static_cast<std::size_t>(melon::in_degree(_graph, u));
    }

    // Sized by the extended id bound [0, largest id + count]: the wrapped
    // graph's own factory cannot be extended, so it is not consulted -- which
    // also grants vertex maps over a base that has none. Arc maps forward.
    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map() const {
        return melon::static_map<vertex, T>(
            static_cast<std::size_t>(_virtual_sentinel()));
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map(
        const T & default_value) const {
        return melon::static_map<vertex, T>(
            static_cast<std::size_t>(_virtual_sentinel()), default_value);
    }
};

template <typename G>
add_virtual_vertices_view(G &&)
    -> add_virtual_vertices_view<melon::views::graph_all_t<G>>;
template <typename G>
add_virtual_vertices_view(G &&, std::size_t)
    -> add_virtual_vertices_view<melon::views::graph_all_t<G>>;

namespace views {

struct add_virtual_vertices_fn {
    template <typename G>
        requires requires(G && g) {
            add_virtual_vertices_view(std::forward<G>(g));
        }
    [[nodiscard]] constexpr auto operator()(G && g,
                                            const std::size_t count = 1) const {
        return add_virtual_vertices_view(std::forward<G>(g), count);
    }
    // The bound form for pipe syntax: g | views::add_virtual_vertices(2).
    template <typename C>
        requires(!graph<std::remove_cvref_t<C>>) &&
                std::convertible_to<C, std::size_t>
    [[nodiscard]] constexpr auto operator()(const C & count) const {
        return melon::views::detail::adaptor_partial<add_virtual_vertices_fn,
                                                     std::size_t>(
            static_cast<std::size_t>(count));
    }
};

inline constexpr add_virtual_vertices_fn add_virtual_vertices{};

}  // namespace views
}  // namespace experimental
}  // namespace melon
