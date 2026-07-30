#undef NDEBUG
#include <gtest/gtest.h>

#include <utility>

#include "melon/algorithm/dijkstra.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/reverse.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// views::reverse wraps like graph_all: ref view for lvalues, owning view for
// rvalues, and the result is a graph_view
////////////////////////////////////////////////////////////////////////////////

template <typename G>
using trivial_subgraph_t = decltype(views::reverse(std::declval<G>()));

GTEST_TEST(reverse_views, graph_view) {
    using G = static_digraph;

    static_assert(
        std::same_as<trivial_subgraph_t<G &>, reverse_view<graph_ref_view<G>>>);
    static_assert(std::same_as<trivial_subgraph_t<const G &>,
                               reverse_view<graph_ref_view<const G>>>);
    static_assert(std::same_as<trivial_subgraph_t<G>,
                               reverse_view<graph_owning_view<G>>>);
    static_assert(std::same_as<trivial_subgraph_t<G &&>,
                               reverse_view<graph_owning_view<G>>>);

    static_assert(graph_view<trivial_subgraph_t<G &>>);
    static_assert(graph_view<trivial_subgraph_t<const G &>>);
    static_assert(graph_view<trivial_subgraph_t<G>>);
    static_assert(graph_view<trivial_subgraph_t<G &&>>);
}

////////////////////////////////////////////////////////////////////////////////
// reversing swaps out- and in- adjacency and leaves everything else untouched
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(reverse_views, static_graph) {
    std::vector<
        std::pair<arc_t<static_digraph>, std::pair<vertex_t<static_digraph>,
                                                   vertex_t<static_digraph>>>>
        arc_pairs(
            {{0, {0, 1}}, {1, {0, 2}}, {2, {1, 2}}, {3, {2, 0}}, {4, {2, 1}}});

    static_digraph graph(3, std::views::keys(std::views::values(arc_pairs)),
                         std::views::values(std::views::values(arc_pairs)));

    auto reverse_graph = views::reverse(graph);

    ASSERT_EQ(num_vertices(graph), reverse_graph.num_vertices());
    ASSERT_EQ(num_arcs(graph), reverse_graph.num_arcs());

    ASSERT_TRUE(EQ_RANGES(vertices(graph), reverse_graph.vertices()));
    ASSERT_TRUE(EQ_RANGES(arcs(graph), reverse_graph.arcs()));

    for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph, u));
    ASSERT_FALSE(
        is_valid_vertex(graph, vertex_t<static_digraph>(num_vertices(graph))));

    for(auto a : arcs(graph)) ASSERT_TRUE(is_valid_arc(graph, a));
    ASSERT_FALSE(is_valid_arc(graph, arc_t<static_digraph>(num_arcs(graph))));

    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph, 0), reverse_graph.in_neighbors(0)));
    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph, 1), reverse_graph.in_neighbors(1)));
    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph, 2), reverse_graph.in_neighbors(2)));

    ASSERT_TRUE(
        EQ_RANGES(in_neighbors(graph, 0), reverse_graph.out_neighbors(0)));
    ASSERT_TRUE(
        EQ_RANGES(in_neighbors(graph, 1), reverse_graph.out_neighbors(1)));
    ASSERT_TRUE(
        EQ_RANGES(in_neighbors(graph, 2), reverse_graph.out_neighbors(2)));

    ASSERT_TRUE(EQ_RANGES(arcs_entries(graph), arc_pairs));

    for(arc_t<static_digraph> a : arcs(graph)) {
        ASSERT_EQ(arc_source(graph, a), arc_pairs[a].second.first);
    }
}

////////////////////////////////////////////////////////////////////////////////
// the endpoint maps are the wrapped graph's own, swapped -- not synthesised
////////////////////////////////////////////////////////////////////////////////

