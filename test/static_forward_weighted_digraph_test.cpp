#include <gtest/gtest.h>

#include "melon/graph.hpp"
#include "melon/static_forward_weighted_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic;
using namespace fhamonic::melon;

static_assert(melon::graph<static_forward_weighted_digraph<int>>);
static_assert(melon::outward_incidence_graph<
              static_forward_weighted_digraph<int>>);
static_assert(melon::outward_adjacency_graph<
              static_forward_weighted_digraph<int>>);
static_assert(
    melon::has_vertex_map<static_forward_weighted_digraph<int>>);

GTEST_TEST(static_forward_weighted_digraph, empty_constructor) {
    using Graph = static_forward_weighted_digraph<int>;
    Graph graph;
    ASSERT_EQ(nb_vertices(graph), 0);
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_FALSE(is_valid_vertex(graph,0));

    EXPECT_DEATH((void)out_arcs(graph,0), "");
}

GTEST_TEST(static_forward_weighted_digraph, empty_vectors_constructor) {
    using Graph = static_forward_weighted_digraph<double>;
    std::vector<vertex_t<Graph>> arcs_sources;
    std::vector<std::pair<std::pair<vertex_t<Graph>, vertex_t<Graph>>, double>>
        arcs_entries_vector;

    Graph graph(0ul, arcs_entries_vector);
    ASSERT_EQ(nb_vertices(graph), 0);
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_FALSE(is_valid_vertex(graph,0));

    EXPECT_DEATH((void)out_arcs(graph,0), "");
}

GTEST_TEST(static_forward_weighted_digraph, vectors_constructor_1) {
    using Graph = static_forward_weighted_digraph<double>;
    std::vector<std::pair<std::pair<vertex_t<Graph>, vertex_t<Graph>>, double>>
        weighted_arcs_entries({{{0, 1}, 1.0},
                               {{0, 2}, 1.0},
                               {{1, 2}, 1.0},
                               {{2, 0}, 1.0},
                               {{2, 1}, 1.0}});

    Graph graph(3ul, weighted_arcs_entries);
    ASSERT_EQ(nb_vertices(graph), 3);

    ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {0, 1, 2}));
    // ASSERT_TRUE(EQ_MULTISETS(arcs(graph), arcs_copy));

    for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph,u));
    ASSERT_FALSE(is_valid_vertex(graph,vertex_t<Graph>(nb_vertices(graph))));

    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,0), {1, 2}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,1), {2}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,2), {0, 1}));
    ASSERT_TRUE(EQ_MULTISETS(std::ranges::views::values(arcs_entries(graph)),
                             std::ranges::views::keys(weighted_arcs_entries)));

    // for(auto && a : out_arcs(graph,0))
    //     ++(weights_map(graph)[a]);
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
//     ASSERT_EQ(nb_vertices(graph), 8);
//     ASSERT_EQ(nb_arcs(graph), 9);

//     ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {0, 1, 2, 3, 4, 5, 6, 7}));
//     ASSERT_TRUE(EQ_MULTISETS(arcs(graph), {0, 1, 2, 3, 4, 5, 6, 7, 8}));

//     for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph,u));
//     ASSERT_FALSE(is_valid_vertex(graph,
//         vertex_t<static_forward_weighted_digraph>(nb_vertices(graph))));

//     for(auto a : arcs(graph)) ASSERT_TRUE(is_valid_arc(graph,a));
//     ASSERT_FALSE(is_valid_arc(graph,
//         arc_t<static_forward_weighted_digraph>(nb_arcs(graph))));

//     ASSERT_EQ_MULTISETS(
//         out_neighbors(graph,0),
//         std::ranges::empty_view<vertex_t<static_forward_weighted_digraph>>());
//     ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,1), {2, 6, 7}));
//     ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,2), {3, 4}));
//     ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph,6), {5}));
//     ASSERT_EQ_MULTISETS(
//         out_neighbors(graph,7),
//         std::ranges::empty_view<vertex_t<static_forward_weighted_digraph>>());

//     ASSERT_TRUE(EQ_MULTISETS(arcs_entries(graph), arc_pairs));
// }