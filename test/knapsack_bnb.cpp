#undef NDEBUG
#include <gtest/gtest.h>

#include <vector>

#include "melon/algorithm/knapsack_bnb.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// knapsack_bnb selects the value-maximal subset of items within the budget
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_bnb, test) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u};
    std::vector<int> values = {10, 7, 1, 3, 2};
    std::vector<int> costs = {9, 12, 2, 7, 5};
    int budget = 15;

    auto alg = knapsack_bnb(items, values, costs, budget);
    alg.run();

    ASSERT_TRUE(EQ_MULTISETS(alg.solution_items(), {0u, 4u}));
    ASSERT_EQ(alg.solution_value(), 12);
    ASSERT_EQ(alg.solution_cost(), 14);
}

////////////////////////////////////////////////////////////////////////////////
// moved-in ranges and a lambda cost map yield the same solution
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_bnb, test2) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u};
    std::vector<int> values = {10, 7, 1, 3, 2};
    std::vector<int> costs = {9, 12, 2, 7, 5};
    int budget = 15;

    auto alg = knapsack_bnb(
        std::move(items), std::move(values), [&](auto i) { return costs[i]; },
        budget);
    alg.run();

    ASSERT_TRUE(EQ_MULTISETS(alg.solution_items(), {0u, 4u}));
    ASSERT_EQ(alg.solution_value(), 12);
    ASSERT_EQ(alg.solution_cost(), 14);
}

////////////////////////////////////////////////////////////////////////////////
// set_budget re-derives the item filter. reset() excludes items costing more
// than the budget, so only assigning _budget keeps newly-affordable items
// excluded and run() returns a wrong "optimal" value after a raise.
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_bnb, set_budget_rederives_the_item_filter) {
    std::vector<std::size_t> items = {0u, 1u, 2u};
    std::vector<int> values = {10, 7, 1};
    std::vector<int> costs = {9, 12, 2};

    auto alg = knapsack_bnb(items, values, costs, 3);
    ASSERT_EQ(alg.run().solution_value(), 1);

    // at budget 23 every item fits; without the re-derivation, the stale
    // filter built at budget 3 keeps items 0 and 1 excluded and answers 1
    alg.set_budget(23);
    ASSERT_EQ(alg.run().solution_value(), 18);
    ASSERT_TRUE(EQ_MULTISETS(alg.solution_items(), {0u, 1u, 2u}));
}

// The bound takes the value/cost ratio in double before multiplying by the
// remaining budget: multiplied in the integer operand types first, this
// instance overflowed and a wrapped-negative bound pruned the branch holding
// the optimum.
GTEST_TEST(knapsack_bnb, bound_arithmetic_survives_wide_value_cost_products) {
    std::vector<unsigned int> items = {0u, 1u, 2u};
    std::vector<int> values = {3, 2, 100000};
    std::vector<int> costs = {1, 1, 50000};
    auto alg = knapsack_bnb(items, values, costs, 50000);
    ASSERT_EQ(alg.run().solution_value(), 100000);
}
