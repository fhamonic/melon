#pragma once

// EXPERIMENTAL — ships and is tested, but carries no stability guarantee:
// anything in melon::experimental may change or disappear in any release,
// including a patch release.

// Augments a graph with one virtual root vertex and one virtual arc from the
// root to each vertex of a given sources range, without copying the graph:
// vertices become 0..n (root = n), arcs become 0..m+k-1 (virtual arc of the
// i-th source = m+i), and the map factories size their maps to cover the
// virtual elements. This is the supersource construction of multi-source
// flow problems, lifted to the graph layer so the consuming algorithm can
// stay augmentation-free.
//
// Constrained to integral vertex and arc ids, which extend by arithmetic; a
// generic id type would need a variant-shaped id and tagged maps, which is a
// different design. Ids must be dense -- every vertex id below num_vertices,
// every arc id below num_arcs -- which is also what makes the static_map
// factories below correct; the container digraphs satisfy it, a filtered
// view does not.

#include <cassert>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <type_traits>
#include <utility>

#include "melon/container/static_map.hpp"
#include "melon/detail/concat_view.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/graph.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {
namespace experimental {

template <graph_view Graph, std::ranges::view Sources>
    requires std::integral<vertex_t<Graph>> && std::integral<arc_t<Graph>> &&
             has_num_vertices<Graph> && has_num_arcs<Graph> &&
             std::ranges::random_access_range<Sources> &&
             std::ranges::sized_range<Sources> &&
             std::convertible_to<std::ranges::range_value_t<Sources>,
                                 vertex_t<Graph>>
class unify_sources_view : public detail::graph_forwarding_interface<
                               unify_sources_view<Graph, Sources>, Graph> {
private:
    friend detail::graph_forwarding_interface<
        unify_sources_view<Graph, Sources>, Graph>;

    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    // The virtual in-arc map only answers inward questions, so it only
    // exists where the base graph has an inward feature to forward; for an
    // out-only graph it would be n slots of dead weight.
    static constexpr bool _forwards_inward_feature =
        has_in_arcs<Graph> || has_in_degree<Graph> ||
        inward_adjacency_graph<Graph>;
    using virtual_in_arc_map =
        detail::vertex_map_if<_forwards_inward_feature, Graph, arc>;

    Graph _graph;
    Sources _sources;
    // The virtual in-arc of each source, or the one-past-the-end sentinel for
    // a non-source: what lets in_arcs / in_degree / in_neighbors answer in
    // O(1) instead of scanning the sources range on every call.
    [[no_unique_address]] virtual_in_arc_map _virtual_in_arc;

    [[nodiscard]] constexpr const Graph & _forwarding_base() const noexcept {
        return _graph;
    }

    [[nodiscard]] constexpr arc _first_virtual_arc() const {
        return static_cast<arc>(melon::num_arcs(_graph));
    }
    [[nodiscard]] constexpr arc _virtual_arc_sentinel() const {
        return static_cast<arc>(
            static_cast<std::size_t>(melon::num_arcs(_graph)) +
            std::ranges::size(_sources));
    }
    [[nodiscard]] constexpr vertex _source_at(const std::size_t i) const {
        return static_cast<vertex>(
            _sources[static_cast<std::ranges::range_difference_t<Sources>>(i)]);
    }

public:
    template <typename G, typename S>
        requires graph_for<G, Graph> &&
                     std::constructible_from<Sources, std::views::all_t<S>>
    constexpr unify_sources_view(G && g, S && sources)
        : _graph(melon::views::graph_all(std::forward<G>(g)))
        , _sources(std::views::all(std::forward<S>(sources)))
        , _virtual_in_arc(_graph, _virtual_arc_sentinel()) {
        [[maybe_unused]] arc e = _first_virtual_arc();
        for(const auto & s : _sources) {
            [[maybe_unused]] const vertex v = static_cast<vertex>(s);
            assert(static_cast<std::size_t>(v) <
                   static_cast<std::size_t>(melon::num_vertices(_graph)));
            if constexpr(_forwards_inward_feature) {
                // A duplicate source would need two virtual in-arcs on one
                // vertex, which the map cannot answer; with no inward feature
                // to forward, duplicates are harmless parallel arcs.
                assert(_virtual_in_arc[v] == _virtual_arc_sentinel());
                _virtual_in_arc[v] = e;
                ++e;
            }
        }
    }

    unify_sources_view()
        requires std::default_initializable<Graph> &&
                     std::default_initializable<Sources> &&
                     std::default_initializable<virtual_in_arc_map>
    = default;
    constexpr unify_sources_view(const unify_sources_view &) = default;
    constexpr unify_sources_view(unify_sources_view &&) = default;

    constexpr unify_sources_view & operator=(const unify_sources_view &) =
        default;
    constexpr unify_sources_view & operator=(unify_sources_view &&) = default;

    [[nodiscard]] constexpr Graph base() const &
        requires std::copy_constructible<Graph>
    {
        return _graph;
    }
    [[nodiscard]] constexpr Graph base() && { return std::move(_graph); }

    // ---- The augmentation's own vocabulary ----------------------------------

    [[nodiscard]] constexpr vertex root() const {
        return static_cast<vertex>(melon::num_vertices(_graph));
    }
    // Aligned with the sources range: the i-th virtual arc goes from the root
    // to the i-th source.
    [[nodiscard]] constexpr auto virtual_arcs() const {
        return std::views::iota(_first_virtual_arc(), _virtual_arc_sentinel());
    }
    [[nodiscard]] constexpr const Sources & sources() const noexcept {
        return _sources;
    }

    // ---- graph interface ----------------------------------------------------

    [[nodiscard]] constexpr auto num_vertices() const {
        return static_cast<std::size_t>(melon::num_vertices(_graph)) + 1;
    }
    [[nodiscard]] constexpr auto num_arcs() const {
        return static_cast<std::size_t>(_virtual_arc_sentinel());
    }

    // Dense integral ids are the class precondition, so the id ranges ARE the
    // element ranges; delegating and concatenating would only obscure that.
    [[nodiscard]] constexpr auto vertices() const {
        return std::views::iota(vertex(0), static_cast<vertex>(num_vertices()));
    }
    [[nodiscard]] constexpr auto arcs() const {
        return std::views::iota(arc(0), _virtual_arc_sentinel());
    }

    [[nodiscard]] constexpr vertex arc_source(const arc & a) const
        requires has_arc_source<Graph>
    {
        return a < _first_virtual_arc() ? melon::arc_source(_graph, a) : root();
    }
    [[nodiscard]] constexpr vertex arc_target(const arc & a) const
        requires has_arc_target<Graph>
    {
        return a < _first_virtual_arc() ? melon::arc_target(_graph, a)
                                        : _source_at(static_cast<std::size_t>(
                                              a - _first_virtual_arc()));
    }

    [[nodiscard]] constexpr auto arcs_entries() const {
        return detail::views::concat(
            std::views::transform(
                melon::arcs_entries(_graph),
                [](auto && entry) {
                    return std::make_pair(
                        static_cast<arc>(std::get<0>(entry)),
                        std::make_pair(static_cast<vertex>(
                                           std::get<0>(std::get<1>(entry))),
                                       static_cast<vertex>(
                                           std::get<1>(std::get<1>(entry)))));
                }),
            std::views::transform(
                std::views::iota(
                    std::size_t{0},
                    static_cast<std::size_t>(std::ranges::size(_sources))),
                [this](const std::size_t i) {
                    return std::make_pair(
                        static_cast<arc>(
                            static_cast<std::size_t>(_first_virtual_arc()) + i),
                        std::make_pair(root(), _source_at(i)));
                }));
    }

    [[nodiscard]] constexpr bool is_valid_vertex(const vertex & u) const
        requires has_is_valid_vertex<Graph>
    {
        return u == root() || melon::is_valid_vertex(_graph, u);
    }
    [[nodiscard]] constexpr bool is_valid_arc(const arc & a) const
        requires has_is_valid_arc<Graph>
    {
        return a < _first_virtual_arc() ? melon::is_valid_arc(_graph, a)
                                        : a < _virtual_arc_sentinel();
    }

    // The incidence ranges concatenate the wrapped graph's range with an
    // always-present virtual piece, empty where it does not apply: one branch
    // per call, one range type for every vertex. The base piece must be
    // default-constructible to stand in empty at the root, where the wrapped
    // graph cannot be consulted at all.
    [[nodiscard]] constexpr auto out_arcs(const vertex & u) const
        requires has_out_arcs<Graph> &&
                 std::default_initializable<
                     std::views::all_t<out_arcs_range_t<Graph>>>
    {
        using base_range = std::views::all_t<out_arcs_range_t<Graph>>;
        const arc first = _first_virtual_arc();
        if(u == root())
            return detail::views::concat(
                base_range(), std::views::iota(first, _virtual_arc_sentinel()));
        return detail::views::concat(
            std::views::all(melon::out_arcs(_graph, u)),
            std::views::iota(first, first));
    }
    [[nodiscard]] constexpr auto in_arcs(const vertex & u) const
        requires has_in_arcs<Graph> &&
                 std::default_initializable<
                     std::views::all_t<in_arcs_range_t<Graph>>>
    {
        using base_range = std::views::all_t<in_arcs_range_t<Graph>>;
        const arc sentinel = _virtual_arc_sentinel();
        if(u == root())
            return detail::views::concat(base_range(),
                                         std::views::iota(sentinel, sentinel));
        const arc lo = _virtual_in_arc[u];
        const arc hi = (lo == sentinel) ? sentinel : static_cast<arc>(lo + 1);
        return detail::views::concat(std::views::all(melon::in_arcs(_graph, u)),
                                     std::views::iota(lo, hi));
    }

    [[nodiscard]] constexpr auto out_neighbors(const vertex & u) const
        requires outward_adjacency_graph<Graph> &&
                 std::default_initializable<
                     std::views::all_t<decltype(melon::out_neighbors(
                         std::declval<const Graph &>(),
                         std::declval<const vertex &>()))>>
    {
        using base_range = std::views::all_t<decltype(melon::out_neighbors(
            std::declval<const Graph &>(), std::declval<const vertex &>()))>;
        const std::size_t num_virtual =
            u == root() ? static_cast<std::size_t>(std::ranges::size(_sources))
                        : std::size_t{0};
        auto virtual_part = std::views::transform(
            std::views::iota(std::size_t{0}, num_virtual),
            [this](const std::size_t i) { return _source_at(i); });
        if(u == root())
            return detail::views::concat(base_range(), std::move(virtual_part));
        return detail::views::concat(
            std::views::all(melon::out_neighbors(_graph, u)),
            std::move(virtual_part));
    }
    [[nodiscard]] constexpr auto in_neighbors(const vertex & u) const
        requires inward_adjacency_graph<Graph> &&
                 std::default_initializable<
                     std::views::all_t<decltype(melon::in_neighbors(
                         std::declval<const Graph &>(),
                         std::declval<const vertex &>()))>>
    {
        using base_range = std::views::all_t<decltype(melon::in_neighbors(
            std::declval<const Graph &>(), std::declval<const vertex &>()))>;
        const vertex r = root();
        if(u == r)
            return detail::views::concat(base_range(), std::views::iota(r, r));
        const vertex extra = (_virtual_in_arc[u] != _virtual_arc_sentinel())
                                 ? vertex(1)
                                 : vertex(0);
        return detail::views::concat(
            std::views::all(melon::in_neighbors(_graph, u)),
            std::views::iota(r, static_cast<vertex>(r + extra)));
    }

    [[nodiscard]] constexpr auto out_degree(const vertex & u) const
        requires has_out_degree<Graph>
    {
        return u == root()
                   ? static_cast<std::size_t>(std::ranges::size(_sources))
                   : static_cast<std::size_t>(melon::out_degree(_graph, u));
    }
    [[nodiscard]] constexpr auto in_degree(const vertex & u) const
        requires has_in_degree<Graph>
    {
        if(u == root()) return std::size_t{0};
        return static_cast<std::size_t>(melon::in_degree(_graph, u)) +
               (_virtual_in_arc[u] != _virtual_arc_sentinel() ? std::size_t{1}
                                                              : std::size_t{0});
    }

    // Sized by the augmented id spaces, which the density precondition makes
    // exactly the key spaces; the wrapped graph's own factories cannot be
    // extended, so they are not consulted.
    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map() const {
        return melon::static_map<vertex, T>(num_vertices());
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map(
        const T & default_value) const {
        return melon::static_map<vertex, T>(num_vertices(), default_value);
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_arc_map() const {
        return melon::static_map<arc, T>(num_arcs());
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_arc_map(const T & default_value) const {
        return melon::static_map<arc, T>(num_arcs(), default_value);
    }
};

template <typename G, typename S>
unify_sources_view(G &&, S &&)
    -> unify_sources_view<melon::views::graph_all_t<G>, std::views::all_t<S>>;

namespace views {

struct unify_sources_fn {
    template <typename G, typename S>
        requires requires(G && g, S && s) {
            unify_sources_view(std::forward<G>(g), std::forward<S>(s));
        }
    [[nodiscard]] constexpr auto operator()(G && g, S && sources) const {
        return unify_sources_view(std::forward<G>(g), std::forward<S>(sources));
    }
    // The bound form for pipe syntax: g | views::unify_sources(sources).
    template <typename S>
        requires(!graph<std::remove_cvref_t<S>>) && std::ranges::range<S>
    [[nodiscard]] constexpr auto operator()(S && sources) const {
        return melon::views::detail::adaptor_partial<unify_sources_fn,
                                                     std::decay_t<S>>(
            std::forward<S>(sources));
    }
};

inline constexpr unify_sources_fn unify_sources{};

}  // namespace views
}  // namespace experimental
}  // namespace melon
