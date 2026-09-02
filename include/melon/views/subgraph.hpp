#pragma once

#include <algorithm>
#include <cassert>
#include <ranges>
#include <type_traits>
#include <utility>

#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/maps/constant.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

// graph_view, not graph: the constructor routes through views::graph_all, so
// the stored type is always a view -- a non-view argument would make the class
// a legal *type* whose constructor hard-errors in the mem-initializer.
template <graph_view Graph, mapping<vertex_t<Graph>> VertexFilter,
          mapping<arc_t<Graph>> ArcFilter>
    requires std::convertible_to<mapped_value_t<VertexFilter, vertex_t<Graph>>,
                                 bool> &&
             std::convertible_to<mapped_value_t<ArcFilter, arc_t<Graph>>, bool>
class subgraph_view : public graph_view_base {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    Graph _graph;
    [[no_unique_address]] VertexFilter _vertex_filter;
    [[no_unique_address]] ArcFilter _arc_filter;

public:
    // not_self goes first, and the storability checks stay in the trailing
    // requires-clause rather than riding on the template parameters. A
    // constrained parameter's constraint is conjoined *before* the trailing
    // clause, so `template <graph_for<Graph> G> requires not_self<...>` checks
    // graph_for first -- and graph_for<G, Graph> asks
    // constructible_from<Graph, graph_all_t<G>>, whose pass_through branch asks
    // constructible_from<subgraph_view, subgraph_view> for G = subgraph_view:
    // the very question being answered. GCC rejects that outright
    // ("satisfaction of atomic constraint depends on itself"), so copying an
    // algorithm that holds a subgraph_view fails to compile. Only the trailing
    // position lets not_self cut that off.
    template <typename G, typename VF = maps::true_map,
              typename AF = maps::true_map>
        requires detail::not_self<G, subgraph_view> && graph_for<G, Graph> &&
                     mapping_for<VF, VertexFilter> && mapping_for<AF, ArcFilter>
    constexpr explicit subgraph_view(G && g,
                                     VF && vertex_filter = maps::true_map{},
                                     AF && arc_filter = maps::true_map{})
        : _graph(views::graph_all(std::forward<G>(g)))
        , _vertex_filter(maps::mapping_all(std::forward<VF>(vertex_filter)))
        , _arc_filter(maps::mapping_all(std::forward<AF>(arc_filter))) {}

    subgraph_view()
        requires std::default_initializable<Graph> &&
                     std::default_initializable<VertexFilter> &&
                     std::default_initializable<ArcFilter>
    = default;
    constexpr subgraph_view(const subgraph_view &) = default;
    constexpr subgraph_view(subgraph_view &&) = default;

    constexpr subgraph_view & operator=(const subgraph_view &) = default;
    constexpr subgraph_view & operator=(subgraph_view &&) = default;

    [[nodiscard]] constexpr Graph base() const &
        requires std::copy_constructible<Graph>
    {
        return _graph;
    }
    [[nodiscard]] constexpr Graph base() && { return std::move(_graph); }

    // The noexcept rule for everything below: a member that merely forwards
    // gets a conditional specification, a member that branches, builds a
    // filter_view or allocates gets none.
    [[nodiscard]] constexpr decltype(auto) num_vertices() const
        noexcept(noexcept(melon::num_vertices(_graph)))
        requires has_num_vertices<Graph> &&
                 std::same_as<VertexFilter, maps::true_map>
    {
        return melon::num_vertices(_graph);
    }
    // Both filters, not just the vertex one: an arc filter changes the arc
    // count too.
    [[nodiscard]] constexpr decltype(auto) num_arcs() const
        noexcept(noexcept(melon::num_arcs(_graph)))
        requires has_num_arcs<Graph> &&
                 std::same_as<VertexFilter, maps::true_map> &&
                 std::same_as<ArcFilter, maps::true_map>
    {
        return melon::num_arcs(_graph);
    }

