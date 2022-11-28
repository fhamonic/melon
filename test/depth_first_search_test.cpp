#include <gtest/gtest.h>

#include "melon/algorithm/depth_first_search.hpp"
#include "melon/static_digraph.hpp"
#include "melon/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(depth_first_search, no_arcs_graph) {
    static_digraph_builder<static_digraph> builder(2);

    auto [graph] = builder.build();

    depth_first_search alg(graph, 0u);

    ASSERT_FALSE(alg.empty_queue());
    ASSERT_EQ(alg.next_entry(), 0u);
    ASSERT_TRUE(alg.empty_queue());
}

GTEST_TEST(depth_first_search, test) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(0, 1);
    builder.add_arc(0, 2);
    builder.add_arc(0, 5);
    builder.add_arc(1, 0);
    builder.add_arc(1, 2);
    builder.add_arc(1, 3);
    builder.add_arc(2, 0);
    builder.add_arc(2, 1);
    builder.add_arc(2, 3);
    builder.add_arc(2, 5);
    builder.add_arc(3, 1);
    builder.add_arc(3, 2);
    builder.add_arc(3, 4);
    builder.add_arc(4, 3);
    builder.add_arc(4, 5);
    builder.add_arc(5, 0);
    builder.add_arc(5, 2);
    builder.add_arc(5, 4);
    builder.add_arc(7, 5);

    auto [graph] = builder.build();

    depth_first_search alg(graph, 0u);

    ASSERT_FALSE(alg.empty_queue());
    ASSERT_EQ(alg.next_entry(), 0u);
    ASSERT_FALSE(alg.empty_queue());
    ASSERT_EQ(alg.next_entry(), 5u);
    ASSERT_FALSE(alg.empty_queue());
    ASSERT_EQ(alg.next_entry(), 4u);
    ASSERT_FALSE(alg.empty_queue());
    ASSERT_EQ(alg.next_entry(), 3u);
    ASSERT_FALSE(alg.empty_queue());
    ASSERT_EQ(alg.next_entry(), 2u);
    ASSERT_FALSE(alg.empty_queue());
    ASSERT_EQ(alg.next_entry(), 1u);
    ASSERT_TRUE(alg.empty_queue());
}
