#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/connected_components.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/undirect.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(connected_components, test) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(0, 1)
        .add_arc(1, 2)
        .add_arc(1, 3)
        .add_arc(2, 0)
        .add_arc(4, 3)
        .add_arc(5, 6)
        .add_arc(6, 5)
        .add_arc(5, 6);

    auto [graph] = builder.build();

    auto ugraph = views::undirect(graph);

    connected_components alg(ugraph);
    // connected_components alg(views::undirect(graph));

    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {0u, 1u, 2u, 3u, 4u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {5u, 6u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {7u}));
    alg.advance();
    ASSERT_TRUE(alg.finished());
}
