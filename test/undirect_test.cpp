#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/dijkstra.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/undirect.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(undirect_views, static_graph) {
    std::vector<
        std::pair<arc_t<static_digraph>, std::pair<vertex_t<static_digraph>,
                                                   vertex_t<static_digraph>>>>
        arc_pairs(
            {{0, {0, 1}}, {1, {0, 2}}, {2, {1, 2}}, {3, {2, 0}}, {4, {2, 1}}});

    static_digraph graph(
        3, std::ranges::views::keys(std::ranges::views::values(arc_pairs)),
        std::ranges::views::values(std::ranges::views::values(arc_pairs)));

    auto ugraph = views::undirect(graph);

    ASSERT_EQ(num_vertices(graph), num_vertices(ugraph));
    ASSERT_EQ(num_arcs(graph), nb_edges(ugraph));

    ASSERT_TRUE(EQ_RANGES(vertices(graph), vertices(ugraph)));
    ASSERT_TRUE(EQ_RANGES(arcs(graph), edges(ugraph)));

    for(auto && a : arcs(graph)) {
        auto u1 = arc_source(graph, a);
        auto v1 = arc_target(graph, a);
        auto [u2, v2] = edge_endpoints(ugraph, a);
        ASSERT_TRUE((u1 == u2 && v1 == v2) || (u1 == v2 && u2 == v1));
    }
}