#include <gtest/gtest.h>

#include "melon/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(static_digraphBuilder, build_without_map) {
    static_digraphBuilder<> builder(8);

    builder.add_arc(3, 4);
    builder.add_arc(1, 7);
    builder.add_arc(5, 2);
    builder.add_arc(2, 4);
    builder.add_arc(5, 3);
    builder.add_arc(6, 5);
    builder.add_arc(1, 2);
    builder.add_arc(1, 6);
    builder.add_arc(2, 3);

    auto [graph] = builder.build();

    AssertRangesAreEqual(
        graph.arcs_pairs(),
        std::vector<std::pair<static_digraph::vertex, static_digraph::vertex>>(
            {{1, 2},
             {1, 6},
             {1, 7},
             {2, 3},
             {2, 4},
             {3, 4},
             {5, 2},
             {5, 3},
             {6, 5}}));
}

GTEST_TEST(static_digraphBuilder, build_with_map) {
    constexpr std::size_t n = 8;
    static_digraphBuilder<int> builder(n);

    std::vector<std::pair<static_digraph::vertex, static_digraph::vertex>> pairs{
        {3, 4}, {1, 7}, {5, 2}, {2, 4}, {5, 3}, {6, 5}, {1, 2}, {1, 6}, {2, 3}};

    auto weight = [n](static_digraph::vertex u, static_digraph::vertex v) {
        return static_cast<int>(u * n + v);
    };

    for(auto & [u, v] : pairs) builder.add_arc(u, v, weight(u, v));

    auto [graph, map] = builder.build();

    AssertRangesAreEqual(
        graph.arcs_pairs(),
        std::vector<std::pair<static_digraph::vertex, static_digraph::vertex>>(
            {{1, 2},
             {1, 6},
             {1, 7},
             {2, 3},
             {2, 4},
             {3, 4},
             {5, 2},
             {5, 3},
             {6, 5}}));

    for(static_digraph::arc a : graph.arcs()) {
        auto u = graph.source(a);
        auto v = graph.target(a);
        ASSERT_EQ(map[a], weight(u, v));
    }
}
