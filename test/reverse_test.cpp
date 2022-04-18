#include <gtest/gtest.h>

#include "melon/adaptor/reverse.hpp"
#include "melon/algorithm/dijkstra.hpp"
#include "melon/arc_list_builder.hpp"
#include "melon/static_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(reverse_adaptor, static_graph) {
    std::vector<std::pair<static_digraph::vertex_t, static_digraph::vertex_t>>
        arc_pairs({{0, 1}, {0, 2}, {1, 2}, {2, 0}, {2, 1}});

    static_digraph graph(3, std::ranges::views::keys(arc_pairs),
                         std::ranges::views::values(arc_pairs));

    auto reverse_graph = adaptors::reverse(graph);

    ASSERT_EQ(graph.nb_vertices(), reverse_graph.nb_vertices());
    ASSERT_EQ(graph.nb_arcs(), reverse_graph.nb_arcs());

    ASSERT_EQ_RANGES(graph.vertices(), reverse_graph.vertices());
    ASSERT_EQ_RANGES(graph.arcs(), reverse_graph.arcs());

    for(auto u : graph.vertices()) ASSERT_TRUE(graph.is_valid_node(u));
    ASSERT_FALSE(
        graph.is_valid_node(static_digraph::vertex_t(graph.nb_vertices())));

    for(auto a : graph.arcs()) ASSERT_TRUE(graph.is_valid_arc(a));
    ASSERT_FALSE(graph.is_valid_arc(static_digraph::arc_t(graph.nb_arcs())));

    ASSERT_EQ_RANGES(graph.out_neighbors(0), reverse_graph.in_neighbors(0));
    ASSERT_EQ_RANGES(graph.out_neighbors(1), reverse_graph.in_neighbors(1));
    ASSERT_EQ_RANGES(graph.out_neighbors(2), reverse_graph.in_neighbors(2));

    ASSERT_EQ_RANGES(graph.in_neighbors(0), reverse_graph.out_neighbors(0));
    ASSERT_EQ_RANGES(graph.in_neighbors(1), reverse_graph.out_neighbors(1));
    ASSERT_EQ_RANGES(graph.in_neighbors(2), reverse_graph.out_neighbors(2));

    ASSERT_EQ_RANGES(graph.arcs_pairs(), arc_pairs);

    for(static_digraph::arc_t a : graph.arcs()) {
        ASSERT_EQ(graph.source(a), arc_pairs[a].first);
    }
}

GTEST_TEST(reverse_adaptor, dijkstra) {
    arc_list_builder<static_digraph, int> builder(6);

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
    auto graph = adaptors::reverse(fgraph);

    Dijkstra dijkstra(graph, length_map);

    dijkstra.add_source(0);
    ASSERT_FALSE(dijkstra.empty_queue());
    ASSERT_EQ(dijkstra.next_node(), std::make_pair(0u, 0));
    ASSERT_FALSE(dijkstra.empty_queue());
    ASSERT_EQ(dijkstra.next_node(), std::make_pair(1u, 7));
    ASSERT_FALSE(dijkstra.empty_queue());
    ASSERT_EQ(dijkstra.next_node(), std::make_pair(2u, 9));
    ASSERT_FALSE(dijkstra.empty_queue());
    ASSERT_EQ(dijkstra.next_node(), std::make_pair(5u, 11));
    ASSERT_FALSE(dijkstra.empty_queue());
    ASSERT_EQ(dijkstra.next_node(), std::make_pair(4u, 20));
    ASSERT_FALSE(dijkstra.empty_queue());
    ASSERT_EQ(dijkstra.next_node(), std::make_pair(3u, 21));
    ASSERT_TRUE(dijkstra.empty_queue());
}