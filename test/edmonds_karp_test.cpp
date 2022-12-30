#include <gtest/gtest.h>

#include "melon/algorithm/edmonds_karp.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/container/static_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(edmonds_karp, test) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 16);
    builder.add_arc(0, 2, 13);
    builder.add_arc(1, 2, 10);
    builder.add_arc(1, 3, 12);
    builder.add_arc(2, 1, 4);
    builder.add_arc(2, 4, 14);
    builder.add_arc(3, 2, 9);
    builder.add_arc(3, 5, 20);
    builder.add_arc(4, 3, 7);
    builder.add_arc(4, 5, 4);

    auto [graph, capacity] = builder.build();

    edmonds_karp alg(graph, capacity, 0u, 5u);
    ASSERT_EQ(alg.run().flow_value(), 23);
    alg.reset();
}
