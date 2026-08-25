#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/unbounded_knapsack_bnb.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// unbounded_knapsack_bnb selects items with multiplicities, maximizing value
// within the budget
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(unbounded_knapsack_bnb, test) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u};
    std::vector<int> values = {10, 7, 1, 3, 2};
    std::vector<int> costs = {9, 12, 2, 7, 5};
    int budget = 15;

    auto alg = unbounded_knapsack_bnb(items, values, costs, budget);
    alg.run();

    ASSERT_TRUE(EQ_MULTISETS(alg.solution_items(),
                             {std::pair{0u, 1}, std::pair{2u, 3}}));
    ASSERT_EQ(alg.solution_value(), 13);
    ASSERT_EQ(alg.solution_cost(), 15);
}

////////////////////////////////////////////////////////////////////////////////
// moved-in ranges and a lambda cost map yield the same solution
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(unbounded_knapsack_bnb, test2) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u};
    std::vector<int> values = {10, 7, 1, 3, 2};
    std::vector<int> costs = {9, 12, 2, 7, 5};
    int budget = 15;

    auto alg = unbounded_knapsack_bnb(
        std::move(items), std::move(values), [&](auto i) { return costs[i]; },
        budget);
    alg.run();

    ASSERT_TRUE(EQ_MULTISETS(alg.solution_items(),
                             {std::pair{0u, 1}, std::pair{2u, 3}}));
    ASSERT_EQ(alg.solution_value(), 13);
    ASSERT_EQ(alg.solution_cost(), 15);
}

////////////////////////////////////////////////////////////////////////////////
// set_budget re-derives the item filter, as in knapsack_bnb
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(unbounded_knapsack_bnb, set_budget_rederives_the_item_filter) {
    std::vector<std::size_t> items = {0u, 1u};
    std::vector<int> values = {5, 4};
    std::vector<int> costs = {4, 3};

    auto alg = unbounded_knapsack_bnb(items, values, costs, 2);
    ASSERT_EQ(alg.run().solution_value(), 0);

    // at budget 6 two copies of item 1 are optimal; without the
    // re-derivation, the stale filter built at budget 2 holds no items at all
    // and answers 0 again
    alg.set_budget(6);
    ASSERT_EQ(alg.run().solution_value(), 8);
    ASSERT_TRUE(EQ_MULTISETS(alg.solution_items(), {std::pair{1u, 2}}));
}
