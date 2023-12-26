#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/topological_sort.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(topological_sort, no_arcs_graph) {
    static_digraph_builder<static_digraph> builder(2);

    auto [graph] = builder.build();

    topological_sort alg(graph);

    static_assert(std::copyable<decltype(alg)>);

    std::vector<vertex_t<static_digraph>> traversal = {0u, 1u};

    std::size_t cpt = 0;
    for(const auto v : alg) {
        ASSERT_EQ(v, traversal[cpt]);
        cpt++;
    }

    for(const auto v : alg) {
        ASSERT_EQ(v, traversal[cpt]);
        cpt++;
    }
}

GTEST_TEST(topological_sort, test) {
    static_digraph_builder<static_digraph> builder(6);

    builder.add_arc(5, 2)
        .add_arc(5, 0)
        .add_arc(4, 0)
        .add_arc(4, 1)
        .add_arc(2, 3)
        .add_arc(3, 1);

    auto [graph] = builder.build();

    topological_sort alg(graph);

     ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 4u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 5u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 0u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 2u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 3u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 1u);
    alg.advance();
    ASSERT_TRUE(alg.finished());
}

GTEST_TEST(topological_sort, algorithm_iterator) {
    static_digraph_builder<static_digraph> builder(6);

    builder.add_arc(5, 2)
        .add_arc(5, 0)
        .add_arc(4, 0)
        .add_arc(4, 1)
        .add_arc(2, 3)
        .add_arc(3, 1);

    auto [graph] = builder.build();

    topological_sort alg(graph);

    static_assert(std::ranges::input_range<decltype(alg)>);
    static_assert(std::ranges::viewable_range<decltype(alg)>);

    std::vector<vertex_t<static_digraph>> traversal = {4u, 5u, 0u, 2u, 3u, 1u};

    std::size_t cpt = 0;
    for(const auto v : alg) {
        ASSERT_EQ(v, traversal[cpt]);
        ++cpt;
    }
}