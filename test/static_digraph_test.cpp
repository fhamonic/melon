#include <gtest/gtest.h>

#include "melon/graph.hpp"
#include "melon/container/static_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic;
using namespace fhamonic::melon;

static_assert(melon::graph<static_digraph>);
static_assert(melon::copyable_graph<static_digraph>);
static_assert(melon::outward_incidence_graph<static_digraph>);
static_assert(melon::outward_adjacency_graph<static_digraph>);
static_assert(melon::inward_incidence_graph<static_digraph>);
static_assert(melon::inward_adjacency_graph<static_digraph>);
static_assert(melon::has_vertex_map<static_digraph>);
static_assert(melon::has_arc_map<static_digraph>);

GTEST_TEST(static_digraph, empty_constructor) {
    static_digraph graph;
    ASSERT_EQ(nb_vertices(graph), 0);
    ASSERT_EQ(nb_arcs(graph), 0);
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_FALSE(is_valid_vertex(graph,0));
    ASSERT_FALSE(is_valid_arc(graph,0));

    EXPECT_DEATH((void)out_arcs(graph,0), "");
    EXPECT_DEATH((void)arc_target(graph,0), "");
    EXPECT_DEATH((void)out_arcs(graph,0), "");
    EXPECT_DEATH((void)arc_source(graph,0), "");
}

GTEST_TEST(static_digraph, empty_vectors_constructor) {
    std::vector<vertex_t<static_digraph>> sources;
    std::vector<vertex_t<static_digraph>> targets;

    static_digraph graph(0, std::move(sources), std::move(targets));
    ASSERT_EQ(nb_vertices(graph), 0);
    ASSERT_EQ(nb_arcs(graph), 0);
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_FALSE(is_valid_vertex(graph,0));
    ASSERT_FALSE(is_valid_arc(graph,0));

    EXPECT_DEATH((void)out_arcs(graph,0), "");
    EXPECT_DEATH((void)arc_target(graph,0), "");
    EXPECT_DEATH((void)out_arcs(graph,0), "");
    EXPECT_DEATH((void)arc_source(graph,0), "");
}

GTEST_TEST(static_digraph, vectors_constructor_1) {
    std::vector<
        std::pair<arc_t<static_digraph>, std::pair<vertex_t<static_digraph>,
                                                   vertex_t<static_digraph>>>>
        arc_pairs(
            {{0, {0, 1}}, {1, {0, 2}}, {2, {1, 2}}, {3, {2, 0}}, {4, {2, 1}}});

    static_digraph graph(
        3, std::ranges::views::keys(std::ranges::views::values(arc_pairs)),
        std::ranges::views::values(std::ranges::views::values(arc_pairs)));
    ASSERT_EQ(nb_vertices(graph), 3);
    ASSERT_EQ(nb_arcs(graph), 5);
    ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {0, 1, 2}));
    ASSERT_TRUE(EQ_MULTISETS(arcs(graph), {0, 1, 2, 3, 4}));

    for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph,u));
    ASSERT_FALSE(
        is_valid_vertex(graph,vertex_t<static_digraph>(nb_vertices(graph))));

    for(auto a : arcs(graph)) ASSERT_TRUE(is_valid_arc(graph,a));
    ASSERT_FALSE(is_valid_arc(graph,arc_t<static_digraph>(nb_arcs(graph))));

    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,0), {1, 2}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,1), {2}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,2), {0, 1}));

    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph,0), {2}));
    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph,1), {0, 2}));
    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph,2), {0, 1}));

    ASSERT_TRUE(EQ_MULTISETS(arcs_entries(graph), arc_pairs));

    for(arc_t<static_digraph> a : arcs(graph)) {
        ASSERT_EQ(arc_source(graph,a), arc_pairs[a].second.first);
        ASSERT_EQ(arc_target(graph,a), arc_pairs[a].second.second);
    }
}

GTEST_TEST(static_digraph, vectors_constructor_2) {
    std::vector<
        std::pair<arc_t<static_digraph>, std::pair<vertex_t<static_digraph>,
                                                   vertex_t<static_digraph>>>>
        arc_pairs({{0, {1, 2}},
                   {1, {1, 6}},
                   {2, {1, 7}},
                   {3, {2, 3}},
                   {4, {2, 4}},
                   {5, {3, 4}},
                   {6, {5, 2}},
                   {7, {5, 3}},
                   {8, {6, 5}}});

    static_digraph graph(
        8, std::ranges::views::keys(std::ranges::views::values(arc_pairs)),
        std::ranges::views::values(std::ranges::views::values(arc_pairs)));
    ASSERT_EQ(nb_vertices(graph), 8);
    ASSERT_EQ(nb_arcs(graph), 9);

    ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {0, 1, 2, 3, 4, 5, 6, 7}));
    ASSERT_TRUE(EQ_MULTISETS(arcs(graph), {0, 1, 2, 3, 4, 5, 6, 7, 8}));

    for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph,u));
    ASSERT_FALSE(
        is_valid_vertex(graph,vertex_t<static_digraph>(nb_vertices(graph))));

    for(auto a : arcs(graph)) ASSERT_TRUE(is_valid_arc(graph,a));
    ASSERT_FALSE(is_valid_arc(graph,arc_t<static_digraph>(nb_arcs(graph))));

    ASSERT_TRUE(EMPTY(out_neighbors(graph,0)));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,1), {2, 6, 7}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,2), {3, 4}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,6), {5}));
    ASSERT_TRUE(EMPTY(out_neighbors(graph,7)));

    ASSERT_TRUE(EQ_MULTISETS(arcs_entries(graph), arc_pairs));

    for(arc_t<static_digraph> a : arcs(graph)) {
        ASSERT_EQ(arc_source(graph,a), arc_pairs[a].second.first);
        ASSERT_EQ(arc_target(graph,a), arc_pairs[a].second.second);
    }
}
