#include <gtest/gtest.h>

#include "melon/algorithm/edmonds_karp.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(edmonds_karp, test) {
    static_digraph_builder<static_digraph, int, char> builder(6);

    // example from https://www.geeksforgeeks.org/max-flow-problem-introduction/
    builder.add_arc(0, 1, 16, false);
    builder.add_arc(0, 2, 13, false);
    builder.add_arc(1, 2, 10, false);
    builder.add_arc(1, 3, 12, true);
    builder.add_arc(2, 1, 4, false);
    builder.add_arc(2, 4, 14, false);
    builder.add_arc(3, 2, 9, false);
    builder.add_arc(3, 5, 20, false);
    builder.add_arc(4, 3, 7, true);
    builder.add_arc(4, 5, 4, true);

    auto [graph, capacity, part_of_min_cut] = builder.build();

    edmonds_karp alg(graph, capacity, 0u, 5u);
    ASSERT_EQ(alg.run().flow_value(), 23);
    ASSERT_TRUE(EQ_MULTISETS(
        alg.min_cut(), std::views::filter(arcs(graph), [&](const auto & a) {
            return part_of_min_cut[a];
        })));
    alg.reset();
}