    constexpr void disable_vertex(const vertex & v) noexcept(
        noexcept(_vertex_filter[v] = false))
        requires output_mapping_of<VertexFilter, vertex_t<Graph>, bool>
    {
        _vertex_filter[v] = false;
    }
    constexpr void enable_vertex(const vertex & v) noexcept(
        noexcept(_vertex_filter[v] = true))
        requires output_mapping_of<VertexFilter, vertex_t<Graph>, bool>
    {
        _vertex_filter[v] = true;
    }
    // Asks has_is_valid_vertex, not has_vertex_removal. Graph is whatever
    // views::graph_all produced -- a graph_ref_view -- which forwards the
    // validity question but deliberately not remove_vertex, so a removal-based
    // guard is false for every wrapped graph and leaves this branch dead: a
    // vertex removed from a mutable_digraph then comes back valid from its
    // subgraph while vertices() has already stopped listing it.
    //
    // The underlying graph goes first: _vertex_filter is indexed by vertex, so
    // subscripting it with a handle the graph no longer recognises is exactly
    // the out-of-range access the check exists to prevent.
    [[nodiscard]] constexpr bool is_valid_vertex(const vertex & v) const {
        if constexpr(has_is_valid_vertex<Graph>)
            return melon::is_valid_vertex(_graph, v) && _vertex_filter[v];
        else
            return _vertex_filter[v];
    }

    // Non-const, like disable_vertex / enable_vertex: these write the filter,
    // which is part of the view's value. A const member could only ever write
    // through a mapping_ref_view filter, never through the mapping_owning_view
    // that views::subgraph(g, maps::true_map{}, map) produces.
    constexpr void disable_arc(const arc & a) noexcept(
        noexcept(_arc_filter[a] = false))
        requires output_mapping_of<ArcFilter, arc_t<Graph>, bool>
    {
        _arc_filter[a] = false;
    }
    constexpr void enable_arc(const arc & a) noexcept(
        noexcept(_arc_filter[a] = true))
        requires output_mapping_of<ArcFilter, arc_t<Graph>, bool>
    {
        _arc_filter[a] = true;
    }
    // The underlying graph goes first, as in is_valid_vertex: subscripting
    // _arc_filter with a handle the graph no longer recognises is the
    // out-of-range access the check exists to prevent.
    [[nodiscard]] constexpr bool is_valid_arc(const arc & a) const {
        if constexpr(has_is_valid_arc<Graph>)
            return melon::is_valid_arc(_graph, a) && _arc_filter[a];
        else
            return _arc_filter[a];
    }

    [[nodiscard]] constexpr auto vertices() const {
        if constexpr(std::same_as<VertexFilter, maps::true_map>) {
            return melon::vertices(_graph);
        } else {
            return std::views::filter(
                melon::vertices(_graph),
                [this](const vertex & v) { return _vertex_filter[v]; });
        }
    }
    // Absent under a vertex filter: filtering melon::arcs(_graph) by the arc
    // filter alone keeps arcs whose ends are filtered out. Without this
    // member, melon::arcs(g) falls back to detail::join_incidence over
    // out_arcs, which does see the vertex filter.
    [[nodiscard]] constexpr auto arcs() const
        requires std::same_as<VertexFilter, maps::true_map>
    {
        if constexpr(std::same_as<ArcFilter, maps::true_map>) {
            return melon::arcs(_graph);
        } else {
            return std::views::filter(
                melon::arcs(_graph),
                [this](const arc & a) { return _arc_filter[a]; });
        }
    }

    // Delegated to the wrapped graph even when the CPO would synthesise the
    // entries there: synthesising on the *view* would capture this object's
    // address, which the filterless specialisation's borrowed promise below
    // forbids. The filtered twin keeps the own-entries requirement -- it is
    // never borrowed, and filtering the base's own entries is what lets an
    // entries-only graph survive filtering at all.
    [[nodiscard]] constexpr decltype(auto) arcs_entries() const
        noexcept(noexcept(melon::arcs_entries(_graph)))
        requires std::same_as<VertexFilter, maps::true_map> &&
                 std::same_as<ArcFilter, maps::true_map>
    {
        return melon::arcs_entries(_graph);
    }
    // The filtered twin: the entry names the arc and both endpoints, which is
    // everything the filters need -- so filtering the base's own entries also
    // works for a graph with no endpoint accessors at all, where the CPO's
    // synthesis is impossible.
    [[nodiscard]] constexpr auto arcs_entries() const
        requires cpo::has_own_arcs_entries<Graph> &&
                 (!std::same_as<VertexFilter, maps::true_map> ||
                  !std::same_as<ArcFilter, maps::true_map>)
    {
        return std::views::filter(
            melon::arcs_entries(_graph), [this](const auto & entry) {
                return _vertex_filter[std::get<0>(std::get<1>(entry))] &&
                       _vertex_filter[std::get<1>(std::get<1>(entry))] &&
                       _arc_filter[std::get<0>(entry)];
            });
    }

