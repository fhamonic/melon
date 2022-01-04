#include <gtest/gtest.h>

#include "melon/dijkstra.hpp"
#include "melon/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(Dijkstra, test) {
    StaticDigraphBuilder<int> builder(6);

    builder.addArc(0, 1, 7);
    builder.addArc(0, 2, 9);
    builder.addArc(0, 5, 14);
    builder.addArc(1, 0, 7);
    builder.addArc(1, 2, 10);
    builder.addArc(1, 3, 15);
    builder.addArc(2, 0, 9);
    builder.addArc(2, 1, 10);
    builder.addArc(2, 3, 12);
    builder.addArc(2, 5, 2);
    builder.addArc(3, 1, 15);
    builder.addArc(3, 2, 12);
    builder.addArc(3, 4, 6);
    builder.addArc(4, 3, 6);
    builder.addArc(4, 5, 9);
    builder.addArc(5, 0, 14);
    builder.addArc(5, 2, 2);
    builder.addArc(5, 4, 9);

    auto [graph, length_map] = builder.build();

    Dijkstra dijkstra(graph, length_map);

    dijkstra.init(0);
    ASSERT_FALSE(dijkstra.emptyQueue());
    ASSERT_EQ(dijkstra.processNextNode(), std::make_pair(0u, 0));
    ASSERT_FALSE(dijkstra.emptyQueue());
    ASSERT_EQ(dijkstra.processNextNode(), std::make_pair(1u, 7));
    ASSERT_FALSE(dijkstra.emptyQueue());
    ASSERT_EQ(dijkstra.processNextNode(), std::make_pair(2u, 9));
    ASSERT_FALSE(dijkstra.emptyQueue());
    ASSERT_EQ(dijkstra.processNextNode(), std::make_pair(5u, 11));
    ASSERT_FALSE(dijkstra.emptyQueue());
    ASSERT_EQ(dijkstra.processNextNode(), std::make_pair(4u, 20));
    ASSERT_FALSE(dijkstra.emptyQueue());
    ASSERT_EQ(dijkstra.processNextNode(), std::make_pair(3u, 21));
    ASSERT_TRUE(dijkstra.emptyQueue());
}
