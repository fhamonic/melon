#include <gtest/gtest.h>

#include "melon/algorithm/robust_fiber.hpp"
#include "melon/arc_list_builder.hpp"
#include "melon/static_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(RobustFiber, test) {
    arc_list_builder<static_digraph, int, int> builder(8);

    builder.add_arc(0, 1, 1, 2);
    builder.add_arc(0, 6, 1, 2);
    builder.add_arc(0, 7, 3, 3);
    builder.add_arc(1, 0, 1, 2);
    builder.add_arc(1, 2, 4, 4);
    builder.add_arc(2, 1, 4, 4);
    builder.add_arc(2, 3, 2, 3);
    builder.add_arc(2, 7, 1, 3);
    builder.add_arc(3, 2, 2, 3);
    builder.add_arc(3, 4, 1, 2);
    builder.add_arc(3, 5, 1, 3);
    builder.add_arc(4, 3, 1, 2);
    builder.add_arc(4, 5, 1, 3);
    builder.add_arc(5, 3, 1, 3);
    builder.add_arc(5, 4, 1, 3);
    builder.add_arc(5, 6, 2, 2);
    builder.add_arc(5, 7, 1, 2);
    builder.add_arc(6, 0, 1, 2);
    builder.add_arc(6, 5, 2, 2);
    builder.add_arc(7, 0, 3, 3);
    builder.add_arc(7, 2, 1, 3);
    builder.add_arc(7, 5, 1, 2);

    auto [graph, reduced_length_map, length_map] = builder.build();

    std::vector<static_digraph::vertex_t> strong_nodes;
    std::vector<static_digraph::vertex_t> weak_nodes;

    RobustFiber algo(
        graph, reduced_length_map, length_map,
        [&strong_nodes](auto && v) { strong_nodes.push_back(v); },
        [&weak_nodes](auto && v) { weak_nodes.push_back(v); });

    algo.add_strong_arc_source(1).run();

    ASSERT_EQ_RANGES(strong_nodes, {6, 5, 4});
    ASSERT_EQ_RANGES(weak_nodes, {0, 1, 7, 2, 3});
}
