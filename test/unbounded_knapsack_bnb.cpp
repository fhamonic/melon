#undef NDEBUG
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <random>
#include <utility>
#include <vector>

#include "melon/algorithm/unbounded_knapsack_bnb.hpp"

#include "random_ranges_helper.hpp"
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

////////////////////////////////////////////////////////////////////////////////
// reset() discards the previous solution, as in knapsack_bnb
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(unbounded_knapsack_bnb, reset_discards_the_previous_solution) {
    std::vector<std::size_t> items = {0u, 1u};
    std::vector<int> values = {5, 4};
    std::vector<int> costs = {4, 3};

    auto alg = unbounded_knapsack_bnb(items, values, costs, 6);
    ASSERT_EQ(alg.run().solution_value(), 8);

    alg.set_budget(2);
    ASSERT_TRUE(EMPTY(alg.solution_items()));
    ASSERT_EQ(alg.solution_value(), 0);
    ASSERT_EQ(alg.solution_cost(), 0);
}

////////////////////////////////////////////////////////////////////////////////
// run() is checked against a table over the budget on random instances --
// identical items, zero values and items of equal ratio included -- and its
// solution against the instance: solution_value() and solution_cost() sum
// the search's own copies of the values, so only solution_items() shows a
// solution mapped back to the wrong items.
//
// A failure here names a seed: random_ranges_helper.hpp prints it and
// MELON_TEST_SEED replays it.
////////////////////////////////////////////////////////////////////////////////

namespace {

struct unbounded_instance {
    std::vector<std::size_t> items;
    std::vector<long long> values;
    std::vector<long long> costs;
    long long budget;
};

// From the engine's raw output: distributions differ between standard
// libraries, and some instances below are picked for how long they take.
long long draw(std::mt19937 & engine, long long low, long long high) {
    return low + static_cast<long long>(
                     engine() % static_cast<unsigned long>(high - low + 1));
}

long long reference_optimum(const unbounded_instance & instance) {
    std::vector<long long> best(static_cast<std::size_t>(instance.budget) + 1,
                                0);
    for(long long b = 1; b <= instance.budget; ++b) {
        const auto with = static_cast<std::size_t>(b);
        for(std::size_t i = 0; i < instance.items.size(); ++i) {
            if(instance.costs[i] > b) continue;
            const auto without =
                static_cast<std::size_t>(b - instance.costs[i]);
            best[with] =
                std::max(best[with], best[without] + instance.values[i]);
        }
    }
    return best[static_cast<std::size_t>(instance.budget)];
}

template <typename Knapsack>
testing::AssertionResult is_consistent_solution(
    const Knapsack & alg, const unbounded_instance & instance) {
    std::vector<std::size_t> taken;
    long long value = 0, cost = 0;
    for(auto && [i, count] : alg.solution_items()) {
        if(count == 0) return testing::AssertionFailure() << "an empty entry";
        taken.push_back(i);
        value += static_cast<long long>(count) * instance.values[i];
        cost += static_cast<long long>(count) * instance.costs[i];
    }
    std::ranges::sort(taken);
    if(std::ranges::adjacent_find(taken) != taken.end())
        return testing::AssertionFailure() << "an item is listed twice";
    if(value != alg.solution_value() || cost != alg.solution_cost())
        return testing::AssertionFailure()
               << "items sum to (" << value << "," << cost
               << "), accessors say (" << alg.solution_value() << ","
               << alg.solution_cost() << ")";
    if(cost > instance.budget)
        return testing::AssertionFailure() << "over budget";
    return testing::AssertionSuccess();
}

// Five classes of items, the values of a class within a rounding of one
// ratio: the bound of nearly every branch exceeds the best solution by less
// than one.
unbounded_instance near_tied_classes(unsigned seed, std::size_t num_items,
                                     long long max_cost, long long budget) {
    std::mt19937 engine(seed);
    const double class_ratios[] = {191.23456789, 171.98765432, 131.31313131,
                                   111.77777777, 71.123123123};
    unbounded_instance instance;
    for(std::size_t i = 0; i < num_items; ++i) {
        const long long cost = draw(engine, max_cost / 10, max_cost);
        instance.items.push_back(i);
        instance.costs.push_back(cost);
        instance.values.push_back(
            std::llround(class_ratios[i % 5] * static_cast<double>(cost)));
    }
    instance.budget = budget;
    return instance;
}

}  // namespace

