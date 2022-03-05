#include <gtest/gtest.h>

#include "melon/static_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(static_digraph, empty_constructor) {
    static_digraph graph;
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

GTEST_TEST(static_digraph, vectors_constructor_no_elements) {
    std::vector<static_digraph::arc> begins;
    std::vector<static_digraph::vertex> targets;

    static_digraph graph(std::move(begins), std::move(targets));
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

GTEST_TEST(static_digraph, vectors_constructor_1) {
    std::vector<static_digraph::arc> begins = {0, 2, 3};
    std::vector<static_digraph::vertex> targets = {1, 2, 2, 0, 1};

    std::vector<std::pair<static_digraph::vertex, static_digraph::vertex>>
        must_arc_pairs({{0, 1}, {0, 2}, {1, 2}, {2, 0}, {2, 1}});

    static_digraph graph(std::move(begins), std::move(targets));
    ASSERT_EQ(graph.nb_vertices(), 3);
    ASSERT_EQ(graph.nb_arcs(), 5);

    AssertRangesAreEqual(graph.vertices(),
                         std::vector<static_digraph::vertex>({0, 1, 2}));
    AssertRangesAreEqual(graph.arcs(),
                         std::vector<static_digraph::arc>({0, 1, 2, 3, 4}));

    for(auto u : graph.vertices()) ASSERT_TRUE(graph.is_valid_node(u));
    ASSERT_FALSE(graph.is_valid_node(static_digraph::vertex(graph.nb_vertices())));

    for(auto a : graph.arcs()) ASSERT_TRUE(graph.is_valid_arc(a));
    ASSERT_FALSE(graph.is_valid_arc(static_digraph::arc(graph.nb_arcs())));

    AssertRangesAreEqual(graph.out_neighbors(0),
                         std::vector<static_digraph::vertex>({1, 2}));
    AssertRangesAreEqual(graph.out_neighbors(1),
                         std::vector<static_digraph::vertex>({2}));
    AssertRangesAreEqual(graph.out_neighbors(2),
                         std::vector<static_digraph::vertex>({0, 1}));

    AssertRangesAreEqual(graph.arcs_pairs(), must_arc_pairs);

    for(static_digraph::arc a : graph.arcs()) {
        ASSERT_EQ(graph.source(a), must_arc_pairs[a].first);
    }
}

GTEST_TEST(static_digraph, vectors_constructor_2) {
    std::vector<static_digraph::arc> begins = {0, 0, 3, 5, 6, 6, 8, 9};
    std::vector<static_digraph::vertex> targets = {2, 6, 7, 3, 4, 4, 2, 3, 5};

    std::vector<std::pair<static_digraph::vertex, static_digraph::vertex>>
        must_arc_pairs({{1, 2},
                        {1, 6},
                        {1, 7},
                        {2, 3},
                        {2, 4},
                        {3, 4},
                        {5, 2},
                        {5, 3},
                        {6, 5}});

    static_digraph graph(std::move(begins), std::move(targets));
    ASSERT_EQ(graph.nb_vertices(), 8);
    ASSERT_EQ(graph.nb_arcs(), 9);

    AssertRangesAreEqual(graph.vertices(), std::vector<static_digraph::vertex>(
                                            {0, 1, 2, 3, 4, 5, 6, 7}));
    AssertRangesAreEqual(graph.arcs(), std::vector<static_digraph::arc>(
                                           {0, 1, 2, 3, 4, 5, 6, 7, 8}));

    for(auto u : graph.vertices()) ASSERT_TRUE(graph.is_valid_node(u));
    ASSERT_FALSE(graph.is_valid_node(static_digraph::vertex(graph.nb_vertices())));

    for(auto a : graph.arcs()) ASSERT_TRUE(graph.is_valid_arc(a));
    ASSERT_FALSE(graph.is_valid_arc(static_digraph::arc(graph.nb_arcs())));

    AssertRangesAreEqual(graph.out_neighbors(0),
                         std::ranges::empty_view<static_digraph::vertex>());
    AssertRangesAreEqual(graph.out_neighbors(1),
                         std::vector<static_digraph::vertex>({2, 6, 7}));
    AssertRangesAreEqual(graph.out_neighbors(2),
                         std::vector<static_digraph::vertex>({3, 4}));
    AssertRangesAreEqual(graph.out_neighbors(6),
                         std::vector<static_digraph::vertex>({5}));
    AssertRangesAreEqual(graph.out_neighbors(7),
                         std::ranges::empty_view<static_digraph::vertex>());

    AssertRangesAreEqual(graph.arcs_pairs(), must_arc_pairs);

    for(static_digraph::arc a : graph.arcs()) {
        ASSERT_EQ(graph.source(a), must_arc_pairs[a].first);
    }
}
