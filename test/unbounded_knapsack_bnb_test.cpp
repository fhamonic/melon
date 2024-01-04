#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/unbounded_knapsack_bnb.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(unbounded_knapsack_bnb, test) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u};
    std::vector<int> values = {10, 7, 1, 3, 2};
    std::vector<int> costs = {9, 12, 2, 7, 5};
    int budget = 15;

    auto alg = unbounded_knapsack_bnb(items, values, costs, budget);
    alg.run();

    ASSERT_TRUE(EQ_MULTISETS(alg.solution_items(), {std::pair{0u, 1}, std::pair{2u, 3}}));
    ASSERT_EQ(alg.solution_value(), 13);
    ASSERT_EQ(alg.solution_cost(), 15);
}

GTEST_TEST(unbounded_knapsack_bnb, test2) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u};
    std::vector<int> values = {10, 7, 1, 3, 2};
    std::vector<int> costs = {9, 12, 2, 7, 5};
    int budget = 15;

    auto alg = unbounded_knapsack_bnb(
        std::move(items), std::move(values), [&](auto i) { return costs[i]; },
        budget);
    alg.run();

    ASSERT_TRUE(EQ_MULTISETS(alg.solution_items(), {std::pair{0u, 1}, std::pair{2u, 3}}));
    ASSERT_EQ(alg.solution_value(), 13);
    ASSERT_EQ(alg.solution_cost(), 15);
}