    [[nodiscard]] constexpr auto arc_source(const arc & a) const
        noexcept(noexcept(melon::arc_source(_graph, a)))
        requires has_arc_source<Graph>
    {
        assert(is_valid_arc(a));
        return melon::arc_source(_graph, a);
    }
    [[nodiscard]] constexpr auto arc_sources_map() const
        noexcept(noexcept(melon::arc_sources_map(_graph)))
        requires has_arc_sources_map<Graph>
    {
        return melon::arc_sources_map(_graph);
    }
    [[nodiscard]] constexpr auto arc_target(const arc & a) const
        noexcept(noexcept(melon::arc_target(_graph, a)))
        requires has_arc_target<Graph>
    {
        assert(is_valid_arc(a));
        return melon::arc_target(_graph, a);
    }
    [[nodiscard]] constexpr auto arc_targets_map() const
        noexcept(noexcept(melon::arc_targets_map(_graph)))
        requires has_arc_targets_map<Graph>
    {
        return melon::arc_targets_map(_graph);
    }

    [[nodiscard]] constexpr auto in_arcs(const vertex & v) const
        requires inward_incidence_graph<Graph>
    {
        assert(is_valid_vertex(v));
        if constexpr(std::same_as<VertexFilter, maps::true_map> &&
                     std::same_as<ArcFilter, maps::true_map>) {
            return melon::in_arcs(_graph, v);
        } else if constexpr(std::same_as<VertexFilter, maps::true_map>) {
            return std::views::filter(
                melon::in_arcs(_graph, v),
                [this](const arc & a) { return _arc_filter[a]; });
        } else {
            return std::views::filter(
                melon::in_arcs(_graph, v), [this](const arc & a) {
                    return _vertex_filter[melon::arc_source(_graph, a)] &&
                           _arc_filter[a];
                });
        }
    }
    [[nodiscard]] constexpr auto out_arcs(const vertex & v) const
        requires outward_incidence_graph<Graph>
    {
        assert(is_valid_vertex(v));
        if constexpr(std::same_as<VertexFilter, maps::true_map> &&
                     std::same_as<ArcFilter, maps::true_map>) {
            return melon::out_arcs(_graph, v);
        } else if constexpr(std::same_as<VertexFilter, maps::true_map>) {
            return std::views::filter(
                melon::out_arcs(_graph, v),
                [this](const arc & a) { return _arc_filter[a]; });
        } else {
            return std::views::filter(
                melon::out_arcs(_graph, v), [this](const arc & a) {
                    return _vertex_filter[melon::arc_target(_graph, a)] &&
                           _arc_filter[a];
                });
        }
    }

    [[nodiscard]] constexpr decltype(auto) out_degree(const vertex & v) const
        noexcept(noexcept(melon::out_degree(_graph, v)))
        requires has_out_degree<Graph> &&
                 std::same_as<VertexFilter, maps::true_map> &&
                 std::same_as<ArcFilter, maps::true_map>
    {
        assert(is_valid_vertex(v));
        return melon::out_degree(_graph, v);
    }
    [[nodiscard]] constexpr decltype(auto) in_degree(const vertex & v) const
        noexcept(noexcept(melon::in_degree(_graph, v)))
        requires has_in_degree<Graph> &&
                 std::same_as<VertexFilter, maps::true_map> &&
                 std::same_as<ArcFilter, maps::true_map>
    {
        assert(is_valid_vertex(v));
        return melon::in_degree(_graph, v);
    }

    [[nodiscard]] constexpr auto in_neighbors(const vertex & v) const
        requires inward_adjacency_graph<Graph>
    {
        assert(is_valid_vertex(v));
        if constexpr(std::same_as<VertexFilter, maps::true_map> &&
                     std::same_as<ArcFilter, maps::true_map>) {
            return melon::in_neighbors(_graph, v);
        } else if constexpr(std::same_as<ArcFilter, maps::true_map>) {
            return std::views::filter(
                melon::in_neighbors(_graph, v),
                [this](const vertex & u) { return _vertex_filter[u]; });
        } else {
            // No vertex filter on top: the both-filters in_arcs above already
            // rejected arcs whose source fails it.
            return std::views::transform(
                in_arcs(v),
                [&](const arc & a) -> vertex { return arc_source(a); });
        }
    }
    [[nodiscard]] constexpr auto out_neighbors(const vertex & v) const
        requires outward_adjacency_graph<Graph>
    {
        assert(is_valid_vertex(v));
        if constexpr(std::same_as<VertexFilter, maps::true_map> &&
                     std::same_as<ArcFilter, maps::true_map>) {
            return melon::out_neighbors(_graph, v);
        } else if constexpr(std::same_as<ArcFilter, maps::true_map>) {
            return std::views::filter(
                melon::out_neighbors(_graph, v),
                [&](const vertex & u) { return _vertex_filter[u]; });
        } else {
            // No vertex filter on top: the both-filters out_arcs above
            // already rejected arcs whose target fails it.
            return std::views::transform(
                out_arcs(v),
                [&](const arc & a) -> vertex { return arc_target(a); });
        }
    }