GTEST_TEST(unbounded_knapsack_bnb, matches_the_reference_optimum) {
    for(std::size_t round = 0; round < 300; ++round) {
        std::mt19937 & engine = test_rng();
        const std::size_t num_items = 1 + engine() % 20;
        const long long max_cost = round % 2 == 0 ? 12 : 300;
        unbounded_instance instance;
        for(std::size_t i = 0; i < num_items; ++i) {
            const long long cost = draw(engine, 1, max_cost);
            instance.items.push_back(i);
            instance.costs.push_back(cost);
            instance.values.push_back(round % 3 == 0 ? 3 * cost
                                      : round % 3 == 1
                                          ? draw(engine, 0, max_cost)
                                          : cost + max_cost / 10);
            if(i > 0 && engine() % 8 == 0) {
                instance.costs[i] = instance.costs[i - 1];
                instance.values[i] = instance.values[i - 1];
            }
        }
        instance.budget = draw(engine, 0, 20 * max_cost);

        auto alg = unbounded_knapsack_bnb(instance.items, instance.values,
                                          instance.costs, instance.budget);
        alg.run();
        ASSERT_EQ(alg.solution_value(), reference_optimum(instance));
        ASSERT_TRUE(is_consistent_solution(alg, instance));
    }
}

////////////////////////////////////////////////////////////////////////////////
// reset() drops an item when copies of a cheaper one, as many as its cost
// pays for, are worth as much
////////////////////////////////////////////////////////////////////////////////

// One copy of the item of cost 3 is worth less than the item of cost 4, and a
// copy and a third would be worth more: counted in fractions of a copy, the
// item of cost 4 is dropped and the answer is 5.
GTEST_TEST(unbounded_knapsack_bnb, dominating_copies_are_whole) {
    std::vector<std::size_t> items = {0u, 1u};
    std::vector<double> values = {5.0, 6.0};
    std::vector<double> costs = {3.0, 4.0};

    auto alg = unbounded_knapsack_bnb(items, values, costs, 4.0);
    ASSERT_EQ(alg.run().solution_value(), 6.0);
}

// Of two identical items the first in the range is the one kept, whatever
// the standard library's sort does with equal elements.
GTEST_TEST(unbounded_knapsack_bnb, first_of_identical_items_is_kept) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u};
    std::vector<int> values = {5, 5, 5, 5};
    std::vector<int> costs = {4, 4, 4, 4};

    auto alg = unbounded_knapsack_bnb(items, values, costs, 9);
    alg.run();
    ASSERT_TRUE(EQ_MULTISETS(alg.solution_items(), {std::pair{0u, 2}}));
}

////////////////////////////////////////////////////////////////////////////////
// the bound is truncated for integral values, from a ratio rounded upward
////////////////////////////////////////////////////////////////////////////////

// The three items share the ratio 1/49, so their bounds are whole numbers --
// but 1470 * (5 / 245.0) is 29.999999999999996. Truncated from a ratio that
// is not rounded upward, a bound reads one too low: the six copies of the
// first item, worth 30, are pruned and 29 is answered.
GTEST_TEST(unbounded_knapsack_bnb,
           bound_is_not_truncated_below_an_attainable_value) {
    std::vector<std::size_t> items = {0u, 1u, 2u};
    std::vector<int> values = {5, 17, 12};
    std::vector<int> costs = {245, 833, 588};

    auto alg = unbounded_knapsack_bnb(items, values, costs, 1480);
    ASSERT_EQ(alg.run().solution_value(), 30);
}

////////////////////////////////////////////////////////////////////////////////
// The three tests below run searches that take milliseconds, under a timeout
// so that losing what keeps them short fails the test instead of hanging the
// suite.
////////////////////////////////////////////////////////////////////////////////

