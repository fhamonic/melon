#include <gtest/gtest.h>

#include "melon/views/reverse.hpp"
#include "melon/algorithm/dijkstra.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(reverse_views, static_graph) {
    std::vector<
        std::pair<arc_t<static_digraph>, std::pair<vertex_t<static_digraph>,
                                                   vertex_t<static_digraph>>>>
        arc_pairs(
            {{0, {0, 1}}, {1, {0, 2}}, {2, {1, 2}}, {3, {2, 0}}, {4, {2, 1}}});

    static_digraph graph(
        3, std::ranges::views::keys(std::ranges::views::values(arc_pairs)),
        std::ranges::views::values(std::ranges::views::values(arc_pairs)));

    auto reverse_graph = viewss::reverse(graph);

    ASSERT_EQ(nb_vertices(graph), reverse_graph.nb_vertices());
    ASSERT_EQ(nb_arcs(graph), reverse_graph.nb_arcs());

    ASSERT_TRUE(EQ_RANGES(vertices(graph), reverse_graph.vertices()));
    ASSERT_TRUE(EQ_RANGES(arcs(graph), reverse_graph.arcs()));

    for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph,u));
    ASSERT_FALSE(
        is_valid_vertex(graph,vertex_t<static_digraph>(nb_vertices(graph))));

    for(auto a : arcs(graph)) ASSERT_TRUE(is_valid_arc(graph,a));
    ASSERT_FALSE(is_valid_arc(graph,arc_t<static_digraph>(nb_arcs(graph))));

    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph,0), reverse_graph.in_neighbors(0)));
    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph,1), reverse_graph.in_neighbors(1)));
    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph,2), reverse_graph.in_neighbors(2)));

    ASSERT_TRUE(
        EQ_RANGES(in_neighbors(graph,0), reverse_graph.out_neighbors(0)));
    ASSERT_TRUE(
        EQ_RANGES(in_neighbors(graph,1), reverse_graph.out_neighbors(1)));
    ASSERT_TRUE(
        EQ_RANGES(in_neighbors(graph,2), reverse_graph.out_neighbors(2)));

    ASSERT_TRUE(EQ_RANGES(arcs_entries(graph), arc_pairs));

    for(arc_t<static_digraph> a : arcs(graph)) {
        ASSERT_EQ(arc_source(graph,a), arc_pairs[a].second.first);
    }
}

GTEST_TEST(reverse_views, dijkstra) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 7);
    builder.add_arc(0, 2, 9);
    builder.add_arc(0, 5, 14);
    builder.add_arc(1, 0, 7);
    builder.add_arc(1, 2, 10);
    builder.add_arc(1, 3, 15);
    builder.add_arc(2, 0, 9);
    builder.add_arc(2, 1, 10);
    builder.add_arc(2, 3, 12);
    builder.add_arc(2, 5, 2);
    builder.add_arc(3, 1, 15);
    builder.add_arc(3, 2, 12);
    builder.add_arc(3, 4, 6);
    builder.add_arc(4, 3, 6);
    builder.add_arc(4, 5, 9);
    builder.add_arc(5, 0, 14);
    builder.add_arc(5, 2, 2);
    builder.add_arc(5, 4, 9);

    auto [fgraph, length_map] = builder.build();
    auto graph = viewss::reverse(fgraph);

    dijkstra alg(graph, length_map);

    alg.add_source(0);
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(0u, 0));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(1u, 7));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(2u, 9));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(5u, 11));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(4u, 20));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(3u, 21));
    alg.advance();
    ASSERT_TRUE(alg.finished());
}