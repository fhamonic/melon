#include <gtest/gtest.h>

#include "melon/static_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(StaticDigraph, empty_constructor) {
    StaticDigraph graph;
    EXPECT_EQ(graph.nb_nodes(), 0);
    EXPECT_EQ(graph.nb_arcs(), 0);
    EXPECT_TRUE(std::ranges::empty(graph.nodes()));
    EXPECT_TRUE(std::ranges::empty(graph.arcs()));
    EXPECT_EQ(std::ranges::distance(graph.arcs_pairs()), 0);

    EXPECT_FALSE(graph.is_valid_node(0));
    EXPECT_FALSE(graph.is_valid_arc(0));

    EXPECT_DEATH(graph.out_arcs(0), "");
    EXPECT_DEATH(graph.target(0), "");
    EXPECT_DEATH(graph.out_arcs(0), "");
}

GTEST_TEST(StaticDigraph, vectors_constructor_no_elements) {
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

GTEST_TEST(StaticDigraph, vectors_constructor_1) {
    std::vector<StaticDigraph::Arc> begins = {0, 2, 3};
    std::vector<StaticDigraph::Node> targets = {1, 2, 2, 0, 1};

    StaticDigraph graph(std::move(begins), std::move(targets));
    EXPECT_EQ(graph.nb_nodes(), 3);
    EXPECT_EQ(graph.nb_arcs(), 5);

    AssertRangesAreEqual(graph.nodes(),
                         std::vector<StaticDigraph::Node>({0, 1, 2}));
    AssertRangesAreEqual(graph.arcs(),
                         std::vector<StaticDigraph::Arc>({0, 1, 2, 3, 4}));

    for(auto u : graph.nodes()) EXPECT_TRUE(graph.is_valid_node(u));
    EXPECT_FALSE(graph.is_valid_node(graph.nb_nodes()));

    for(auto a : graph.arcs()) EXPECT_TRUE(graph.is_valid_arc(a));
    EXPECT_FALSE(graph.is_valid_arc(graph.nb_arcs()));

    AssertRangesAreEqual(graph.out_neighbors(0),
                         std::vector<StaticDigraph::Node>({1, 2}));
    AssertRangesAreEqual(graph.out_neighbors(1),
                         std::vector<StaticDigraph::Node>({2}));
    AssertRangesAreEqual(graph.out_neighbors(2),
                         std::vector<StaticDigraph::Node>({0, 1}));

    AssertRangesAreEqual(
        graph.arcs_pairs(),
        std::vector<std::pair<StaticDigraph::Node, StaticDigraph::Node>>(
            {{0, 1}, {0, 2}, {1, 2}, {2, 0}, {2, 1}}));
}

GTEST_TEST(StaticDigraph, vectors_constructor_2) {
    std::vector<StaticDigraph::Arc> begins = {0, 0, 3, 5, 6, 6, 8, 9};
    std::vector<StaticDigraph::Node> targets = {2, 6, 7, 3, 4, 4, 2, 3, 5};

    StaticDigraph graph(std::move(begins), std::move(targets));
    EXPECT_EQ(graph.nb_nodes(), 8);
    EXPECT_EQ(graph.nb_arcs(), 9);

    AssertRangesAreEqual(graph.nodes(), std::vector<StaticDigraph::Node>(
                                            {0, 1, 2, 3, 4, 5, 6, 7}));
    AssertRangesAreEqual(graph.arcs(), std::vector<StaticDigraph::Arc>(
                                           {0, 1, 2, 3, 4, 5, 6, 7, 8}));

    for(auto u : graph.nodes()) EXPECT_TRUE(graph.is_valid_node(u));
    EXPECT_FALSE(graph.is_valid_node(graph.nb_nodes()));

    for(auto a : graph.arcs()) EXPECT_TRUE(graph.is_valid_arc(a));
    EXPECT_FALSE(graph.is_valid_arc(graph.nb_arcs()));

    AssertRangesAreEqual(graph.out_neighbors(0),
                         std::ranges::empty_view<StaticDigraph::Node>());
    AssertRangesAreEqual(graph.out_neighbors(1),
                         std::vector<StaticDigraph::Node>({2, 6, 7}));
    AssertRangesAreEqual(graph.out_neighbors(2),
                         std::vector<StaticDigraph::Node>({3, 4}));
    AssertRangesAreEqual(graph.out_neighbors(6),
                         std::vector<StaticDigraph::Node>({5}));
    AssertRangesAreEqual(graph.out_neighbors(7),
                         std::ranges::empty_view<StaticDigraph::Node>());

    AssertRangesAreEqual(
        graph.arcs_pairs(),
        std::vector<std::pair<StaticDigraph::Node, StaticDigraph::Node>>(
            {{1, 2},
             {1, 6},
             {1, 7},
             {2, 3},
             {2, 4},
             {3, 4},
             {5, 2},
             {5, 3},
             {6, 5}}));
}
