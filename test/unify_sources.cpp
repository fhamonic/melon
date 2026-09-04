#undef NDEBUG
#include <gtest/gtest.h>

#include <ranges>
#include <vector>

#include "melon/algorithm/edmonds_karp.hpp"
#include "melon/container/mutable_digraph.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/experimental/unify_sources.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "arc_list_digraph.hpp"
#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// unify_sources extends a dense-id graph with a root vertex and one virtual
// arc per source, and models the full incidence interface over the extended
// id spaces
////////////////////////////////////////////////////////////////////////////////

namespace {
// vertices 0..3, arcs 0: 0>2, 1: 1>2, 2: 2>3; sources {0, 1} add root 4 and
// virtual arcs 3: 4>0 and 4: 4>1
auto unified_test_instance() {
    static_digraph_builder<static_digraph, int> builder(4);
    builder.add_arc({0u, 2u}, 3).add_arc({1u, 2u}, 5).add_arc({2u, 3u}, 4);
    return builder.build();
}
}  // namespace

using unified_t = decltype(experimental::views::unify_sources(
    std::declval<static_digraph &>(),
    std::declval<std::vector<unsigned int> &>()));
static_assert(graph_view<unified_t>);
static_assert(outward_incidence_graph<unified_t>);
static_assert(inward_incidence_graph<unified_t>);
static_assert(outward_adjacency_graph<unified_t>);
static_assert(inward_adjacency_graph<unified_t>);
static_assert(has_arc_source<unified_t> && has_arc_target<unified_t>);
static_assert(has_out_degree<unified_t> && has_in_degree<unified_t>);
static_assert(has_num_vertices<unified_t> && has_num_arcs<unified_t>);
static_assert(has_vertex_map<unified_t> && has_arc_map<unified_t>);

GTEST_TEST(unify_sources, extended_id_spaces) {
    auto [graph, cap] = unified_test_instance();
    std::vector<unsigned int> sources = {0u, 1u};
    auto view = experimental::views::unify_sources(graph, sources);

    ASSERT_EQ(view.root(), 4u);
    ASSERT_EQ(view.num_vertices(), 5u);
    ASSERT_EQ(view.num_arcs(), 5u);
    ASSERT_TRUE(EQ_MULTISETS(view.vertices(), {0u, 1u, 2u, 3u, 4u}));
    ASSERT_TRUE(EQ_MULTISETS(view.arcs(), {0u, 1u, 2u, 3u, 4u}));

    // virtual_arcs is aligned with the sources range that was given
    ASSERT_TRUE(std::ranges::equal(view.virtual_arcs(),
                                   std::vector<unsigned int>{3u, 4u}));
    ASSERT_EQ(arc_source(view, 3u), 4u);
    ASSERT_EQ(arc_target(view, 3u), 0u);
    ASSERT_EQ(arc_source(view, 4u), 4u);
    ASSERT_EQ(arc_target(view, 4u), 1u);
    // the wrapped graph's arcs keep their endpoints
    ASSERT_EQ(arc_source(view, 0u), 0u);
    ASSERT_EQ(arc_target(view, 2u), 3u);

    ASSERT_TRUE(is_valid_vertex(view, 4u));
    ASSERT_FALSE(is_valid_vertex(view, 5u));
    ASSERT_TRUE(is_valid_arc(view, 4u));
    ASSERT_FALSE(is_valid_arc(view, 5u));
}

