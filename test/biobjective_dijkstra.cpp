#undef NDEBUG
#include <gtest/gtest.h>

#include <format>
#include <print>

#include "melon/algorithm/biobjective_dijkstra.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(biobjective_dijkstra, domination) {
    static_digraph_builder<static_digraph, int, int> builder(1);
    auto [graph, blue_length_map, red_length_map] = builder.build();

    biobjective_dijkstra alg(graph, blue_length_map, red_length_map);

    alg.add_source(0u, 1, 1);
    ASSERT_TRUE(alg.is_dominated(0u, std::make_pair(2, 2)));
    ASSERT_TRUE(alg.is_dominated(0u, std::make_pair(2, 1)));
    ASSERT_TRUE(alg.is_dominated(0u, std::make_pair(1, 2)));
    ASSERT_FALSE(alg.is_dominated(0u, std::make_pair(1, 1)));
    ASSERT_FALSE(alg.is_dominated(0u, std::make_pair(1, 0)));
    ASSERT_FALSE(alg.is_dominated(0u, std::make_pair(0, 1)));
    ASSERT_FALSE(alg.is_dominated(0u, std::make_pair(0, 0)));
    ASSERT_FALSE(alg.is_dominated(0u, std::make_pair(0, 2)));
    ASSERT_FALSE(alg.is_dominated(0u, std::make_pair(2, 0)));
}

GTEST_TEST(biobjective_dijkstra, relax) {
    static_digraph_builder<static_digraph, int, int> builder(9);
    auto [graph, blue_length_map, red_length_map] = builder.build();

    biobjective_dijkstra alg(graph, blue_length_map, red_length_map);

    for(auto && v : vertices(graph)) alg.add_source(v, 1, 1);

    alg.relax(0u, std::make_pair(2, 2));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(0u), {std::make_pair(1, 1)}));
    alg.relax(1u, std::make_pair(2, 1));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(1u), {std::make_pair(1, 1)}));
    alg.relax(2u, std::make_pair(1, 2));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(2u), {std::make_pair(1, 1)}));
    alg.relax(3u, std::make_pair(1, 1));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(3u), {std::make_pair(1, 1)}));
    alg.relax(4u, std::make_pair(1, 0));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(4u), {std::make_pair(1, 0)}));
    alg.relax(5u, std::make_pair(0, 1));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(5u), {std::make_pair(0, 1)}));
    alg.relax(6u, std::make_pair(0, 0));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(6u), {std::make_pair(0, 0)}));
    alg.relax(7u, std::make_pair(0, 2));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(7u),
                             {std::make_pair(1, 1), std::make_pair(0, 2)}));
    alg.relax(8u, std::make_pair(2, 0));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(8u),
                             {std::make_pair(1, 1), std::make_pair(2, 0)}));
}

GTEST_TEST(biobjective_dijkstra, test) {
    static_digraph_builder<static_digraph, int, int> builder(10);

    // https://hal.science/hal-03162962/document
    builder.add_arc(0u, 2u, 1, 4);
    builder.add_arc(0u, 4u, 2, 2);
    builder.add_arc(0u, 5u, 4, 1);
    builder.add_arc(0u, 7u, 1, 11);
    builder.add_arc(1u, 9u, 1, 0);
    builder.add_arc(2u, 3u, 5, 0);
    builder.add_arc(2u, 6u, 3, 0);
    builder.add_arc(2u, 9u, 1, 4);
    builder.add_arc(3u, 9u, 1, 2);
    builder.add_arc(4u, 9u, 2, 3);
    builder.add_arc(5u, 7u, 5, 9);
    builder.add_arc(5u, 9u, 4, 1);
    builder.add_arc(6u, 1u, 2, 0);
    builder.add_arc(7u, 8u, 1, 1);
    builder.add_arc(8u, 9u, 0, 0);

    auto [graph, blue_length_map, red_length_map] = builder.build();

    biobjective_dijkstra alg(graph, blue_length_map, red_length_map);
    alg.add_source(0u);

    // for(auto && [v, label] : alg) {
    //     std::print("{} : ({},{})\n", v, label.first, label.second);
    // }

    alg.run();
    ASSERT_TRUE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(0u), {std::make_pair(0, 0)}));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(1u), {std::make_pair(6, 4)}));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(2u), {std::make_pair(1, 4)}));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(3u), {std::make_pair(6, 4)}));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(4u), {std::make_pair(2, 2)}));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(5u), {std::make_pair(4, 1)}));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(6u), {std::make_pair(4, 4)}));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(7u),
                             {std::make_pair(9, 10), std::make_pair(1, 11)}));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(8u),
                             {std::make_pair(2, 12), std::make_pair(10, 11)}));
    ASSERT_TRUE(EQ_MULTISETS(alg.pareto_front(9u),
                             {std::make_pair(2, 8), std::make_pair(4, 5),
                              std::make_pair(7, 4), std::make_pair(8, 2)}));
    alg.reset();
}
