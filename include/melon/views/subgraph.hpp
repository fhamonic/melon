#pragma once

#include <algorithm>
#include <cassert>
#include <ranges>

#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

// graph_view, not graph: see reverse_view -- the constructor routes through
// views::graph_all, so the stored type is always a view, and a non-view
// argument made the class a legal type whose constructor hard-errored.
template <graph_view Graph, mapping<vertex_t<Graph>> VertexFilter,
          mapping<arc_t<Graph>> ArcFilter>
    requires std::convertible_to<mapped_value_t<VertexFilter, vertex_t<Graph>>,
                                 bool> &&
             std::convertible_to<mapped_value_t<ArcFilter, arc_t<Graph>>, bool>
class subgraph_view : public graph_view_base {
private:
    // Private, like every other view's: vertex_t<T> / arc_t<T> are the
    // supported way to name a graph's handle types.
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
    // ("satisfaction of atomic constraint depends on itself"), so every copy of
    // an algorithm holding a subgraph_view failed to compile. not_self exists
    // precisely to cut that off, and only the trailing position lets it.
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

    // The std adaptor shape (filter_view &co.): default-constructible exactly
    // when everything it stores is.
    subgraph_view()
        requires std::default_initializable<Graph> &&
                     std::default_initializable<VertexFilter> &&
                     std::default_initializable<ArcFilter>
    = default;
    constexpr subgraph_view(const subgraph_view &) = default;
    constexpr subgraph_view(subgraph_view &&) = default;

    constexpr subgraph_view & operator=(const subgraph_view &) = default;
    constexpr subgraph_view & operator=(subgraph_view &&) = default;

    // The adaptor shape, like std::ranges::filter_view: a copy of the adapted
    // view from a const lvalue, moved out of an rvalue. subgraph had no way at
    // all to hand back the graph underneath it.
    [[nodiscard]] constexpr Graph base() const &
        requires std::copy_constructible<Graph>
    {
        return _graph;
    }
    [[nodiscard]] constexpr Graph base() && { return std::move(_graph); }

    // This file was missed by the noexcept sweep the rest of the views got:
    // every member below used to be unconditionally noexcept, including the
    // ones that allocate. The rule is the same one applied elsewhere -- a
    // member that merely forwards gets a conditional specification, a member
    // that branches, builds a filter_view or allocates gets none.
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
    // No specification: the body branches and reads a caller-supplied filter.
    //
    // Asks has_is_valid_vertex, not has_vertex_removal. Graph is whatever
    // views::graph_all produced -- a graph_ref_view -- which forwards the
    // validity question but deliberately not remove_vertex, so the old guard
    // was false for every wrapped graph and this branch was dead: a vertex
    // removed from a mutable_digraph came back valid from its subgraph while
    // vertices() had already stopped listing it.
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
    // which is part of the view's value. The `const` also only ever compiled
    // for a mapping_ref_view filter -- with the mapping_owning_view that
    // views::subgraph(g, maps::true_map{}, map) produces, a const member cannot
    // write through it at all.
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
    // See is_valid_vertex.
    [[nodiscard]] constexpr bool is_valid_arc(const arc & a) const {
        if constexpr(has_is_valid_arc<Graph>)
            return melon::is_valid_arc(_graph, a) && _arc_filter[a];
        else
            return _arc_filter[a];
    }

    // Not noexcept: builds a filter_view over a caller-supplied predicate.
    [[nodiscard]] constexpr auto vertices() const {
        if constexpr(std::same_as<VertexFilter, maps::true_map>) {
            return melon::vertices(_graph);
        } else {
            return std::views::filter(
                melon::vertices(_graph),
                [this](const vertex & v) { return _vertex_filter[v]; });
        }
    }
    // Not noexcept: see vertices().
    [[nodiscard]] constexpr auto arcs() const
        requires std::same_as<VertexFilter,
                              maps::true_map>  // if false, use the incidence
                                               // join hierarchy of
                                               // melon::arcs(g)
    {
        if constexpr(std::same_as<ArcFilter, maps::true_map>) {
            return melon::arcs(_graph);
        } else {
            return std::views::filter(
                melon::arcs(_graph),
                [this](const arc & a) { return _arc_filter[a]; });
        }
    }

