#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/depth_first_search.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(depth_first_search, no_arcs_graph) {
    static_digraph_builder<static_digraph> builder(2);

    auto [graph] = builder.build();

    depth_first_search alg(graph, 0u);

    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 0u);
    alg.advance();
    ASSERT_TRUE(alg.finished());
    alg.reset();
}

GTEST_TEST(depth_first_search, test) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(0, 1)
        .add_arc(0, 2)
        .add_arc(0, 3)
        .add_arc(1, 3)
        .add_arc(2, 4)
        .add_arc(3, 5)
        .add_arc(3, 6)
        .add_arc(4, 7)
        .add_arc(4, 5)
        .add_arc(5, 2);

    auto [graph] = builder.build();

    depth_first_search alg(graph, 0u);

    static_assert(std::copyable<decltype(alg)>);

    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 0u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 1u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 3u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 5u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 2u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 4u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 7u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 6u);
    alg.advance();
    ASSERT_TRUE(alg.finished());
}

GTEST_TEST(depth_first_search, traversal_iterator) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(0, 1)
        .add_arc(0, 2)
        .add_arc(0, 3)
        .add_arc(1, 3)
        .add_arc(2, 4)
        .add_arc(3, 5)
        .add_arc(3, 6)
        .add_arc(4, 7)
        .add_arc(4, 5)
        .add_arc(5, 2);

    auto [graph] = builder.build();

    depth_first_search alg(graph, 0u);

    static_assert(std::ranges::input_range<decltype(alg)>);
    static_assert(std::ranges::viewable_range<decltype(alg)>);

    std::vector<vertex_t<static_digraph>> traversal = {0u, 1u, 3u, 5u,
                                                       2u, 4u, 7u, 6u};

    std::size_t cpt = 0;
    for(const auto v : alg) {
        ASSERT_EQ(v, traversal[cpt]);
        ++cpt;
    }
}