// 10 000 items in classes of nearly equal ratio: compared as a real number,
// the bound of a branch that cannot gain a whole unit still exceeds the best
// solution, and the branch is searched.
GTEST_TEST(unbounded_knapsack_bnb, bound_is_truncated_for_integral_values) {
    const auto instance = near_tied_classes(1u, 10'000, 100'000, 10'000'000);
    auto alg = unbounded_knapsack_bnb(instance.items, instance.values,
                                      instance.costs, instance.budget);
    ASSERT_TRUE(alg.run_with_timeout(std::chrono::seconds(1)));
    ASSERT_TRUE(is_consistent_solution(alg, instance));
}

// Every cost a multiple of 10 under a budget that is not: the last 5 can
// never be spent, and a bound computed from the budget as given stays above
// every solution.
GTEST_TEST(unbounded_knapsack_bnb,
           budget_is_tightened_to_the_costs_common_divisor) {
    auto instance = near_tied_classes(1u, 100, 1000, 0);
    for(long long & cost : instance.costs) cost *= 10;
    instance.budget = 1'000'005;

    auto alg = unbounded_knapsack_bnb(instance.items, instance.values,
                                      instance.costs, instance.budget);
    ASSERT_TRUE(alg.run_with_timeout(std::chrono::seconds(1)));
    ASSERT_EQ(alg.solution_value(), reference_optimum(instance));
    ASSERT_TRUE(is_consistent_solution(alg, instance));
}

// 1e11 copies of the first item, then one item of cost 7, fill the budget
// and are optimal: every other solution gives up a copy of the first item
// for less. The search has to back out of those copies to prove it, and one
// by one that is 1e11 steps.
GTEST_TEST(unbounded_knapsack_bnb, copies_are_given_up_all_at_once) {
    std::vector<std::size_t> items = {0u, 1u, 2u};
    std::vector<long long> values = {10, 6, 2};
    std::vector<long long> costs = {10, 7, 3};
    const long long budget = 1'000'000'000'007;

    auto alg = unbounded_knapsack_bnb(items, values, costs, budget);
    ASSERT_TRUE(alg.run_with_timeout(std::chrono::seconds(1)));
    ASSERT_EQ(alg.solution_value(), 1'000'000'000'006);
    ASSERT_EQ(alg.solution_cost(), budget);
}

////////////////////////////////////////////////////////////////////////////////
// floating-point costs: the copies that fit are counted through a quotient,
// which rounds either way
////////////////////////////////////////////////////////////////////////////////

// Seven copies cost exactly the budget, and the budget over the cost comes
// out as 6.999999999999999.
GTEST_TEST(unbounded_knapsack_bnb,
           floating_point_quotient_does_not_drop_a_copy) {
    const double cost = 2.411;
    std::vector<std::size_t> items = {0u};
    std::vector<double> values = {1.0};
    std::vector<double> costs = {cost};
    const double budget = 7 * cost;
    ASSERT_LT(budget / cost, 7.0);

    auto alg = unbounded_knapsack_bnb(items, values, costs, budget);
    ASSERT_EQ(alg.run().solution_value(), 7.0);
    ASSERT_LE(alg.solution_cost(), budget);
}

////////////////////////////////////////////////////////////////////////////////
// run_with_timeout stops the search and leaves a valid solution behind. The
// values are the costs plus a fixed charge, which the bound cannot tell
// apart: this search lasts about a second, and stopped it must return long
// before that -- a longer margin would not tell it from a search that
// ignores the request and runs to its end.
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(unbounded_knapsack_bnb,
           stopped_search_returns_promptly_with_its_incumbent) {
    std::mt19937 engine(2u);
    unbounded_instance instance;
    for(std::size_t i = 0; i < 700; ++i) {
        const long long cost = draw(engine, 1000, 10'000);
        instance.items.push_back(i);
        instance.costs.push_back(cost);
        instance.values.push_back(cost + 1000);
    }
    instance.budget = 1'000'000;

    auto alg = unbounded_knapsack_bnb(instance.items, instance.values,
                                      instance.costs, instance.budget);
    const auto start = std::chrono::steady_clock::now();
    ASSERT_FALSE(alg.run_with_timeout(std::chrono::milliseconds(1)));
    ASSERT_LT(std::chrono::steady_clock::now() - start,
              std::chrono::milliseconds(500));
    ASSERT_GT(alg.solution_value(), 0);
    ASSERT_TRUE(is_consistent_solution(alg, instance));
}
