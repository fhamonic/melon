#ifndef KNAPSACK_TEST_HELPER_HPP
#define KNAPSACK_TEST_HELPER_HPP

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <vector>

#include "random_ranges_helper.hpp"

// Shared by the knapsack solvers' tests: they have one interface, and are
// checked on the same instances against the same reference.

struct knapsack_instance {
    std::vector<std::size_t> items;
    std::vector<int> values;
    std::vector<int> costs;
    int budget;
};

inline knapsack_instance random_instance(std::size_t num_items,
                                         std::size_t num_distinct,
                                         int max_value, int max_cost,
                                         bool tied_ratios) {
    const auto distinct_values = random_vector<int>(num_distinct, 0, max_value);
    const auto distinct_costs = random_vector<int>(num_distinct, 0, max_cost);
    knapsack_instance instance;
    int total_cost = 0;
    for(std::size_t i = 0; i < num_items; ++i) {
        const std::size_t drawn = test_rng()() % num_distinct;
        instance.items.push_back(i);
        instance.costs.push_back(distinct_costs[drawn]);
        instance.values.push_back(tied_ratios ? 3 * distinct_costs[drawn]
                                              : distinct_values[drawn]);
        total_cost += distinct_costs[drawn];
    }
    instance.budget =
        static_cast<int>(test_rng()() % static_cast<unsigned>(total_cost + 2));
    return instance;
}

inline knapsack_instance random_instance(std::size_t round) {
    const std::size_t num_items = 1 + test_rng()() % 30;
    const bool tied_ratios = round % 3 == 0;
    if(round % 2 == 0)
        return random_instance(num_items, 1 + test_rng()() % 4, 30, 20,
                               tied_ratios);
    return random_instance(num_items, num_items, 1000, 200, tied_ratios);
}

inline int reference_optimum(const knapsack_instance & instance) {
    std::vector<int> best(static_cast<std::size_t>(instance.budget) + 1, 0);
    for(std::size_t i = 0; i < instance.items.size(); ++i) {
        const int cost = instance.costs[i];
        for(int b = instance.budget; b >= cost; --b) {
            const auto with = static_cast<std::size_t>(b);
            const auto without = static_cast<std::size_t>(b - cost);
            best[with] =
                std::max(best[with], best[without] + instance.values[i]);
        }
    }
    return best.back();
}

// The accessors agree with each other and with the instance: solution_value()
// and solution_cost() sum the search's own copies of the values, so only
// solution_items() shows a solution mapped back to the wrong items, or to the
// same copy of a repeated item twice.
template <typename Knapsack>
testing::AssertionResult is_consistent_solution(
    const Knapsack & alg, const knapsack_instance & instance) {
    std::vector<std::size_t> taken;
    int value = 0, cost = 0;
    for(auto && i : alg.solution_items()) {
        taken.push_back(i);
        value += instance.values[i];
        cost += instance.costs[i];
    }
    std::ranges::sort(taken);
    if(std::ranges::adjacent_find(taken) != taken.end())
        return testing::AssertionFailure() << "an item is taken twice";
    if(value != alg.solution_value() || cost != alg.solution_cost())
        return testing::AssertionFailure()
               << "items sum to (" << value << "," << cost
               << "), accessors say (" << alg.solution_value() << ","
               << alg.solution_cost() << ")";
    if(cost > instance.budget)
        return testing::AssertionFailure() << "over budget";
    return testing::AssertionSuccess();
}

inline constexpr std::size_t NUM_INSTANCES = 300;

#endif  // KNAPSACK_TEST_HELPER_HPP