    // Forwarded only when the wrapped graph carries its *own* arcs_entries,
    // exactly as graph_forwarding_interface does: forwarding the synthesised
    // fallback would stack a transform on a transform. Without this member an
    // entries-only graph wrapped by a subgraph stopped modeling `graph` at
    // all, and a graph with its own entries got the fallback.
    [[nodiscard]] constexpr decltype(auto) arcs_entries() const
        noexcept(noexcept(melon::arcs_entries(_graph)))
        requires cpo::has_own_arcs_entries<Graph> &&
                 std::same_as<VertexFilter, maps::true_map> &&
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
                return _vertex_filter[std::get<1>(entry).first] &&
                       _vertex_filter[std::get<1>(entry).second] &&
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

    // Not noexcept: see vertices().
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
    // Not noexcept: see vertices().
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
            // return arc_filter.filter(melon::out_arcs(_graph, v));
        } else {
            return std::views::filter(
                melon::out_arcs(_graph, v), [this](const arc & a) {
                    return _vertex_filter[melon::arc_target(_graph, a)] &&
                           _arc_filter[a];
                });
        }
    }

    // Forwarded only when no filter can change the answer.
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

    // Not noexcept: see vertices().
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
            return std::views::filter(
                std::views::transform(
                    in_arcs(v),
                    [&](const arc & a) -> vertex { return arc_source(a); }),
                [&](const vertex & u) -> bool { return _vertex_filter[u]; });
        }
    }
    // Not noexcept: see vertices().
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
            return std::views::filter(
                std::views::transform(
                    out_arcs(v),
                    [&](const arc & a) -> vertex { return arc_target(a); }),
                [&](const vertex & u) -> bool { return _vertex_filter[u]; });
        }
    }

    // None of the four below is noexcept: they allocate. The default value
    // is taken by const reference, like every other create_*_map in the
    // library and like the CPO that calls them -- by value cost one extra copy
    // of the default on every call.
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
    // The filter used to be spelled `const mapping_owning_view<...>`. A const
    // member deletes the defaulted assignments, so induced_subgraph failed
    // std::movable and therefore graph_view: views::graph_all stopped passing
    // an rvalue through and wrapped the whole thing in a graph_owning_view.
    // What the const was buying -- a filter the caller cannot poke -- is
    // restored below by hiding the base's enable/disable instead.
    using base_subgraph =
        subgraph_view<Graph, mapping_owning_view<vertex_map_t<Graph, bool>>,
                      maps::true_map>;

    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    vertices_fn _vertices;

    // Static: it reads no member, and it used to be called on *this from a
    // mem-initializer, before any base or member existed.
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

    // By reference, not by value: std::views::all_t of an *rvalue* container is
    // a std::ranges::owning_view, whose copy constructor is deleted, so
    // returning a copy made vertices() ill-formed for
    // `induced_subgraph(g, std::vector{...})`. Construction still compiled --
    // only the concept check failed, silently, and graph<induced_subgraph>
    // came back false.
    // A ref_view over the stored range, not a copy of it: std::views::all_t of
    // an *rvalue* container is a move-only std::ranges::owning_view, so
    // returning a copy made vertices() ill-formed for
    // `induced_subgraph(g, std::vector{...})` -- construction still compiled,
    // only the concept check failed, silently, and graph<induced_subgraph> came
    // back false. Returning `const vertices_fn &` does not work either: a const
    // lvalue of a view type is not a viewable_range, so the arcs() fallback in
    // graph.hpp could no longer transform it. ref_view is both copyable and
    // borrowed whatever the source range was.
    [[nodiscard]] constexpr auto vertices() const noexcept {
        return std::ranges::ref_view(_vertices);
    }
};

template <typename G, typename VR>
induced_subgraph_view(G &&, VR &&)
    -> induced_subgraph_view<views::graph_all_t<G>, std::views::all_t<VR>>;

namespace views {

// The adaptor object under the class's old name. Three spellings, all
// naming the same subgraph_view<...> type: the direct call
// views::subgraph(g, vf, af), the bound form g | views::subgraph(vf, af),
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
    // map by reference, per the library-wide mapping_all rule. Deliberate:
    // converging would mean deep-copying lvalue maps in the direct call,
    // out of step with every other map-taking entry point. Either semantics
    // is spellable in either form -- mapping_ref_view(m) pipes a reference,
    // auto(m) passes the direct call a copy; all four cells are pinned in
    // test/subgraph.cpp and documented in docs/views/graphs.md.
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
