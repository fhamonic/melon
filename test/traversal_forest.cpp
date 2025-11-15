#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/traversal_forest.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/subgraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(traversal_forest, test) {
    static_digraph_builder<static_digraph> builder(4);

    builder.add_arc(0, 1).add_arc(2, 1);

    auto [graph] = builder.build();

    traversal_forest alg(graph);

    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {0u, 1u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {2u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {3u}));
    alg.advance();
    ASSERT_TRUE(alg.finished());
}