    template <typename T>
        requires has_vertex_map<Graph>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const {
        return melon::create_vertex_map<T>(_graph);
    }
    template <typename T>
        requires has_vertex_map<Graph>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        const T & default_value) const {
        return melon::create_vertex_map<T>(_graph, default_value);
    }

    template <typename T>
        requires has_arc_map<Graph>
    [[nodiscard]] constexpr decltype(auto) create_arc_map() const {
        return melon::create_arc_map<T>(_graph);
    }
    template <typename T>
        requires has_arc_map<Graph>
    [[nodiscard]] constexpr decltype(auto) create_arc_map(
        const T & default_value) const {
        return melon::create_arc_map<T>(_graph, default_value);
    }
};

template <typename G, typename VF = maps::true_map,
          typename AF = maps::true_map>
subgraph_view(G &&, VF && = {}, AF && = {})
    -> subgraph_view<views::graph_all_t<G>, maps::mapping_all_t<VF>,
                     maps::mapping_all_t<AF>>;

// Only the filterless case: with a filter present, every range member is a
// filter_view capturing `this`, which is exactly what borrowed_graph.hpp
// names as the reason subgraph cannot be borrowed. With both filters
// maps::true_map every range member forwards straight through, so the view is
// borrowed exactly when the wrapped view is -- graph_ref_view yes,
// graph_owning_view no.
template <typename G, typename VF, typename AF>
inline constexpr bool enable_borrowed_graph<subgraph_view<G, VF, AF>> =
    std::same_as<VF, maps::true_map> && std::same_as<AF, maps::true_map> &&
    enable_borrowed_graph<G>;

template <graph Graph, std::ranges::viewable_range vertices_fn>
    requires std::convertible_to<std::ranges::range_value_t<vertices_fn>,
                                 vertex_t<Graph>> &&
             has_vertex_map<Graph>
class induced_subgraph_view
    : public subgraph_view<Graph,
                           mapping_owning_view<vertex_map_t<Graph, bool>>,
                           maps::true_map> {
private:
    // The filter must not be const-qualified here: a const member deletes the
    // defaulted assignments, so induced_subgraph_view fails std::movable and
    // therefore graph_view, and views::graph_all stops passing an rvalue
    // through and wraps the whole thing in a graph_owning_view. A filter the
    // caller cannot poke comes from hiding the base's enable/disable instead.
    using base_subgraph =
        subgraph_view<Graph, mapping_owning_view<vertex_map_t<Graph, bool>>,
                      maps::true_map>;

    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    vertices_fn _vertices;

    // Static: it reads no member, and it runs from a mem-initializer, before
    // any base or member exists.
    template <typename G, typename VR>
    [[nodiscard]] static constexpr auto construct_vertex_filter(
        const G & g, VR && vertices_range) {
        auto filter = melon::create_vertex_map<bool>(g, false);
        for(const auto & v : vertices_range) filter[v] = true;
        return filter;
    }

    // The filter and _vertices are two spellings of one vertex set: letting a
    // caller flip a bit in the filter would desync them, and vertices() would
    // keep naming the vertex the graph no longer has.
    using base_subgraph::disable_vertex;
    using base_subgraph::enable_vertex;

public:
    // The two arguments below are indeterminately sequenced, but harmlessly
    // so: subgraph's constructor takes the graph by forwarding reference, so
    // std::forward is a cast and nothing is moved out of g until the base's
    // own mem-initializer runs -- strictly after both arguments are evaluated.
    template <typename G, typename VR>
    constexpr explicit induced_subgraph_view(G && g, VR && vertices_range)
        : base_subgraph(std::forward<G>(g),
                        construct_vertex_filter(g, vertices_range), {})
        , _vertices(std::views::all(std::forward<VR>(vertices_range))) {}

    induced_subgraph_view()
        requires std::default_initializable<base_subgraph> &&
                     std::default_initializable<vertices_fn>
    = default;
    constexpr induced_subgraph_view(const induced_subgraph_view &) = default;
    constexpr induced_subgraph_view(induced_subgraph_view &&) = default;

    constexpr induced_subgraph_view & operator=(const induced_subgraph_view &) =
        default;
    constexpr induced_subgraph_view & operator=(induced_subgraph_view &&) =
        default;

    // A ref_view over the stored range, not a copy of it: std::views::all_t of
    // an *rvalue* container is a move-only std::ranges::owning_view, so
    // returning a copy makes vertices() ill-formed for
    // `induced_subgraph(g, std::vector{...})` -- construction still compiles,
    // only the concept check fails, silently, and graph<induced_subgraph> comes
    // back false. Returning `const vertices_fn &` does not work either: a const
    // lvalue of a view type is not a viewable_range, so the arcs() fallback in
    // graph.hpp can no longer transform it. ref_view is both copyable and
    // borrowed whatever the source range was.
    [[nodiscard]] constexpr auto vertices() const noexcept {
        return std::ranges::ref_view(_vertices);
    }
};

