#include <gtest/gtest.h>

#include "melon/graph.hpp"

#include "dumb_digraph.hpp"
#include "ranges_test_helper.hpp"

using namespace fhamonic;
using namespace fhamonic::melon;

static_assert(melon::graph<dumb_digraph>);
static_assert(melon::outward_incidence_graph<dumb_digraph>);
static_assert(melon::outward_adjacency_graph<dumb_digraph>);
static_assert(melon::has_vertex_removal<dumb_digraph>);
static_assert(melon::has_arc_removal<dumb_digraph>);
static_assert(melon::has_change_arc_source<dumb_digraph>);
static_assert(melon::has_change_arc_target<dumb_digraph>);

using Graph = dumb_digraph;
using arc_entries_list = std::initializer_list<
    std::pair<arc_t<Graph>, std::pair<vertex_t<Graph>, vertex_t<Graph>>>>;

GTEST_TEST(dumb_digraph, empty_constructor) {
    Graph graph;
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_FALSE(is_valid_vertex(graph, 0));
    EXPECT_DEATH((void)out_arcs(graph, 0), "");
    EXPECT_DEATH((void)in_arcs(graph, 0), "");
    EXPECT_DEATH((void)out_neighbors(graph, 0), "");
    EXPECT_DEATH((void)in_neighbors(graph, 0), "");
}

GTEST_TEST(dumb_digraph, create_vertices) {
    Graph graph;

    graph.create_vertex(0);
    graph.create_vertex(1);
    graph.create_vertex(2);

    ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {0, 1, 2}));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(out_arcs(graph, 0)));
    ASSERT_TRUE(EMPTY(out_arcs(graph, 1)));
    ASSERT_TRUE(EMPTY(out_arcs(graph, 2)));
    ASSERT_TRUE(is_valid_vertex(graph, 2));
    ASSERT_FALSE(is_valid_vertex(graph, 3));
    EXPECT_DEATH((void)out_arcs(graph, 3), "");
}

GTEST_TEST(dumb_digraph, create_arcs) {
    Graph graph;

    graph.create_vertex(0);
    graph.create_vertex(1);
    graph.create_vertex(2);

    graph.create_arc(0, 0, 1);
    graph.create_arc(1, 0, 2);
    graph.create_arc(2, 2, 1);

    ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {0, 1, 2}));

    ASSERT_EQ(arc_source(graph, 0), 0);
    ASSERT_EQ(arc_source(graph, 1), 0);
    ASSERT_EQ(arc_source(graph, 2), 2);
    ASSERT_EQ(arc_target(graph, 0), 1);
    ASSERT_EQ(arc_target(graph, 1), 2);
    ASSERT_EQ(arc_target(graph, 2), 1);

    ASSERT_TRUE(
        EQ_MULTISETS(arcs_entries(graph),
                     arc_entries_list{{0, {0, 1}}, {1, {0, 2}}, {2, {2, 1}}}));

    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 0), {1, 2}));
    ASSERT_TRUE(EMPTY(out_neighbors(graph, 1)));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 2), {1}));
}

// GTEST_TEST(dumb_digraph, remove_arcs) {
//     Graph graph;
//     auto a = create_vertex(graph);
//     auto b = create_vertex(graph);
//     auto c = create_vertex(graph);
//     auto ab = graph.create_arc(a, b);
//     auto ac = graph.create_arc(a, c);
//     auto cb = graph.create_arc(c, b);

//     remove_arc(graph,ac);

//     ASSERT_FALSE(is_valid_arc(graph,ac));
//     ASSERT_EQ(arc_source(graph,ab), a);
//     EXPECT_DEATH((void)arc_source(graph,ac), "");
//     ASSERT_EQ(arc_source(graph,cb), c);
//     ASSERT_EQ(arc_target(graph,ab), b);
//     EXPECT_DEATH((void)arc_target(graph,ac), "");
//     ASSERT_EQ(arc_target(graph,cb), b);

//     ASSERT_TRUE(
//         EQ_MULTISETS(arcs_entries(graph), arc_entries_list{{c, b}, {a,
//         b}}));

//     ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,a), {b}));
//     ASSERT_TRUE(EMPTY(out_neighbors(graph,b)));
//     ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,c), {b}));
// }