// regression 2.2: `reverse` spelled its endpoint maps `sources_map()` /
// `targets_map()`, names no CPO ever looks for, so
// arc_sources_map(reverse_view) fell back to the synthesised lambda instead of
// handing back the wrapped graph's *targets* map. Values stayed right -- the
// fallback calls arc_source, which reverse does swap correctly -- so only the
// return type tells the two apart.
GTEST_TEST(reverse_views, endpoint_maps_are_swapped_and_not_synthesised) {
    using G = static_digraph;

    const std::vector<std::pair<unsigned int, unsigned int>> arc_pairs(
        {{0u, 1u}, {0u, 2u}, {1u, 2u}, {2u, 0u}, {2u, 1u}});
    const G graph(3, std::views::keys(arc_pairs),
                  std::views::values(arc_pairs));

    const auto reverse_graph = views::reverse(graph);
    using R = std::remove_cvref_t<decltype(reverse_graph)>;

    // the members exist under the names the CPOs probe, ...
    static_assert(melon::cpo::has_member_arc_sources_map<R>);
    static_assert(melon::cpo::has_member_arc_targets_map<R>);
    // ... and they hand back the wrapped graph's own maps, crosswise
    static_assert(
        std::same_as<decltype(arc_sources_map(std::declval<const R &>())),
                     decltype(arc_targets_map(std::declval<const G &>()))>);
    static_assert(
        std::same_as<decltype(arc_targets_map(std::declval<const R &>())),
                     decltype(arc_sources_map(std::declval<const G &>()))>);

    const auto sources = arc_sources_map(reverse_graph);
    const auto targets = arc_targets_map(reverse_graph);
    for(const arc_t<G> a : arcs(graph)) {
        ASSERT_EQ(sources[a], arc_target(graph, a));
        ASSERT_EQ(targets[a], arc_source(graph, a));
        ASSERT_EQ(sources[a], arc_source(reverse_graph, a));
        ASSERT_EQ(targets[a], arc_target(reverse_graph, a));
    }
}

////////////////////////////////////////////////////////////////////////////////
// algorithms consume the reversed view like any graph
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(reverse_views, dijkstra) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 7);
    builder.add_arc(0, 2, 9);
    builder.add_arc(0, 5, 14);
    builder.add_arc(1, 0, 7);
    builder.add_arc(1, 2, 10);
    builder.add_arc(1, 3, 15);
    builder.add_arc(2, 0, 9);
    builder.add_arc(2, 1, 10);
    builder.add_arc(2, 3, 12);
    builder.add_arc(2, 5, 2);
    builder.add_arc(3, 1, 15);
    builder.add_arc(3, 2, 12);
    builder.add_arc(3, 4, 6);
    builder.add_arc(4, 3, 6);
    builder.add_arc(4, 5, 9);
    builder.add_arc(5, 0, 14);
    builder.add_arc(5, 2, 2);
    builder.add_arc(5, 4, 9);

    auto [fgraph, length_map] = builder.build();
    auto graph = views::reverse(fgraph);

    dijkstra alg(graph, length_map);

    alg.add_source(0);
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(0u, 0));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(1u, 7));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(2u, 9));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(5u, 11));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(4u, 20));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(3u, 21));
    alg.advance();
    ASSERT_TRUE(alg.finished());
}

////////////////////////////////////////////////////////////////////////////////
// copying a mutable lvalue view uses the copy constructor
////////////////////////////////////////////////////////////////////////////////

// regression: `template <typename G> reverse(G &&)` was unconstrained, so for
// a non-const lvalue of the view type it beat the copy constructor -- it
// deduces an exact match where the copy constructor needs an added const --
// and hard-errored on `static_cast<static_digraph &>` inside graph_ref_view.
// `explicit` masked it: the copy-initialisation spelling `auto r2 = r;` never
// reaches the template.
GTEST_TEST(reverse_views, copying_a_mutable_lvalue_uses_the_copy_constructor) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc(0, 1).add_arc(1, 2);
    auto [graph] = builder.build();

    reverse_view view(graph);

    reverse_view from_mutable_lvalue(view);
    static_assert(std::same_as<decltype(view), decltype(from_mutable_lvalue)>);
    ASSERT_EQ(from_mutable_lvalue.num_arcs(), num_arcs(graph));
    ASSERT_TRUE(
        EQ_RANGES(view.in_neighbors(1u), from_mutable_lvalue.in_neighbors(1u)));

    decltype(view) from_const_lvalue(std::as_const(view));
    ASSERT_EQ(from_const_lvalue.num_arcs(), num_arcs(graph));

    // reversing a reverse is a different specialization, so the guard leaves it
    // reachable -- it just needs the template arguments spelled out, since CTAD
    // always prefers the copy deduction candidate here.
    reverse_view<views::graph_all_t<decltype(view) &>> nested(view);
    ASSERT_EQ(nested.num_arcs(), num_arcs(graph));
    ASSERT_TRUE(EQ_RANGES(graph.out_neighbors(0u), nested.out_neighbors(0u)));
}
