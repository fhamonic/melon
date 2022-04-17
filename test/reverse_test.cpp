#include <gtest/gtest.h>

#include "melon/static_digraph.hpp"
#include "melon/adaptor/reverse.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(reverse_adaptor, static_graph) {
    std::vector<std::pair<static_digraph::vertex_t, static_digraph::vertex_t>>
        arc_pairs({{0, 1}, {0, 2}, {1, 2}, {2, 0}, {2, 1}});

    static_digraph graph(3, std::ranges::views::keys(arc_pairs),
                         std::ranges::views::values(arc_pairs));

    auto reverse_graph = reverse(graph);

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