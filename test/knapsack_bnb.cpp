#undef NDEBUG
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <random>
#include <vector>

#include "melon/algorithm/knapsack_bnb.hpp"

#include "knapsack_test_helper.hpp"
#include "random_ranges_helper.hpp"
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

////////////////////////////////////////////////////////////////////////////////
// reset() discards the previous solution, as the constructor's state has none:
// the solution is iterators into the value/cost pairs reset() rebuilds, so a
// kept one reads invalidated entries -- MSVC's checked iterators abort there.
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_bnb, reset_discards_the_previous_solution) {
    std::vector<std::size_t> items = {0u, 1u, 2u};
    std::vector<int> values = {10, 7, 1};
    std::vector<int> costs = {9, 12, 2};

    auto alg = knapsack_bnb(items, values, costs, 23);
    ASSERT_EQ(alg.run().solution_value(), 18);

    alg.set_budget(3);
    ASSERT_TRUE(EMPTY(alg.solution_items()));
    ASSERT_EQ(alg.solution_value(), 0);
    ASSERT_EQ(alg.solution_cost(), 0);
    ASSERT_EQ(alg.run().solution_value(), 1);
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

////////////////////////////////////////////////////////////////////////////////
// the bound stays an upper bound in floating point. 1225 * (28 / 1372.0) is
// 24.999999999999996, so a bound truncated from the plain product reads 31
// where items 1 and 3 are worth 32, and their branch is pruned. The items are
// given in ratio order, the tied pair included, so no sort has a tie to move.
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_bnb, bound_is_not_truncated_below_an_attainable_value) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u};
    std::vector<int> values = {6, 7, 28, 25};
    std::vector<int> costs = {245, 294, 1372, 1225};

    auto alg = knapsack_bnb(items, values, costs, 1519);
    ASSERT_EQ(alg.run().solution_value(), 32);
    ASSERT_TRUE(EQ_MULTISETS(alg.solution_items(), {1u, 3u}));
}

////////////////////////////////////////////////////////////////////////////////
// run() is checked against a dynamic program over the budget on random
// instances. Zero values and zero costs are drawn on purpose, and the number
// of distinct (value, cost) pairs is what routes an instance: few of them
// repeat items, which are then searched as counted runs, while wide ranges
// leave every item on its own.
//
// A failure here names a seed: random_ranges_helper.hpp prints it and
// MELON_TEST_SEED replays it.
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_bnb, matches_the_reference_optimum) {
    for(std::size_t round = 0; round < NUM_INSTANCES; ++round) {
        const auto instance = random_instance(round);
        auto alg = knapsack_bnb(instance.items, instance.values, instance.costs,
                                instance.budget);
        alg.run();
        ASSERT_EQ(alg.solution_value(), reference_optimum(instance));
        ASSERT_TRUE(is_consistent_solution(alg, instance));
    }
}

////////////////////////////////////////////////////////////////////////////////
// set_relative_gap bounds how far below the optimum run() may stop:
// solution_value() >= optimum / (1 + gap)
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_bnb, relative_gap_bounds_the_distance_to_the_optimum) {
    for(std::size_t round = 0; round < NUM_INSTANCES; ++round) {
        const auto instance = random_instance(round);
        auto alg = knapsack_bnb(instance.items, instance.values, instance.costs,
                                instance.budget);
        alg.set_relative_gap(0.25).run();
        ASSERT_GE(5 * alg.solution_value(), 4 * reference_optimum(instance));
        ASSERT_TRUE(is_consistent_solution(alg, instance));
    }
}

////////////////////////////////////////////////////////////////////////////////
// The three tests below pin searches that are exponential without the device
// they name. They run under a timeout so that losing the device fails the
// test instead of hanging the suite; with it, each takes milliseconds.
////////////////////////////////////////////////////////////////////////////////

