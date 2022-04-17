#include <gtest/gtest.h>

#include "melon/static_forward_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(static_forward_digraph, empty_constructor) {
    static_forward_digraph graph;
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

GTEST_TEST(static_forward_digraph, vectors_constructor_no_elements) {
    std::vector<static_forward_digraph::vertex_t> sources;
    std::vector<static_forward_digraph::vertex_t> targets;

    static_forward_digraph graph(0, std::move(sources), std::move(targets));
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

GTEST_TEST(static_forward_digraph, vectors_constructor_1) {
    std::vector<std::pair<static_forward_digraph::vertex_t,
                          static_forward_digraph::vertex_t>>
        arc_pairs({{0, 1}, {0, 2}, {1, 2}, {2, 0}, {2, 1}});

    static_forward_digraph graph(3, std::ranges::views::keys(arc_pairs),
                                 std::ranges::views::values(arc_pairs));
    ASSERT_EQ(graph.nb_vertices(), 3);
    ASSERT_EQ(graph.nb_arcs(), 5);

    AssertRangesAreEqual(
        graph.vertices(),
        std::vector<static_forward_digraph::vertex_t>({0, 1, 2}));
    AssertRangesAreEqual(
        graph.arcs(),
        std::vector<static_forward_digraph::arc_t>({0, 1, 2, 3, 4}));

    for(auto u : graph.vertices()) ASSERT_TRUE(graph.is_valid_node(u));
    ASSERT_FALSE(graph.is_valid_node(
        static_forward_digraph::vertex_t(graph.nb_vertices())));

    for(auto a : graph.arcs()) ASSERT_TRUE(graph.is_valid_arc(a));
    ASSERT_FALSE(
        graph.is_valid_arc(static_forward_digraph::arc_t(graph.nb_arcs())));

    AssertRangesAreEqual(graph.out_neighbors(0),
                         std::vector<static_forward_digraph::vertex_t>({1, 2}));
    AssertRangesAreEqual(graph.out_neighbors(1),
                         std::vector<static_forward_digraph::vertex_t>({2}));
    AssertRangesAreEqual(graph.out_neighbors(2),
                         std::vector<static_forward_digraph::vertex_t>({0, 1}));

    AssertRangesAreEqual(graph.arcs_pairs(), arc_pairs);

    for(static_forward_digraph::arc_t a : graph.arcs()) {
        ASSERT_EQ(graph.source(a), arc_pairs[a].first);
    }
}

GTEST_TEST(static_forward_digraph, vectors_constructor_2) {
    std::vector<std::pair<static_forward_digraph::vertex_t,
                          static_forward_digraph::vertex_t>>
        arc_pairs({{1, 2},
                   {1, 6},
                   {1, 7},
                   {2, 3},
                   {2, 4},
                   {3, 4},
                   {5, 2},
                   {5, 3},
                   {6, 5}});

    static_forward_digraph graph(8, std::ranges::views::keys(arc_pairs),
                                 std::ranges::views::values(arc_pairs));
    ASSERT_EQ(graph.nb_vertices(), 8);
    ASSERT_EQ(graph.nb_arcs(), 9);

    AssertRangesAreEqual(graph.vertices(),
                         std::vector<static_forward_digraph::vertex_t>(
                             {0, 1, 2, 3, 4, 5, 6, 7}));
    AssertRangesAreEqual(graph.arcs(),
                         std::vector<static_forward_digraph::arc_t>(
                             {0, 1, 2, 3, 4, 5, 6, 7, 8}));

    for(auto u : graph.vertices()) ASSERT_TRUE(graph.is_valid_node(u));
    ASSERT_FALSE(graph.is_valid_node(
        static_forward_digraph::vertex_t(graph.nb_vertices())));

    for(auto a : graph.arcs()) ASSERT_TRUE(graph.is_valid_arc(a));
    ASSERT_FALSE(
        graph.is_valid_arc(static_forward_digraph::arc_t(graph.nb_arcs())));

    AssertRangesAreEqual(
        graph.out_neighbors(0),
        std::ranges::empty_view<static_forward_digraph::vertex_t>());
    AssertRangesAreEqual(
        graph.out_neighbors(1),
        std::vector<static_forward_digraph::vertex_t>({2, 6, 7}));
    AssertRangesAreEqual(graph.out_neighbors(2),
                         std::vector<static_forward_digraph::vertex_t>({3, 4}));
    AssertRangesAreEqual(graph.out_neighbors(6),
                         std::vector<static_forward_digraph::vertex_t>({5}));
    AssertRangesAreEqual(
        graph.out_neighbors(7),
        std::ranges::empty_view<static_forward_digraph::vertex_t>());

    AssertRangesAreEqual(graph.arcs_pairs(), arc_pairs);

    for(static_forward_digraph::arc_t a : graph.arcs()) {
        ASSERT_EQ(graph.source(a), arc_pairs[a].first);
    }
}
