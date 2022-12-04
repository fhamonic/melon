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

GTEST_TEST(mutable_digraph, empty_constructor) {
    using Graph = mutable_digraph;
    Graph graph;
    ASSERT_EQ(std::ranges::distance(graph.vertices()), 0);
    ASSERT_EQ(std::ranges::distance(graph.arcs()), 0);
    ASSERT_EQ(std::ranges::distance(graph.arcs_pairs()), 0);

    ASSERT_FALSE(graph.is_valid_vertex(0));

    EXPECT_DEATH(graph.out_arcs(0), "");
}

GTEST_TEST(mutable_digraph, create_vertices) {
    using Graph = mutable_digraph;
    Graph graph;

    auto a = graph.create_vertex();
    auto b = graph.create_vertex();
    auto c = graph.create_vertex();

    ASSERT_EQ_RANGES(graph.vertices(), {0, 1, 2});
    ASSERT_EQ(std::ranges::distance(graph.arcs()), 0);

    ASSERT_EQ(std::ranges::distance(graph.out_arcs(0)), 0);
    ASSERT_EQ(std::ranges::distance(graph.out_arcs(1)), 0);
    ASSERT_EQ(std::ranges::distance(graph.out_arcs(2)), 0);
    ASSERT_FALSE(graph.is_valid_vertex(3));
    EXPECT_DEATH(graph.out_arcs(3), "");
}

GTEST_TEST(mutable_digraph, create_arcs) {
    using Graph = mutable_digraph;
    Graph graph;

    auto a = graph.create_vertex();
    auto b = graph.create_vertex();
    auto c = graph.create_vertex();

    auto ab = graph.create_arc(a, b);
    auto ac = graph.create_arc(a, c);
    auto cb = graph.create_arc(c, b);

    ASSERT_EQ(std::ranges::distance(graph.arcs()), 3);

    ASSERT_EQ(graph.target(ab), b);
    ASSERT_EQ(graph.target(ac), c);
    ASSERT_EQ(graph.target(cb), b);

    std::vector<std::pair<vertex_t<Graph>, vertex_t<Graph>>> pairs = {
        {a, b}, {a, c}, {c, b}};
    ASSERT_EQ_RANGES(graph.arcs_pairs(), pairs);

    ASSERT_EQ_RANGES(graph.out_neighbors(a), {c, b});
    ASSERT_EQ(std::ranges::distance(graph.out_neighbors(b)), 0);
    ASSERT_EQ_RANGES(graph.out_neighbors(c), {b});
}

GTEST_TEST(mutable_digraph, remove_arcs) {
    using Graph = mutable_digraph;
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

    std::vector<std::pair<vertex_t<Graph>, vertex_t<Graph>>> pairs = {{a, b},
                                                                      {c, b}};
    ASSERT_EQ_RANGES(graph.arcs_pairs(), pairs);

    ASSERT_EQ_RANGES(graph.out_neighbors(a), {b});
    ASSERT_EQ(std::ranges::distance(graph.out_neighbors(b)), 0);
    ASSERT_EQ_RANGES(graph.out_neighbors(c), {b});
}
