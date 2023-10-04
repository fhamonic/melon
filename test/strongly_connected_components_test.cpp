#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/strongly_connected_components.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(strongly_connected_components, test) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(0, 1)
        .add_arc(0, 2)
        .add_arc(0, 5)
        .add_arc(1, 0)
        .add_arc(1, 2)
        .add_arc(1, 3)
        .add_arc(2, 0)
        .add_arc(2, 1)
        .add_arc(2, 3)
        .add_arc(2, 5)
        .add_arc(3, 1)
        .add_arc(3, 2)
        .add_arc(3, 4)
        .add_arc(4, 3)
        .add_arc(4, 5)
        .add_arc(5, 0)
        .add_arc(5, 2)
        .add_arc(5, 4)
        .add_arc(7, 5);

    auto [graph] = builder.build();

    strongly_connected_components alg(graph);

    alg.run();
}