GTEST_TEST(unify_sources, incidence_and_adjacency) {
    auto [graph, cap] = unified_test_instance();
    std::vector<unsigned int> sources = {0u, 1u};
    auto view = experimental::views::unify_sources(graph, sources);

    ASSERT_TRUE(EQ_MULTISETS(out_arcs(view, 4u), {3u, 4u}));
    ASSERT_TRUE(EQ_MULTISETS(out_arcs(view, 0u), {0u}));
    ASSERT_TRUE(EMPTY(out_arcs(view, 3u)));

    ASSERT_TRUE(EMPTY(in_arcs(view, 4u)));
    ASSERT_TRUE(EQ_MULTISETS(in_arcs(view, 0u), {3u}));
    ASSERT_TRUE(EQ_MULTISETS(in_arcs(view, 1u), {4u}));
    ASSERT_TRUE(EQ_MULTISETS(in_arcs(view, 2u), {0u, 1u}));
    ASSERT_TRUE(EQ_MULTISETS(in_arcs(view, 3u), {2u}));

    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(view, 4u), {0u, 1u}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(view, 2u), {3u}));
    ASSERT_TRUE(EMPTY(in_neighbors(view, 4u)));
    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(view, 0u), {4u}));
    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(view, 2u), {0u, 1u}));

    ASSERT_EQ(out_degree(view, 4u), 2u);
    ASSERT_EQ(out_degree(view, 0u), 1u);
    ASSERT_EQ(in_degree(view, 4u), 0u);
    ASSERT_EQ(in_degree(view, 0u), 1u);
    ASSERT_EQ(in_degree(view, 2u), 2u);
}

GTEST_TEST(unify_sources, arcs_entries_cover_the_virtual_arcs) {
    auto [graph, cap] = unified_test_instance();
    std::vector<unsigned int> sources = {0u, 1u};
    auto view = experimental::views::unify_sources(graph, sources);

    std::vector<std::pair<unsigned int, std::pair<unsigned int, unsigned int>>>
        expected = {{0u, {0u, 2u}},
                    {1u, {1u, 2u}},
                    {2u, {2u, 3u}},
                    {3u, {4u, 0u}},
                    {4u, {4u, 1u}}};
    ASSERT_TRUE(EQ_MULTISETS(arcs_entries(view), expected));
}

GTEST_TEST(unify_sources, maps_cover_the_virtual_elements) {
    auto [graph, cap] = unified_test_instance();
    std::vector<unsigned int> sources = {0u, 1u};
    auto view = experimental::views::unify_sources(graph, sources);

    auto vertex_map = create_vertex_map<int>(view, 0);
    auto arc_map = create_arc_map<int>(view, 0);
    for(auto && v : vertices(view)) vertex_map[v] = int(v) + 1;
    for(auto && a : arcs(view)) arc_map[a] = int(a) + 1;
    ASSERT_EQ(vertex_map[view.root()], 5);
    for(auto && a : view.virtual_arcs()) ASSERT_EQ(arc_map[a], int(a) + 1);
}

////////////////////////////////////////////////////////////////////////////////
// the flagship consumer: multi-source maximum flow, with the user extending
// the capacity map over the virtual arcs through a lambda keyed by the
// view's arc ids
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(unify_sources, multi_source_max_flow) {
    auto [graph, cap] = unified_test_instance();
    std::vector<unsigned int> sources = {0u, 1u};
    auto view = experimental::views::unify_sources(graph, sources);

    auto capacity = [&cap, first = 3u](const unsigned int a) {
        return a < first ? cap[a] : 100;
    };
    edmonds_karp alg(view, capacity, view.root(), 3u);
    // both sources can feed vertex 2 (3 + 5 units), but the 2>3 arc caps the
    // flow at 4
    ASSERT_EQ(alg.run().flow_value(), 4);
}

////////////////////////////////////////////////////////////////////////////////
// composition and degenerate shapes
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(unify_sources, pipe_syntax) {
    auto [graph, cap] = unified_test_instance();
    auto view = graph | experimental::views::unify_sources(
                            std::vector<unsigned int>{2u});
    ASSERT_EQ(view.root(), 4u);
    ASSERT_TRUE(EQ_MULTISETS(out_arcs(view, 4u), {3u}));
    ASSERT_EQ(arc_target(view, 3u), 2u);
}

