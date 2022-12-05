#include <gtest/gtest.h>

#include "melon/concepts/graph.hpp"
#include "melon/static_forward_weighted_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic;
using namespace fhamonic::melon;

static_assert(melon::concepts::graph<static_forward_weighted_digraph<int>>);
static_assert(melon::concepts::incidence_list_graph<
              static_forward_weighted_digraph<int>>);
static_assert(melon::concepts::adjacency_list_graph<
              static_forward_weighted_digraph<int>>);
static_assert(
    melon::concepts::has_vertices_map<static_forward_weighted_digraph<int>>);

GTEST_TEST(static_forward_weighted_digraph, empty_constructor) {
    using Graph = static_forward_weighted_digraph<int>;
    Graph graph;
    ASSERT_EQ(graph.nb_vertices(), 0);
    ASSERT_EQ(graph.nb_arcs(), 0);
    ASSERT_TRUE(std::ranges::empty(graph.vertices()));
    ASSERT_EQ(std::ranges::distance(graph.arcs()), 0);
    ASSERT_EQ(std::ranges::distance(graph.arcs_pairs()), 0);

    ASSERT_FALSE(graph.is_valid_vertex(0));

    EXPECT_DEATH(graph.out_arcs(0), "");
}

GTEST_TEST(static_forward_weighted_digraph, empty_vectors_constructor) {
    using Graph = static_forward_weighted_digraph<double>;
    std::vector<vertex_t<Graph>> arcs_sources;
    std::vector<std::pair<vertex_t<Graph>, double>> arcs;

    Graph graph(0, std::move(arcs_sources), std::move(arcs));
    ASSERT_EQ(graph.nb_vertices(), 0);
    ASSERT_EQ(graph.nb_arcs(), 0);
    ASSERT_TRUE(std::ranges::empty(graph.vertices()));
    ASSERT_EQ(std::ranges::distance(graph.arcs()), 0);
    ASSERT_EQ(std::ranges::distance(graph.arcs_pairs()), 0);

    ASSERT_FALSE(graph.is_valid_vertex(0));

    EXPECT_DEATH(graph.out_arcs(0), "");
}

GTEST_TEST(static_forward_weighted_digraph, vectors_constructor_1) {
    using Graph = static_forward_weighted_digraph<double>;
    std::vector<std::pair<vertex_t<Graph>, vertex_t<Graph>>> arcs_pairs(
        {{0, 1}, {0, 2}, {1, 2}, {2, 0}, {2, 1}});

    std::vector<vertex_t<Graph>> arcs_sources = {0, 0, 1, 2, 2};
    std::vector<std::pair<vertex_t<Graph>, double>> arcs = {
        {1, 1.0}, {2, 1.0}, {2, 1.0}, {0, 1.0}, {1, 1.0}};
    std::vector<std::pair<vertex_t<Graph>, double>> arcs_copy = arcs;

    Graph graph(3, std::move(arcs_sources), std::move(arcs));
    ASSERT_EQ(graph.nb_vertices(), 3);
    ASSERT_EQ(graph.nb_arcs(), 5);

    ASSERT_TRUE(EQ_RANGES(graph.vertices(), {0, 1, 2}));
    ASSERT_TRUE(EQ_RANGES(graph.arcs(), arcs_copy));

    for(auto u : graph.vertices()) ASSERT_TRUE(graph.is_valid_vertex(u));
    ASSERT_FALSE(graph.is_valid_vertex(vertex_t<Graph>(graph.nb_vertices())));

    ASSERT_TRUE(EQ_RANGES(graph.out_neighbors(0), {1, 2}));
    ASSERT_TRUE(EQ_RANGES(graph.out_neighbors(1), {2}));
    ASSERT_TRUE(EQ_RANGES(graph.out_neighbors(2), {0, 1}));
    ASSERT_TRUE(EQ_RANGES(graph.arcs_pairs(), arcs_pairs));
}

// GTEST_TEST(static_forward_weighted_digraph, vectors_constructor_2) {
//     std::vector<std::pair<vertex_t<static_forward_weighted_digraph>,
//                           vertex_t<static_forward_weighted_digraph>>>
//         arc_pairs({{1, 2},
//                    {1, 6},
//                    {1, 7},
//                    {2, 3},
//                    {2, 4},
//                    {3, 4},
//                    {5, 2},
//                    {5, 3},
//                    {6, 5}});

//     static_forward_weighted_digraph graph(
//         8, std::ranges::views::keys(arc_pairs),
//         std::ranges::views::values(arc_pairs));
//     ASSERT_EQ(graph.nb_vertices(), 8);
//     ASSERT_EQ(graph.nb_arcs(), 9);

//     ASSERT_TRUE(EQ_RANGES(graph.vertices(), {0, 1, 2, 3, 4, 5, 6, 7}));
//     ASSERT_TRUE(EQ_RANGES(graph.arcs(), {0, 1, 2, 3, 4, 5, 6, 7, 8}));

//     for(auto u : graph.vertices()) ASSERT_TRUE(graph.is_valid_vertex(u));
//     ASSERT_FALSE(graph.is_valid_vertex(
//         vertex_t<static_forward_weighted_digraph>(graph.nb_vertices())));

//     for(auto a : graph.arcs()) ASSERT_TRUE(graph.is_valid_arc(a));
//     ASSERT_FALSE(graph.is_valid_arc(
//         arc_t<static_forward_weighted_digraph>(graph.nb_arcs())));

//     ASSERT_EQ_RANGES(
//         graph.out_neighbors(0),
//         std::ranges::empty_view<vertex_t<static_forward_weighted_digraph>>());
//     ASSERT_TRUE(EQ_RANGES(graph.out_neighbors(1), {2, 6, 7}));
//     ASSERT_TRUE(EQ_RANGES(graph.out_neighbors(2), {3, 4}));
//     ASSERT_TRUE(EQ_RANGES(graph.out_neighbors(6), {5}));
//     ASSERT_EQ_RANGES(
//         graph.out_neighbors(7),
//         std::ranges::empty_view<vertex_t<static_forward_weighted_digraph>>());

//     ASSERT_TRUE(EQ_RANGES(graph.arcs_pairs(), arc_pairs));
// }