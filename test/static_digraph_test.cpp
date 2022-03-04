#include <gtest/gtest.h>

#include "melon/static_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(StaticDigraph, empty_constructor) {
    StaticDigraph graph;
    ASSERT_EQ(graph.nb_vertices(), 0);
    ASSERT_EQ(graph.nb_arcs(), 0);
    ASSERT_TRUE(std::ranges::empty(graph.vertices()));
    ASSERT_TRUE(std::ranges::empty(graph.arcs()));
    ASSERT_EQ(std::ranges::distance(graph.arcs_pairs()), 0);

    ASSERT_FALSE(graph.is_valid_node(0));
    ASSERT_FALSE(graph.is_valid_arc(0));

    EXPECT_DEATH(graph.out_arcs(0), "");
    EXPECT_DEATH(graph.source(0), "");
    EXPECT_DEATH(graph.target(0), "");
    EXPECT_DEATH(graph.out_arcs(0), "");
}

GTEST_TEST(StaticDigraph, vectors_constructor_no_elements) {
    std::vector<StaticDigraph::arc> begins;
    std::vector<StaticDigraph::vertex> targets;

    StaticDigraph graph(std::move(begins), std::move(targets));
    ASSERT_EQ(graph.nb_vertices(), 0);
    ASSERT_EQ(graph.nb_arcs(), 0);
    ASSERT_TRUE(std::ranges::empty(graph.vertices()));
    ASSERT_TRUE(std::ranges::empty(graph.arcs()));
    ASSERT_EQ(std::ranges::distance(graph.arcs_pairs()), 0);

    ASSERT_FALSE(graph.is_valid_node(0));
    ASSERT_FALSE(graph.is_valid_arc(0));

    EXPECT_DEATH(graph.out_arcs(0), "");
    EXPECT_DEATH(graph.source(0), "");
    EXPECT_DEATH(graph.target(0), "");
    EXPECT_DEATH(graph.out_neighbors(0), "");
}

GTEST_TEST(StaticDigraph, vectors_constructor_1) {
    std::vector<StaticDigraph::arc> begins = {0, 2, 3};
    std::vector<StaticDigraph::vertex> targets = {1, 2, 2, 0, 1};

    std::vector<std::pair<StaticDigraph::vertex, StaticDigraph::vertex>>
        must_arc_pairs({{0, 1}, {0, 2}, {1, 2}, {2, 0}, {2, 1}});

    StaticDigraph graph(std::move(begins), std::move(targets));
    ASSERT_EQ(graph.nb_vertices(), 3);
    ASSERT_EQ(graph.nb_arcs(), 5);

    AssertRangesAreEqual(graph.vertices(),
                         std::vector<StaticDigraph::vertex>({0, 1, 2}));
    AssertRangesAreEqual(graph.arcs(),
                         std::vector<StaticDigraph::arc>({0, 1, 2, 3, 4}));

    for(auto u : graph.vertices()) ASSERT_TRUE(graph.is_valid_node(u));
    ASSERT_FALSE(graph.is_valid_node(StaticDigraph::vertex(graph.nb_vertices())));

    for(auto a : graph.arcs()) ASSERT_TRUE(graph.is_valid_arc(a));
    ASSERT_FALSE(graph.is_valid_arc(StaticDigraph::arc(graph.nb_arcs())));

    AssertRangesAreEqual(graph.out_neighbors(0),
                         std::vector<StaticDigraph::vertex>({1, 2}));
    AssertRangesAreEqual(graph.out_neighbors(1),
                         std::vector<StaticDigraph::vertex>({2}));
    AssertRangesAreEqual(graph.out_neighbors(2),
                         std::vector<StaticDigraph::vertex>({0, 1}));

    AssertRangesAreEqual(graph.arcs_pairs(), must_arc_pairs);

    for(StaticDigraph::arc a : graph.arcs()) {
        ASSERT_EQ(graph.source(a), must_arc_pairs[a].first);
    }
}

GTEST_TEST(StaticDigraph, vectors_constructor_2) {
    std::vector<StaticDigraph::arc> begins = {0, 0, 3, 5, 6, 6, 8, 9};
    std::vector<StaticDigraph::vertex> targets = {2, 6, 7, 3, 4, 4, 2, 3, 5};

    std::vector<std::pair<StaticDigraph::vertex, StaticDigraph::vertex>>
        must_arc_pairs({{1, 2},
                        {1, 6},
                        {1, 7},
                        {2, 3},
                        {2, 4},
                        {3, 4},
                        {5, 2},
                        {5, 3},
                        {6, 5}});

    StaticDigraph graph(std::move(begins), std::move(targets));
    ASSERT_EQ(graph.nb_vertices(), 8);
    ASSERT_EQ(graph.nb_arcs(), 9);

    AssertRangesAreEqual(graph.vertices(), std::vector<StaticDigraph::vertex>(
                                            {0, 1, 2, 3, 4, 5, 6, 7}));
    AssertRangesAreEqual(graph.arcs(), std::vector<StaticDigraph::arc>(
                                           {0, 1, 2, 3, 4, 5, 6, 7, 8}));

    for(auto u : graph.vertices()) ASSERT_TRUE(graph.is_valid_node(u));
    ASSERT_FALSE(graph.is_valid_node(StaticDigraph::vertex(graph.nb_vertices())));

    for(auto a : graph.arcs()) ASSERT_TRUE(graph.is_valid_arc(a));
    ASSERT_FALSE(graph.is_valid_arc(StaticDigraph::arc(graph.nb_arcs())));

    AssertRangesAreEqual(graph.out_neighbors(0),
                         std::ranges::empty_view<StaticDigraph::vertex>());
    AssertRangesAreEqual(graph.out_neighbors(1),
                         std::vector<StaticDigraph::vertex>({2, 6, 7}));
    AssertRangesAreEqual(graph.out_neighbors(2),
                         std::vector<StaticDigraph::vertex>({3, 4}));
    AssertRangesAreEqual(graph.out_neighbors(6),
                         std::vector<StaticDigraph::vertex>({5}));
    AssertRangesAreEqual(graph.out_neighbors(7),
                         std::ranges::empty_view<StaticDigraph::vertex>());

    AssertRangesAreEqual(graph.arcs_pairs(), must_arc_pairs);

    for(StaticDigraph::arc a : graph.arcs()) {
        ASSERT_EQ(graph.source(a), must_arc_pairs[a].first);
    }
}
