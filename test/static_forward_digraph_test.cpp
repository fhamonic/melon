#include <gtest/gtest.h>

#include "melon/concepts/graph.hpp"
#include "melon/static_forward_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic;
using namespace fhamonic::melon;

static_assert(melon::concepts::graph<static_forward_digraph>);
static_assert(melon::concepts::outward_incidence_graph<static_forward_digraph>);
static_assert(melon::concepts::outward_adjacency_graph<static_forward_digraph>);
static_assert(melon::concepts::has_vertex_map<static_forward_digraph>);
static_assert(melon::concepts::has_arc_map<static_forward_digraph>);

GTEST_TEST(static_forward_digraph, empty_constructor) {
    static_forward_digraph graph;
    ASSERT_EQ(nb_vertices(graph), 0);
    ASSERT_EQ(nb_arcs(graph), 0);
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_FALSE(is_valid_vertex(graph,0));
    ASSERT_FALSE(is_valid_arc(graph,0));

    EXPECT_DEATH((void)out_arcs(graph,0), "");
    EXPECT_DEATH((void)target(graph,0), "");
}

GTEST_TEST(static_forward_digraph, empty_vectors_constructor) {
    std::vector<vertex_t<static_forward_digraph>> sources;
    std::vector<vertex_t<static_forward_digraph>> targets;

    static_forward_digraph graph(0, std::move(sources), std::move(targets));
    ASSERT_EQ(nb_vertices(graph), 0);
    ASSERT_EQ(nb_arcs(graph), 0);
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_FALSE(is_valid_vertex(graph,0));
    ASSERT_FALSE(is_valid_arc(graph,0));

    EXPECT_DEATH((void)out_arcs(graph,0), "");
    EXPECT_DEATH((void)target(graph,0), "");
    EXPECT_DEATH((void)out_neighbors(graph,0), "");
}

GTEST_TEST(static_forward_digraph, vectors_constructor_1) {
    std::vector<std::pair<arc_t<static_forward_digraph>,
                          std::pair<vertex_t<static_forward_digraph>,
                                    vertex_t<static_forward_digraph>>>>
        arc_pairs(
            {{0, {0, 1}}, {1, {0, 2}}, {2, {1, 2}}, {3, {2, 0}}, {4, {2, 1}}});

    static_forward_digraph graph(
        3, std::ranges::views::keys(std::ranges::views::values(arc_pairs)),
        std::ranges::views::values(std::ranges::views::values(arc_pairs)));
    ASSERT_EQ(nb_vertices(graph), 3);
    ASSERT_EQ(nb_arcs(graph), 5);

    ASSERT_TRUE(EQ_RANGES(vertices(graph), {0, 1, 2}));
    ASSERT_TRUE(EQ_RANGES(arcs(graph), {0, 1, 2, 3, 4}));

    for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph,u));
    ASSERT_FALSE(is_valid_vertex(graph,
        vertex_t<static_forward_digraph>(nb_vertices(graph))));

    for(auto a : arcs(graph)) ASSERT_TRUE(is_valid_arc(graph,a));
    ASSERT_FALSE(
        is_valid_arc(graph,arc_t<static_forward_digraph>(nb_arcs(graph))));

    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph,0), {1, 2}));
    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph,1), {2}));
    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph,2), {0, 1}));
    ASSERT_TRUE(EQ_RANGES(arcs_entries(graph), arc_pairs));

    // for(arc_t<static_forward_digraph> a : arcs(graph)) {
    //     ASSERT_EQ(source(graph,a), arc_pairs[a].first);
    // }
}

GTEST_TEST(static_forward_digraph, vectors_constructor_2) {
    std::vector<std::pair<arc_t<static_forward_digraph>,std::pair<vertex_t<static_forward_digraph>,
                          vertex_t<static_forward_digraph>>>>
        arc_pairs({{0, {1, 2}},
                   {1, {1, 6}},
                   {2, {1, 7}},
                   {3, {2, 3}},
                   {4, {2, 4}},
                   {5, {3, 4}},
                   {6, {5, 2}},
                   {7, {5, 3}},
                   {8, {6, 5}}});

    static_forward_digraph graph(
        8, std::ranges::views::keys(std::ranges::views::values(arc_pairs)),
        std::ranges::views::values(std::ranges::views::values(arc_pairs)));
    ASSERT_EQ(nb_vertices(graph), 8);
    ASSERT_EQ(nb_arcs(graph), 9);

    ASSERT_TRUE(EQ_RANGES(vertices(graph), {0, 1, 2, 3, 4, 5, 6, 7}));
    ASSERT_TRUE(EQ_RANGES(arcs(graph), {0, 1, 2, 3, 4, 5, 6, 7, 8}));

    for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph,u));
    ASSERT_FALSE(is_valid_vertex(graph,
        vertex_t<static_forward_digraph>(nb_vertices(graph))));

    for(auto a : arcs(graph)) ASSERT_TRUE(is_valid_arc(graph,a));
    ASSERT_FALSE(
        is_valid_arc(graph,arc_t<static_forward_digraph>(nb_arcs(graph))));

    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph,0),
                  std::ranges::empty_view<vertex_t<static_forward_digraph>>()));
    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph,1), {2, 6, 7}));
    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph,2), {3, 4}));
    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph,6), {5}));
    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph,7),
                  std::ranges::empty_view<vertex_t<static_forward_digraph>>()));

    ASSERT_TRUE(EQ_RANGES(arcs_entries(graph), arc_pairs));
}