GTEST_TEST(unify_sources, empty_sources) {
    auto [graph, cap] = unified_test_instance();
    auto view = experimental::views::unify_sources(
        graph, std::views::empty<unsigned int>);
    ASSERT_EQ(view.root(), 4u);
    ASSERT_EQ(view.num_arcs(), 3u);
    ASSERT_TRUE(EMPTY(view.virtual_arcs()));
    ASSERT_TRUE(EMPTY(out_arcs(view, 4u)));
    ASSERT_TRUE(EQ_MULTISETS(in_arcs(view, 2u), {0u, 1u}));
}

GTEST_TEST(unify_sources, all_vertices_as_sources) {
    // the network-simplex shape: every vertex hangs off the root
    auto [graph, cap] = unified_test_instance();
    auto view = experimental::views::unify_sources(graph, vertices(graph));
    ASSERT_EQ(view.num_arcs(), 7u);
    ASSERT_TRUE(EQ_MULTISETS(out_arcs(view, 4u), {3u, 4u, 5u, 6u}));
    for(auto && v : vertices(graph)) ASSERT_EQ(arc_target(view, 3u + v), v);
}

////////////////////////////////////////////////////////////////////////////////
// the virtual in-arc map exists only where the base graph has an inward
// feature to forward: over a bare arc list the view answers no inward
// question, so duplicate sources are legal parallel arcs instead of a
// precondition
////////////////////////////////////////////////////////////////////////////////

using unified_arc_list_t = decltype(experimental::views::unify_sources(
    std::declval<arc_list_digraph &>(),
    std::declval<std::vector<unsigned int> &>()));
static_assert(melon::graph<unified_arc_list_t>);
static_assert(!has_in_arcs<unified_arc_list_t>);
static_assert(!has_in_degree<unified_arc_list_t>);

GTEST_TEST(unify_sources, no_inward_map_over_a_bare_arc_list) {
    arc_list_digraph graph{3u, {{0u, 1u}, {1u, 2u}}};
    std::vector<unsigned int> sources = {1u, 1u};
    auto view = experimental::views::unify_sources(graph, sources);

    ASSERT_EQ(view.root(), 3u);
    ASSERT_EQ(view.num_arcs(), 4u);
    std::vector<std::pair<unsigned int, std::pair<unsigned int, unsigned int>>>
        expected = {
            {0u, {0u, 1u}}, {1u, {1u, 2u}}, {2u, {3u, 1u}}, {3u, {3u, 1u}}};
    ASSERT_TRUE(EQ_MULTISETS(arcs_entries(view), expected));
}

////////////////////////////////////////////////////////////////////////////////
// duplicate sources are a precondition: one vertex cannot answer in_arcs
// with two virtual arcs
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(unify_sources, duplicate_sources_are_a_precondition) {
    auto [graph, cap] = unified_test_instance();
    std::vector<unsigned int> sources = {1u, 1u};
    auto construct = [&]() {
        auto view = experimental::views::unify_sources(graph, sources);
        (void)view;
    };
    EXPECT_DEATH(construct(), "");
}

////////////////////////////////////////////////////////////////////////////////
// over a hole-free mutable_digraph -- dense, so admissible -- the root's
// incidence must list exactly the virtual arcs. The empty base piece is a
// default-constructed intrusive-iterator subrange: it is empty only because
// those iterators default at-end, and an iterator defaulting to a zero
// cursor makes this walk a null structure
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(unify_sources, root_incidence_over_a_mutable_digraph) {
    mutable_digraph graph;
    const auto v0 = graph.create_vertex();
    const auto v1 = graph.create_vertex();
    const auto v2 = graph.create_vertex();
    (void)graph.create_arc(v0, v1);
    (void)graph.create_arc(v1, v2);
    std::vector<unsigned int> sources = {v0, v2};
    auto view = experimental::views::unify_sources(graph, sources);

    ASSERT_EQ(view.root(), 3u);
    ASSERT_TRUE(
        std::ranges::equal(out_arcs(view, view.root()), view.virtual_arcs()));
    ASSERT_TRUE(std::ranges::empty(in_arcs(view, view.root())));
    ASSERT_TRUE(std::ranges::equal(out_arcs(view, v1), out_arcs(graph, v1)));
}
