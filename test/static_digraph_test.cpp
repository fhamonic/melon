#include <gtest/gtest.h>
#include <iostream>

#include "melon/static_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

int main(int argc, char ** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

GTEST_TEST(StaticDigraph, empty_constructor) {
    StaticDigraph graph;
    EXPECT_EQ(graph.nb_nodes(), 0);
    EXPECT_EQ(graph.nb_arcs(), 0);
    EXPECT_TRUE(std::ranges::empty(graph.nodes()));
    EXPECT_TRUE(std::ranges::empty(graph.arcs()));
    EXPECT_TRUE(std::ranges::distance(graph.arcs_pairs()) == 0);

    EXPECT_FALSE(graph.is_valid_node(0));
    EXPECT_FALSE(graph.is_valid_arc(0));

    EXPECT_DEATH(graph.out_arcs(0), "");
    EXPECT_DEATH(graph.target(0), "");
    EXPECT_DEATH(graph.out_arcs(0), "");
}

GTEST_TEST(StaticDigraph, builder_constructor_no_elements) {
    std::vector<StaticDigraph::Arc> begins;
    std::vector<StaticDigraph::Node> targets;

    StaticDigraph graph(std::move(begins), std::move(targets));
    EXPECT_EQ(graph.nb_nodes(), 0);
    EXPECT_EQ(graph.nb_arcs(), 0);
    EXPECT_TRUE(std::ranges::empty(graph.nodes()));
    EXPECT_TRUE(std::ranges::empty(graph.arcs()));
    EXPECT_EQ(std::ranges::distance(graph.arcs_pairs()), 0);

    EXPECT_FALSE(graph.is_valid_node(0));
    EXPECT_FALSE(graph.is_valid_arc(0));

    EXPECT_DEATH(graph.out_arcs(0), "");
    EXPECT_DEATH(graph.target(0), "");
    EXPECT_DEATH(graph.out_neighbors(0), "");
}

GTEST_TEST(StaticDigraph, builder_constructor) {
    std::vector<StaticDigraph::Arc> begins = {0, 0, 3, 5, 6, 6, 8, 9};
    std::vector<StaticDigraph::Node> targets = {2, 6, 7, 3, 4, 4, 2, 3, 5};

    StaticDigraph graph(std::move(begins), std::move(targets));
    EXPECT_EQ(graph.nb_nodes(), 8);
    EXPECT_EQ(graph.nb_arcs(), 9);

    range_equals(graph.nodes(), {0, 1, 2, 3, 4, 5, 6, 7});

    // EXPECT_EQ(std::ranges::size(graph.arcs()), 9);
    // EXPECT_EQ(std::ranges::distance(graph.arcs_pairs()), 9);
}
