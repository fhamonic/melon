#include <gtest/gtest.h>

#include "melon/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(StaticDigraphBuilder, build_without_map) {
    StaticDigraphBuilder<> builder(8);

    builder.addArc(3, 4);
    builder.addArc(1, 7);
    builder.addArc(5, 2);
    builder.addArc(2, 4);
    builder.addArc(5, 3);
    builder.addArc(6, 5);
    builder.addArc(1, 2);
    builder.addArc(1, 6);
    builder.addArc(2, 3);

    auto [graph] = builder.build();

    AssertRangesAreEqual(
        graph.arcs_pairs(),
        std::vector<std::pair<StaticDigraph::Node, StaticDigraph::Node>>(
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

GTEST_TEST(StaticDigraphBuilder, build_with_map) {
    constexpr std::size_t n = 8;
    StaticDigraphBuilder<double> builder(n);

    std::vector<std::pair<StaticDigraph::Node, StaticDigraph::Node>> pairs{
        {3, 4}, {1, 7}, {5, 2}, {2, 4}, {5, 3}, {6, 5}, {1, 2}, {1, 6}, {2, 3}};

    auto weight = [n](StaticDigraph::Node u, StaticDigraph::Node v) {
        return u * n + v;
    };

    for(auto & [u, v] : pairs)
        builder.addArc(u, v, weight(u,v));

    auto [graph, map] = builder.build();

    AssertRangesAreEqual(
        graph.arcs_pairs(),
        std::vector<std::pair<StaticDigraph::Node, StaticDigraph::Node>>(
            {{1, 2},
             {1, 6},
             {1, 7},
             {2, 3},
             {2, 4},
             {3, 4},
             {5, 2},
             {5, 3},
             {6, 5}}));

    for(StaticDigraph::Arc a : graph.arcs()) {
        auto u = graph.source(a);
        auto v = graph.target(a);
        ASSERT_EQ(map[a], weight(u,v));
    }
}
