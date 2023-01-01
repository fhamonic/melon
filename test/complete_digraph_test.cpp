#include <gtest/gtest.h>

#include "melon/graph.hpp"
#include "melon/views/complete_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic;
using namespace fhamonic::melon;

using G = views::complete_digraph<>;

static_assert(melon::graph<G>);
static_assert(melon::outward_incidence_graph<G>);
static_assert(melon::outward_adjacency_graph<G>);
static_assert(melon::inward_incidence_graph<G>);
static_assert(melon::inward_adjacency_graph<G>);
static_assert(melon::has_vertex_map<G>);
static_assert(melon::has_arc_map<G>);

GTEST_TEST(complete_digraph, empty_constructor) {
    G graph;
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    EXPECT_DEATH((void)arc_source(graph, 0), "");
    EXPECT_DEATH((void)arc_target(graph, 0), "");

    EXPECT_DEATH((void)out_arcs(graph, 0), "");
    EXPECT_DEATH((void)out_neighbors(graph, 0), "");
    EXPECT_DEATH((void)in_arcs(graph, 0), "");
    EXPECT_DEATH((void)in_neighbors(graph, 0), "");
}

GTEST_TEST(complete_digraph, k4) {
    G graph(4);

    ASSERT_EQ(nb_vertices(graph), 4);
    ASSERT_EQ(nb_arcs(graph), 12);

    ASSERT_TRUE(EQ_MULTISETS(out_arcs(graph, 0), {0, 1, 2}));
    ASSERT_TRUE(EQ_MULTISETS(out_arcs(graph, 1), {3, 4, 5}));
    ASSERT_TRUE(EQ_MULTISETS(out_arcs(graph, 2), {6, 7, 8}));
    ASSERT_TRUE(EQ_MULTISETS(out_arcs(graph, 3), {9, 10, 11}));

    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 0), {1, 2, 3}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 1), {0, 2, 3}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 2), {0, 1, 3}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 3), {0, 1, 2}));

    ASSERT_TRUE(EQ_MULTISETS(in_arcs(graph, 0), {3, 6, 9}));
    ASSERT_TRUE(EQ_MULTISETS(in_arcs(graph, 1), {0, 7, 10}));
    ASSERT_TRUE(EQ_MULTISETS(in_arcs(graph, 2), {1, 4, 11}));
    ASSERT_TRUE(EQ_MULTISETS(in_arcs(graph, 3), {2, 5, 8}));

    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, 0), {1, 2, 3}));
    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, 1), {0, 2, 3}));
    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, 2), {0, 1, 3}));
    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, 3), {0, 1, 2}));   
}
