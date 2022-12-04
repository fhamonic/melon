#include <gtest/gtest.h>

#include "melon/concepts/graph.hpp"
#include "melon/mutable_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic;
using namespace fhamonic::melon;

static_assert(melon::concepts::graph<mutable_digraph>);
static_assert(melon::concepts::incidence_list_graph<mutable_digraph>);
static_assert(melon::concepts::adjacency_list_graph<mutable_digraph>);
static_assert(melon::concepts::has_vertex_map<mutable_digraph>);

using Graph = mutable_digraph;
using vertices_pair_list =
    std::initializer_list<std::pair<vertex_t<Graph>, vertex_t<Graph>>>;

GTEST_TEST(mutable_digraph, empty_constructor) {
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

GTEST_TEST(mutable_digraph, create_vertices) {
    Graph graph;

    auto a = graph.create_vertex();
    auto b = graph.create_vertex();
    auto c = graph.create_vertex();

    ASSERT_EQ_SETS(graph.vertices(), {a, b, c});
    ASSERT_TRUE(EMPTY(graph.arcs()));
    ASSERT_TRUE(EMPTY(graph.out_arcs(0)));
    ASSERT_TRUE(EMPTY(graph.out_arcs(1)));
    ASSERT_TRUE(EMPTY(graph.out_arcs(2)));
    ASSERT_TRUE(graph.is_valid_vertex(2));
    ASSERT_FALSE(graph.is_valid_vertex(3));
    EXPECT_DEATH(graph.out_arcs(3), "");
}

GTEST_TEST(mutable_digraph, create_arcs) {
    Graph graph;

    auto a = graph.create_vertex();
    auto b = graph.create_vertex();
    auto c = graph.create_vertex();

    auto ab = graph.create_arc(a, b);
    auto ac = graph.create_arc(a, c);
    auto cb = graph.create_arc(c, b);

    ASSERT_EQ_SETS(graph.vertices(), {ab, ac, cb});

    ASSERT_EQ(graph.source(ab), a);
    ASSERT_EQ(graph.source(ac), a);
    ASSERT_EQ(graph.source(cb), c);
    ASSERT_EQ(graph.target(ab), b);
    ASSERT_EQ(graph.target(ac), c);
    ASSERT_EQ(graph.target(cb), b);

    ASSERT_EQ_SETS(graph.arcs_pairs(),
                   vertices_pair_list{{c, b}, {a, c}, {a, b}});

    ASSERT_EQ_SETS(graph.out_neighbors(a), {b, c});
    ASSERT_TRUE(EMPTY(graph.out_neighbors(b)));
    ASSERT_EQ_SETS(graph.out_neighbors(c), {b});
}

GTEST_TEST(mutable_digraph, remove_arcs) {
    Graph graph;
    auto a = graph.create_vertex();
    auto b = graph.create_vertex();
    auto c = graph.create_vertex();
    auto ab = graph.create_arc(a, b);
    auto ac = graph.create_arc(a, c);
    auto cb = graph.create_arc(c, b);

    graph.remove_arc(ac);

    ASSERT_EQ(graph.target(ab), b);
    ASSERT_EQ(graph.target(cb), b);

    ASSERT_EQ_SETS(graph.arcs_pairs(), vertices_pair_list{{c, b}, {a, b}});

    ASSERT_EQ_SETS(graph.out_neighbors(a), {b});
    ASSERT_TRUE(EMPTY(graph.out_neighbors(b)));
    ASSERT_EQ_SETS(graph.out_neighbors(c), {b});
}
