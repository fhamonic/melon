#include <gtest/gtest.h>

#include "melon/algorithm/bidirectional_dijkstra.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/container/static_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(bidirectional_dijkstra, test) {
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

    auto [graph, length_map] = builder.build();

    bidirectional_dijkstra alg(graph, length_map, 0u, 3u);
    ASSERT_EQ(alg.run(), 21);
    ASSERT_TRUE(alg.path_found());
    ASSERT_TRUE(EQ_MULTISETS(alg.path(), {1, 8}));
    alg.reset();
}
