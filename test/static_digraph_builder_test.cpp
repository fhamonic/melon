#include <gtest/gtest.h>

#include "melon/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(StaticDigraphBuilder, build_no_maps) {
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
