#include <gtest/gtest.h>

#include "melon/concepts/graph.hpp"
#include "melon/mutable_weighted_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic;
using namespace fhamonic::melon;

static_assert(melon::concepts::graph<mutable_weighted_digraph<int>>);
static_assert(
    melon::concepts::incidence_list_graph<mutable_weighted_digraph<int>>);
// static_assert(melon::concepts::adjacency_list_graph<
//               mutable_weighted_digraph<int>>);
// static_assert(
//     melon::concepts::has_vertex_map<mutable_weighted_digraph<int>>);

GTEST_TEST(mutable_weighted_digraph, empty_constructor) {
    using Graph = mutable_weighted_digraph<int>;
    Graph graph;
    ASSERT_EQ(graph.nb_vertices(), 0);
    // ASSERT_EQ(graph.nb_arcs(), 0);
    ASSERT_TRUE(std::ranges::empty(graph.vertices()));
    ASSERT_EQ(std::ranges::distance(graph.arcs()), 0);
    ASSERT_EQ(std::ranges::distance(graph.arcs_pairs()), 0);

    ASSERT_FALSE(graph.is_valid_node(0));

    EXPECT_DEATH(graph.out_arcs(0), "");
}

GTEST_TEST(mutable_weighted_digraph, create_vertices) {
    using Graph = mutable_weighted_digraph<double>;
    Graph graph;

    auto a = graph.create_vertex();
    auto b = graph.create_vertex();
    auto c = graph.create_vertex();

    ASSERT_EQ(graph.nb_vertices(), 3);
    ASSERT_EQ_RANGES(graph.vertices(), {0, 1, 2});

    ASSERT_EQ(std::ranges::distance(graph.out_arcs(0)), 0);
    ASSERT_EQ(std::ranges::distance(graph.out_arcs(1)), 0);
    ASSERT_EQ(std::ranges::distance(graph.out_arcs(2)), 0);
    ASSERT_FALSE(graph.is_valid_node(3));
    EXPECT_DEATH(graph.out_arcs(3), "");
}

GTEST_TEST(mutable_weighted_digraph, create_arcs) {
    using Graph = mutable_weighted_digraph<double>;
    Graph graph;

    auto a = graph.create_vertex();
    auto b = graph.create_vertex();
    auto c = graph.create_vertex();

    auto ab = graph.create_arc(a, b, 0.2);
    auto ac = graph.create_arc(a, c, 1.0);
    auto cb = graph.create_arc(c, b, 3.14);

    ASSERT_EQ(graph.target(ab), b);
    ASSERT_EQ(graph.target(ac), c);
    ASSERT_EQ(graph.target(cb), b);

    ASSERT_EQ(graph.weight(ab), 0.2);
    ASSERT_EQ(graph.weight(ac), 1.0);
    ASSERT_EQ(graph.weight(cb), 3.14);

    std::vector<std::pair<vertex_t<Graph>, vertex_t<Graph>>> pairs = {
        {a, b}, {a, c}, {c, b}};
    ASSERT_EQ_RANGES(graph.arcs_pairs(), pairs);

    ASSERT_EQ_RANGES(graph.out_neighbors(a), {c, b});
    ASSERT_EQ(std::ranges::distance(graph.out_neighbors(b)), 0);
    ASSERT_EQ_RANGES(graph.out_neighbors(c), {b});
}

// GTEST_TEST(mutable_weighted_digraph, remove_arcs) {
//     using Graph = mutable_weighted_digraph<double>;
//     Graph graph;
//     auto a = graph.create_vertex();
//     auto b = graph.create_vertex();
//     auto c = graph.create_vertex();
//     auto ab = graph.create_arc(a, b, 0.2);
//     auto ac = graph.create_arc(a, c, 1.0);
//     auto cb = graph.create_arc(c, b, 3.14);

//     graph.remove_arc(ac);

//     ASSERT_EQ(graph.target(ab), b);
//     ASSERT_EQ(graph.target(cb), b);

//     ASSERT_EQ(graph.weight(ab), 0.2);
//     ASSERT_EQ(graph.weight(cb), 3.14);
//     std::vector<std::pair<vertex_t<Graph>, vertex_t<Graph>>> pairs = {{a, b},
//                                                                       {c,
//                                                                       b}};
//     ASSERT_EQ_RANGES(graph.arcs_pairs(), pairs);

//     ASSERT_EQ_RANGES(graph.out_neighbors(a), {b});
//     ASSERT_TRUE(std::ranges::empty(graph.out_neighbors(b)));
//     ASSERT_EQ_RANGES(graph.out_neighbors(c), {b});
// }
