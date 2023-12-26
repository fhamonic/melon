#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/kruskal.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/undirect.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(kruskal, test) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder
        .add_arc(0, 1, 7)    // 0
        .add_arc(0, 2, 9)    // 1
        .add_arc(0, 5, 14)   // 2
        .add_arc(1, 2, 10)   // 3
        .add_arc(1, 3, 15)   // 4
        .add_arc(2, 3, 12)   // 5
        .add_arc(2, 5, 2)    // 6
        .add_arc(3, 4, 6)    // 7
        .add_arc(4, 5, 11);  // 8

    auto [graph, cost_map] = builder.build();
    auto ugraph = views::undirect(graph);

    kruskal alg(ugraph, cost_map);

    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 6);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 7);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 0);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 1);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 8);
    alg.advance();
    ASSERT_TRUE(alg.finished());
    alg.reset();
}
