#include <gtest/gtest.h>
#include <iostream>

#include "melon/static_digraph.hpp"

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
    // EXPECT_TRUE(std::ranges::empty(graph.arcs_pairs()));

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
    ASSERT_TRUE(std::ranges::empty(graph.nodes()));
    ASSERT_TRUE(std::ranges::empty(graph.arcs()));

    EXPECT_FALSE(graph.is_valid_node(0));
    EXPECT_FALSE(graph.is_valid_arc(0));

    EXPECT_DEATH(graph.out_arcs(0), "");
    EXPECT_DEATH(graph.target(0), "");
    EXPECT_DEATH(graph.out_arcs(0), "");
}
