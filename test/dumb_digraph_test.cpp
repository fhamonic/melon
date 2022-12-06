#include <gtest/gtest.h>

#include "melon/concepts/graph.hpp"

#include "dumb_digraph.hpp"
#include "ranges_test_helper.hpp"

using namespace fhamonic;
using namespace fhamonic::melon;

static_assert(melon::concepts::graph<dumb_digraph>);
static_assert(melon::concepts::incidence_list_graph<dumb_digraph>);
static_assert(melon::concepts::adjacency_list_graph<dumb_digraph>);
static_assert(melon::concepts::has_vertex_removal<dumb_digraph>);
static_assert(melon::concepts::has_arc_removal<dumb_digraph>);
static_assert(melon::concepts::has_arc_change_source<dumb_digraph>);
static_assert(melon::concepts::has_arc_change_target<dumb_digraph>);

using Graph = dumb_digraph;
using arcs_pairs_list = std::initializer_list<
    std::pair<arc_t<Graph>, std::pair<vertex_t<Graph>, vertex_t<Graph>>>>;

GTEST_TEST(dumb_digraph, empty_constructor) {
    Graph graph;
    ASSERT_TRUE(EMPTY(graph.vertices()));
    ASSERT_TRUE(EMPTY(graph.arcs()));
    ASSERT_TRUE(EMPTY(graph.arcs_pairs()));

    ASSERT_FALSE(graph.is_valid_vertex(0));
    EXPECT_DEATH(graph.out_arcs(0), "");
    EXPECT_DEATH(graph.in_arcs(0), "");
    EXPECT_DEATH(graph.out_neighbors(0), "");
    EXPECT_DEATH(graph.in_neighbors(0), "");
}

GTEST_TEST(dumb_digraph, create_vertices) {
    Graph graph;

    graph.create_vertex(0);
    graph.create_vertex(1);
    graph.create_vertex(2);

    ASSERT_TRUE(EQ_MULTISETS(graph.vertices(), {0, 1, 2}));
    ASSERT_TRUE(EMPTY(graph.arcs()));
    ASSERT_TRUE(EMPTY(graph.out_arcs(0)));
    ASSERT_TRUE(EMPTY(graph.out_arcs(1)));
    ASSERT_TRUE(EMPTY(graph.out_arcs(2)));
    ASSERT_TRUE(graph.is_valid_vertex(2));
    ASSERT_FALSE(graph.is_valid_vertex(3));
    EXPECT_DEATH(graph.out_arcs(3), "");
}

GTEST_TEST(dumb_digraph, create_arcs) {
    Graph graph;

    graph.create_vertex(0);
    graph.create_vertex(1);
    graph.create_vertex(2);

    graph.create_arc(0, 0, 1);
    graph.create_arc(1, 0, 2);
    graph.create_arc(2, 2, 1);

    ASSERT_TRUE(EQ_MULTISETS(graph.vertices(), {0, 1, 2}));

    ASSERT_EQ(graph.source(0), 0);
    ASSERT_EQ(graph.source(1), 0);
    ASSERT_EQ(graph.source(2), 2);
    ASSERT_EQ(graph.target(0), 1);
    ASSERT_EQ(graph.target(1), 2);
    ASSERT_EQ(graph.target(2), 1);

    ASSERT_TRUE(
        EQ_MULTISETS(graph.arcs_pairs(),
                     arcs_pairs_list{{0, {0, 1}}, {1, {0, 2}}, {2, {2, 1}}}));

    ASSERT_TRUE(EQ_MULTISETS(graph.out_neighbors(0), {1, 2}));
    ASSERT_TRUE(EMPTY(graph.out_neighbors(1)));
    ASSERT_TRUE(EQ_MULTISETS(graph.out_neighbors(2), {1}));
}

// GTEST_TEST(dumb_digraph, remove_arcs) {
//     Graph graph;
//     auto a = graph.create_vertex();
//     auto b = graph.create_vertex();
//     auto c = graph.create_vertex();
//     auto ab = graph.create_arc(a, b);
//     auto ac = graph.create_arc(a, c);
//     auto cb = graph.create_arc(c, b);

//     graph.remove_arc(ac);

//     ASSERT_FALSE(graph.is_valid_arc(ac));
//     ASSERT_EQ(graph.source(ab), a);
//     EXPECT_DEATH(graph.source(ac), "");
//     ASSERT_EQ(graph.source(cb), c);
//     ASSERT_EQ(graph.target(ab), b);
//     EXPECT_DEATH(graph.target(ac), "");
//     ASSERT_EQ(graph.target(cb), b);

//     ASSERT_TRUE(
//         EQ_MULTISETS(graph.arcs_pairs(), arcs_pairs_list{{c, b}, {a,
//         b}}));

//     ASSERT_TRUE(EQ_MULTISETS(graph.out_neighbors(a), {b}));
//     ASSERT_TRUE(EMPTY(graph.out_neighbors(b)));
//     ASSERT_TRUE(EQ_MULTISETS(graph.out_neighbors(c), {b}));
// }
