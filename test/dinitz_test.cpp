#include <gtest/gtest.h>

#include "melon/algorithm/dinitz.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(dinitz, no_arcs) {
    static_digraph_builder<static_digraph, int, char> builder(2);

    auto [graph, capacity, part_of_minimum_cut] = builder.build();

    dinitz alg(graph, capacity, 0u, 1u);
    ASSERT_EQ(alg.run().flow_value(), 0);
    ASSERT_TRUE(EMPTY(alg.minimum_cut()));
    alg.reset();
}

GTEST_TEST(dinitz, arc_with_0_capacity) {
    static_digraph_builder<static_digraph, int> builder(2);

    builder.add_arc(0, 1, 0);

    auto [graph, capacity] = builder.build();

    dinitz alg(graph, capacity, 0u, 1u);
    ASSERT_EQ(alg.run().flow_value(), 0);
    // ASSERT_TRUE(EQ_MULTISETS(alg.minimum_cut(), {0u}));
    alg.reset();
}

GTEST_TEST(dinitz, arc_with_fixed_capacity) {
    static_digraph_builder<static_digraph, int> builder(2);

    builder.add_arc(0, 1, 107);

    auto [graph, capacity] = builder.build();

    dinitz alg(graph, capacity, 0u, 1u);
    ASSERT_EQ(alg.run().flow_value(), 107);
    ASSERT_TRUE(EQ_MULTISETS(alg.minimum_cut(), {0u}));
    alg.reset();
}

GTEST_TEST(dinitz, test) {
    static_digraph_builder<static_digraph, int, char> builder(6);

    // example from https://www.geeksforgeeks.org/max-flow-problem-introduction/
    builder.add_arc(0, 1, 16, false);
    builder.add_arc(0, 2, 13, false);
    builder.add_arc(1, 2, 10, false);
    builder.add_arc(1, 3, 12, true);  //
    builder.add_arc(2, 1, 4, false);
    builder.add_arc(2, 4, 14, false);
    builder.add_arc(3, 2, 9, false);
    builder.add_arc(3, 5, 20, false);
    builder.add_arc(4, 3, 7, true);  //
    builder.add_arc(4, 5, 4, true);  //

    auto [graph, capacity, part_of_minimum_cut] = builder.build();

    dinitz alg(graph, capacity, 0u, 5u);
    ASSERT_EQ(alg.run().flow_value(), 23);
    ASSERT_TRUE(EQ_MULTISETS(
        alg.minimum_cut(), std::views::filter(arcs(graph), [&](const auto & a) {
            return part_of_minimum_cut[a];
        })));
    alg.reset();
}

#include "melon/utility/value_map.hpp"
#include "melon/views/complete_digraph.hpp"

GTEST_TEST(dinitz, complete_digraph_view) {
    auto graph = views::complete_digraph<>(5ul);
    dinitz alg(graph, views::map([](const auto & a) { return 1; }), 0ul, 1ul);
    ASSERT_EQ(alg.run().flow_value(), 4);
    alg.reset();
}
