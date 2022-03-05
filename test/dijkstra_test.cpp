#include <gtest/gtest.h>

#include "melon/dijkstra.hpp"
#include "melon/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(Dijkstra, test) {
    static_digraphBuilder<int> builder(6);

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

    Dijkstra dijkstra(graph, length_map);

    dijkstra.add_source(0);
    ASSERT_FALSE(dijkstra.empty_queue());
    ASSERT_EQ(dijkstra.next_node(), std::make_pair(0u, 0));
    ASSERT_FALSE(dijkstra.empty_queue());
    ASSERT_EQ(dijkstra.next_node(), std::make_pair(1u, 7));
    ASSERT_FALSE(dijkstra.empty_queue());
    ASSERT_EQ(dijkstra.next_node(), std::make_pair(2u, 9));
    ASSERT_FALSE(dijkstra.empty_queue());
    ASSERT_EQ(dijkstra.next_node(), std::make_pair(5u, 11));
    ASSERT_FALSE(dijkstra.empty_queue());
    ASSERT_EQ(dijkstra.next_node(), std::make_pair(4u, 20));
    ASSERT_FALSE(dijkstra.empty_queue());
    ASSERT_EQ(dijkstra.next_node(), std::make_pair(3u, 21));
    ASSERT_TRUE(dijkstra.empty_queue());
}