// 40 copies of each of 10 items in two classes of nearly equal ratio: taken
// one by one, the copies of an item are 2^40 subsets where only 41 counts
// differ, and the bound cannot tell the subsets of a class apart.
GTEST_TEST(knapsack_bnb, repeated_items_are_searched_as_counts) {
    const std::vector<int> distinct_costs = {101, 233, 347, 461, 587,
                                             619, 743, 811, 907, 983};
    knapsack_instance instance;
    int total_cost = 0;
    for(std::size_t i = 0; i < 400; ++i) {
        const int cost = distinct_costs[i % 10];
        instance.items.push_back(i);
        instance.costs.push_back(cost);
        instance.values.push_back((i % 10 < 5 ? 137 : 83) * cost / 100);
        total_cost += cost;
    }
    instance.budget = total_cost / 2;

    auto alg = knapsack_bnb(instance.items, instance.values, instance.costs,
                            instance.budget);
    ASSERT_TRUE(alg.run_with_timeout(std::chrono::seconds(30)));
    ASSERT_EQ(alg.solution_value(), 133406);
    ASSERT_TRUE(is_consistent_solution(alg, instance));
}

// More copies of an item than the budget holds: the copies in excess are
// never taken together, so totalling the whole run -- 3.2e9 here -- overflows
// a cost type wide enough for every solution.
GTEST_TEST(knapsack_bnb, repeated_items_beyond_the_budget_do_not_overflow) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};
    std::vector<int> values(8, 10);
    std::vector<int> costs(8, 400'000'000);

    auto alg = knapsack_bnb(items, values, costs, 1'000'000'000);
    ASSERT_EQ(alg.run().solution_value(), 20);
    ASSERT_EQ(alg.solution_cost(), 800'000'000);
    ASSERT_TRUE(SIZE(alg.solution_items(), 2));
}

// The copies of a floating-point item that fit are counted through a
// quotient, which rounds either way. Once the dear item is taken, what is
// left of the budget holds six copies, and divided by their cost it comes out
// just under 6: counting from the quotient alone leaves the sixth out.
GTEST_TEST(knapsack_bnb, floating_point_quotient_does_not_drop_a_copy) {
    const double cost = 3.8010616628802643;
    const double dear_cost = 7.0213700395553156;
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};
    std::vector<double> values(8, 1.0);
    std::vector<double> costs(8, cost);
    values[0] = 1000.0;
    costs[0] = dear_cost;
    const double budget = dear_cost + 6 * cost;
    ASSERT_LT((budget - dear_cost) / cost, 6.0);
    ASSERT_LE(6 * cost, budget - dear_cost);

    auto alg = knapsack_bnb(items, values, costs, budget);
    ASSERT_EQ(alg.run().solution_value(), 1006.0);
}

// Even costs under an odd budget: the last unit can never be spent, and a
// bound computed from the budget as given stays one above every solution.
GTEST_TEST(knapsack_bnb, budget_is_tightened_to_the_costs_common_divisor) {
    std::vector<std::size_t> items;
    std::vector<int> costs;
    for(std::size_t i = 0; i < 40; ++i) {
        items.push_back(i);
        costs.push_back(2 * static_cast<int>(i + 1));
    }

    auto alg = knapsack_bnb(items, costs, costs, 801);
    ASSERT_TRUE(alg.run_with_timeout(std::chrono::seconds(30)));
    ASSERT_EQ(alg.solution_value(), 800);
}

// Floating-point values in classes of equal ratio, no item repeated: the best
// fills differ from the bound by rounding error alone, so an exact search
// enumerates them without end and only the gap lets it stop. The gap is set
// before set_budget(), which re-derives everything else: a reset() dropping
// it would leave this instance exact, and running out the timeout.
GTEST_TEST(knapsack_bnb, relative_gap_ends_a_search_among_floating_point_ties) {
    std::uniform_real_distribution<double> cost_distr(1.0, 100.0);
    const std::vector<double> class_ratios = {1.9, 1.7, 1.3, 1.1, 0.7};
    std::vector<std::size_t> items;
    std::vector<double> values, costs;
    double total_cost = 0;
    for(std::size_t i = 0; i < 1000; ++i) {
        const double cost = cost_distr(test_rng());
        items.push_back(i);
        costs.push_back(cost);
        values.push_back(class_ratios[i % class_ratios.size()] * cost);
        total_cost += cost;
    }
    const double budget = total_cost / 2;

    auto alg = knapsack_bnb(items, values, costs, 0.0);
    alg.set_relative_gap(1e-6).set_budget(budget);
    ASSERT_TRUE(alg.run_with_timeout(std::chrono::seconds(30)));
    ASSERT_GT(alg.solution_value(), 0.0);
    // solution_cost() sums in another order than the search did
    ASSERT_LE(alg.solution_cost(), budget * (1 + 1e-12));
}