template <typename G, typename VR>
induced_subgraph_view(G &&, VR &&)
    -> induced_subgraph_view<views::graph_all_t<G>, std::views::all_t<VR>>;

namespace views {

// Three spellings, all naming the same subgraph_view<...> type: the direct
// call views::subgraph(g, vf, af), the bound form g | views::subgraph(vf, af),
// and -- since every filter is optional -- the nullary g | views::subgraph().
// Not itself a closure: `g | views::subgraph` without the parentheses is
// ill-formed, exactly as `r | std::views::filter` is.
struct subgraph_fn {
    // Defaulted template parameters, exactly like subgraph_view's
    // constructor: a braced `{}` argument deduces nothing, falls back to
    // maps::true_map, and keeps `views::subgraph(g, {}, af)` working.
    template <typename G, typename VF = maps::true_map,
              typename AF = maps::true_map>
        requires graph<std::remove_cvref_t<G>> &&
                 requires(G && g, VF && vf, AF && af) {
                     subgraph_view(std::forward<G>(g), std::forward<VF>(vf),
                                   std::forward<AF>(af));
                 }
    [[nodiscard]] constexpr auto operator()(
        G && g, VF && vertex_filter = maps::true_map{},
        AF && arc_filter = maps::true_map{}) const {
        return subgraph_view(std::forward<G>(g),
                             std::forward<VF>(vertex_filter),
                             std::forward<AF>(arc_filter));
    }

    // The bound form: no graph yet, hold the filters and wait for one. The
    // filters are decay-*copied* into the closure (adaptor_partial): a
    // closure must be self-contained or it dangles. This is the one place
    // the two spellings diverge -- the direct call above stores an lvalue
    // map by reference, per the library-wide mapping_all rule. Converging
    // them would mean deep-copying lvalue maps in the direct call, out of
    // step with every other map-taking entry point. Either semantics is
    // spellable in either form -- mapping_ref_view(m) pipes a reference,
    // auto(m) passes the direct call a copy; see docs/views/graphs.md.
    template <typename VF = maps::true_map, typename AF = maps::true_map>
        requires(!graph<std::remove_cvref_t<VF>>)
    [[nodiscard]] constexpr auto operator()(
        VF && vertex_filter = maps::true_map{},
        AF && arc_filter = maps::true_map{}) const {
        return detail::adaptor_partial<subgraph_fn, std::decay_t<VF>,
                                       std::decay_t<AF>>(
            std::forward<VF>(vertex_filter), std::forward<AF>(arc_filter));
    }
};

inline constexpr subgraph_fn subgraph{};

// See views::subgraph; the vertex range is not optional, so there is no
// nullary form.
struct induced_subgraph_fn {
    template <typename G, typename VR>
        requires graph<std::remove_cvref_t<G>> &&
                 requires(G && g, VR && vertices_range) {
                     induced_subgraph_view(std::forward<G>(g),
                                           std::forward<VR>(vertices_range));
                 }
    [[nodiscard]] constexpr auto operator()(G && g,
                                            VR && vertices_range) const {
        return induced_subgraph_view(std::forward<G>(g),
                                     std::forward<VR>(vertices_range));
    }

    template <typename VR>
        requires(!graph<std::remove_cvref_t<VR>>)
    [[nodiscard]] constexpr auto operator()(VR && vertices_range) const {
        return detail::adaptor_partial<induced_subgraph_fn, std::decay_t<VR>>(
            std::forward<VR>(vertices_range));
    }
};

inline constexpr induced_subgraph_fn induced_subgraph{};

}  // namespace views
}  // namespace melon
