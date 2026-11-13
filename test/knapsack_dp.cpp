#undef NDEBUG
#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

#include "melon/algorithm/knapsack_bnb.hpp"
#include "melon/algorithm/knapsack_dp.hpp"

#include "knapsack_test_helper.hpp"
#include "random_ranges_helper.hpp"
#include "ranges_test_helper.hpp"

using namespace melon;

namespace {

// From the engine's raw output: distributions differ between standard
// libraries, and several instances below are picked for how long they take.
long long draw(std::mt19937 & engine, long long low, long long high) {
    return low + static_cast<long long>(
                     engine() % static_cast<unsigned long>(high - low + 1));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// knapsack_dp selects the value-maximal subset of items within the budget,
// through the interface of knapsack_bnb
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_dp, test) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u};
    std::vector<int> values = {10, 7, 1, 3, 2};
    std::vector<int> costs = {9, 12, 2, 7, 5};
    int budget = 15;

    auto alg = knapsack_dp(items, values, costs, budget);
    alg.run();

    ASSERT_TRUE(EQ_MULTISETS(alg.solution_items(), {0u, 4u}));
    ASSERT_EQ(alg.solution_value(), 12);
    ASSERT_EQ(alg.solution_cost(), 14);
}

////////////////////////////////////////////////////////////////////////////////
// moved-in ranges and a lambda cost map yield the same solution
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_dp, test2) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u};
    std::vector<int> values = {10, 7, 1, 3, 2};
    std::vector<int> costs = {9, 12, 2, 7, 5};
    int budget = 15;

    auto alg = knapsack_dp(
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

GTEST_TEST(knapsack_dp, set_budget_rederives_the_item_filter) {
    std::vector<std::size_t> items = {0u, 1u, 2u};
    std::vector<int> values = {10, 7, 1};
    std::vector<int> costs = {9, 12, 2};

    auto alg = knapsack_dp(items, values, costs, 3);
    ASSERT_EQ(alg.run().solution_value(), 1);

    alg.set_budget(23);
    ASSERT_EQ(alg.run().solution_value(), 18);
    ASSERT_TRUE(EQ_MULTISETS(alg.solution_items(), {0u, 1u, 2u}));
}

////////////////////////////////////////////////////////////////////////////////
// reset() discards the previous solution, as the constructor's state has none:
// the solution is iterators into the value/cost pairs reset() rebuilds, so a
// kept one reads invalidated entries -- MSVC's checked iterators abort there.
// A second run() then answers from scratch.
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_dp, reset_discards_the_previous_solution) {
    std::vector<std::size_t> items = {0u, 1u, 2u};
    std::vector<int> values = {10, 7, 1};
    std::vector<int> costs = {9, 12, 2};

    auto alg = knapsack_dp(items, values, costs, 23);
    ASSERT_EQ(alg.run().solution_value(), 18);
    ASSERT_EQ(alg.run().solution_value(), 18);

    alg.set_budget(3);
    ASSERT_TRUE(EMPTY(alg.solution_items()));
    ASSERT_EQ(alg.solution_value(), 0);
    ASSERT_EQ(alg.solution_cost(), 0);
    ASSERT_EQ(alg.run().solution_value(), 1);
}

////////////////////////////////////////////////////////////////////////////////
// run() is checked against a dynamic program over the budget on random
// instances -- zero values, zero costs and repeated items included -- and its
// solution against the instance: the value alone would hide a solution
// rebuilt from the wrong chunks.
//
// A failure here names a seed: random_ranges_helper.hpp prints it and
// MELON_TEST_SEED replays it.
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_dp, matches_the_reference_optimum) {
    for(std::size_t round = 0; round < NUM_INSTANCES; ++round) {
        const auto instance = random_instance(round);
        auto alg = knapsack_dp(instance.items, instance.values, instance.costs,
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

GTEST_TEST(knapsack_dp, relative_gap_bounds_the_distance_to_the_optimum) {
    for(std::size_t round = 0; round < NUM_INSTANCES; ++round) {
        const auto instance = random_instance(round);
        auto alg = knapsack_dp(instance.items, instance.values, instance.costs,
                               instance.budget);
        alg.set_relative_gap(0.25).run();
        ASSERT_GE(5 * alg.solution_value(), 4 * reference_optimum(instance));
        ASSERT_TRUE(is_consistent_solution(alg, instance));
    }
}

////////////////////////////////////////////////////////////////////////////////
// The bounds are rounded to integers from floating-point products, and an
// exact product can come out one ulp off. Each instance below has its optimum
// pruned when one bound is rounded without the nudge that keeps it valid:
// the first for a state under the budget (39 is answered), the second for a
// state over it (171).
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_dp, bounds_are_not_rounded_below_an_attainable_value) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u};
    {
        std::vector<int> values = {18, 21, 17, 2};
        std::vector<int> costs = {882, 1029, 833, 98};
        auto alg = knapsack_dp(items, values, costs, 1960);
        ASSERT_EQ(alg.run().solution_value(), 40);
    }
    {
        std::vector<int> values = {42, 108, 22, 21};
        std::vector<int> costs = {378, 738, 198, 153};
        auto alg = knapsack_dp(items, values, costs, 1314);
        ASSERT_EQ(alg.run().solution_value(), 172);
    }
}

////////////////////////////////////////////////////////////////////////////////
// The two tests below run searches that take milliseconds, under a timeout so
// that losing what keeps them short fails the test instead of hanging the
// suite. The timeout is a second, not more: a search gone wrong here gains a
// gigabyte of states every few seconds.
////////////////////////////////////////////////////////////////////////////////

// Three families of pack sizes, each with its own unit cost: the budget's
// remainder fits no pack, so no solution meets the bound and the states are
// only ever discarded by a cheaper state worth more. Without that rule every
// subset of chunks is its own state. knapsack_bnb enumerates the counts of a
// family here and does not finish.
GTEST_TEST(knapsack_dp, cheaper_states_worth_more_discard_the_others) {
    const std::vector<int> unit_costs = {30, 42, 55};
    const std::vector<int> unit_values = {57, 55, 39};
    const std::vector<int> pack_sizes = {1, 2, 3, 5, 8, 12};
    knapsack_instance instance;
    int total_cost = 0;
    for(std::size_t i = 0; i < 720; ++i) {
        const std::size_t family = i % 3;
        const int size = pack_sizes[(i / 3) % 6];
        instance.items.push_back(i);
        instance.costs.push_back(size * unit_costs[family]);
        instance.values.push_back(size * unit_values[family]);
        total_cost += instance.costs.back();
    }
    instance.budget = total_cost / 2 + 17;

    auto alg = knapsack_dp(instance.items, instance.values, instance.costs,
                           instance.budget);
    ASSERT_TRUE(alg.run_with_timeout(std::chrono::seconds(1)));
    ASSERT_EQ(alg.solution_value(), reference_optimum(instance));
    ASSERT_TRUE(is_consistent_solution(alg, instance));
}

// Unrelated values and costs up to 1e9, where a table over the budget is out
// of reach and no two item sets share a cost: the states are kept to the few
// item sets near the break solution by their bounds alone. The optimum is
// knapsack_bnb's, which this shape suits.
GTEST_TEST(knapsack_dp, agrees_with_knapsack_bnb_on_large_coefficients) {
    std::mt19937 engine(20261113u);
    std::vector<std::size_t> items;
    std::vector<long long> values, costs;
    long long total_cost = 0;
    for(std::size_t i = 0; i < 2000; ++i) {
        items.push_back(i);
        values.push_back(draw(engine, 1, 1'000'000'000));
        costs.push_back(draw(engine, 1, 1'000'000'000));
        total_cost += costs.back();
    }

    auto alg = knapsack_dp(items, values, costs, total_cost / 2);
    ASSERT_TRUE(alg.run_with_timeout(std::chrono::seconds(1)));
    auto reference = knapsack_bnb(items, values, costs, total_cost / 2);
    ASSERT_EQ(alg.solution_value(), reference.run().solution_value());
    ASSERT_LE(alg.solution_cost(), total_cost / 2);
}

////////////////////////////////////////////////////////////////////////////////
// widths: the states may cost up to twice the budget and no more
////////////////////////////////////////////////////////////////////////////////

// More copies of an item than the budget holds: the copies in excess are
// never taken together, and the chunk of eight they would make -- 3.2e9
// here -- overflows a cost type wide enough for twice the budget.
GTEST_TEST(knapsack_dp, repeated_items_beyond_the_budget_do_not_overflow) {
    std::vector<std::size_t> items(16);
    std::iota(items.begin(), items.end(), std::size_t{0});
    std::vector<int> values(16, 10);
    std::vector<int> costs(16, 400'000'000);

    auto alg = knapsack_dp(items, values, costs, 1'000'000'000);
    ASSERT_EQ(alg.run().solution_value(), 20);
    ASSERT_EQ(alg.solution_cost(), 800'000'000);
    ASSERT_TRUE(SIZE(alg.solution_items(), 2));
}

// A budget of 1.005e9 in an int, which holds 2.1e9. Six cheap items make the
// break solution, and four dear ones of barely lower ratio are offered on top
// of it. No bound discards the states that take them, as no item set comes
// within 4e6 of the budget, and together they cost 2.2e9 -- unless a state
// too far over the budget to ever come back under it is not created at all.
// Wrapped around, that cost passes for a feasible one and its value for the
// optimum.
GTEST_TEST(knapsack_dp, states_over_the_budget_stay_within_twice_the_budget) {
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u};
    std::vector<int> costs = {
        100'000'001, 100'000'001, 100'000'001, 100'000'003, 100'000'003,
        100'000'003, 420'000'003, 410'000'007, 400'000'009, 390'000'011};
    std::vector<long long> values(costs.begin(), costs.end());
    for(std::size_t dear = 6; dear < 10; ++dear) --values[dear];
    const int budget = 1'005'000'000;

    auto alg = knapsack_dp(items, values, costs, budget);
    auto reference = knapsack_bnb(items, values, costs, budget);
    ASSERT_EQ(alg.run().solution_value(), reference.run().solution_value());
    ASSERT_LE(alg.solution_cost(), budget);
}

// Seven copies whose cost, summed, is the budget: the budget over the cost
// rounds to 6.999999999999999, and capping the copies at that quotient
// leaves out one that fits.
GTEST_TEST(knapsack_dp, floating_point_quotient_does_not_drop_a_copy) {
    const double cost = 9.2463050097019366;
    std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u, 5u, 6u};
    std::vector<double> values(7, 1.0);
    std::vector<double> costs(7, cost);
    const double budget = cost + 2 * cost + 4 * cost;
    ASSERT_LT(budget / cost, 7.0);

    auto alg = knapsack_dp(items, values, costs, budget);
    ASSERT_EQ(alg.run().solution_value(), 7.0);
    ASSERT_LE(alg.solution_cost(), budget);
}

////////////////////////////////////////////////////////////////////////////////
// The record of the changes leading to each state is compacted as the search
// goes, and the incumbent and the live states are renumbered with it. These
// searches are long enough to compact several times; they answer with the
// wrong items if either renumbering is missed.
////////////////////////////////////////////////////////////////////////////////

namespace {

// Families of pack sizes with their own unit cost, values a little off the
// family's ratio: no bound is ever met, so the states live long.
knapsack_instance pack_size_instance(std::mt19937 & engine) {
    knapsack_instance instance;
    int total_cost = 0;
    for(std::size_t family = 0; family < 3; ++family) {
        const int unit_cost = static_cast<int>(draw(engine, 10, 25));
        const int ratio = static_cast<int>(draw(engine, 50, 200));
        for(const int size : {1, 2, 3, 5, 8, 12}) {
            const int cost = size * unit_cost;
            const int value =
                cost * ratio + static_cast<int>(draw(engine, -2, 2));
            for(std::size_t copy = 0; copy < 40; ++copy) {
                instance.items.push_back(instance.items.size());
                instance.costs.push_back(cost);
                instance.values.push_back(value);
                total_cost += cost;
            }
        }
    }
    instance.budget = total_cost / 2;
    return instance;
}

// Classes of nearly equal ratio, twenty copies of each of a hundred items:
// the state count follows the size of the costs.
struct tied_classes_instance {
    std::vector<std::size_t> items;
    std::vector<long long> values;
    std::vector<long long> costs;
    long long budget;
};
tied_classes_instance tied_classes(const long long max_cost) {
    std::mt19937 engine(20261113u);
    const std::vector<double> class_ratios = {1.9, 1.7, 1.3, 1.1, 0.7};
    tied_classes_instance instance;
    long long total_cost = 0;
    for(std::size_t distinct = 0; distinct < 100; ++distinct) {
        const long long cost = draw(engine, max_cost / 10, max_cost);
        const long long value = std::llround(
            100.0 * class_ratios[distinct % 5] * static_cast<double>(cost));
        for(std::size_t copy = 0; copy < 20; ++copy) {
            instance.items.push_back(instance.items.size());
            instance.costs.push_back(cost);
            instance.values.push_back(value);
            total_cost += cost;
        }
    }
    instance.budget = total_cost / 2;
    return instance;
}

}  // namespace

GTEST_TEST(knapsack_dp, solution_survives_the_compaction_of_its_history) {
    std::mt19937 engine(20261113u);
    for(std::size_t round = 0; round < 8; ++round) {
        const auto instance = pack_size_instance(engine);
        auto alg = knapsack_dp(instance.items, instance.values, instance.costs,
                               instance.budget);
        alg.run();
        ASSERT_EQ(alg.solution_value(), reference_optimum(instance));
        ASSERT_TRUE(is_consistent_solution(alg, instance));
    }

    const auto instance = tied_classes(10'000);
    auto alg = knapsack_dp(instance.items, instance.values, instance.costs,
                           instance.budget);
    auto reference = knapsack_bnb(instance.items, instance.values,
                                  instance.costs, instance.budget);
    ASSERT_EQ(alg.run().solution_value(), reference.run().solution_value());
    long long value = 0, cost = 0;
    for(auto && i : alg.solution_items()) {
        value += instance.values[i];
        cost += instance.costs[i];
    }
    ASSERT_EQ(value, alg.solution_value());
    ASSERT_LE(cost, instance.budget);
}

////////////////////////////////////////////////////////////////////////////////
// run_with_timeout stops the search and leaves a valid solution behind. With
// costs up to 4e6 the exact search below lasts seconds; stopped, it must
// return long before that, which an hour-long margin would not tell from a
// search that ignores the request and runs to its end.
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_dp, stopped_search_returns_promptly_with_its_incumbent) {
    const auto instance = tied_classes(4'000'000);
    auto alg = knapsack_dp(instance.items, instance.values, instance.costs,
                           instance.budget);

    const auto start = std::chrono::steady_clock::now();
    ASSERT_FALSE(alg.run_with_timeout(std::chrono::milliseconds(1)));
    ASSERT_LT(std::chrono::steady_clock::now() - start,
              std::chrono::seconds(1));

    long long value = 0, cost = 0;
    for(auto && i : alg.solution_items()) {
        value += instance.values[i];
        cost += instance.costs[i];
    }
    ASSERT_GT(value, 0);
    ASSERT_EQ(value, alg.solution_value());
    ASSERT_EQ(cost, alg.solution_cost());
    ASSERT_LE(cost, instance.budget);
}

////////////////////////////////////////////////////////////////////////////////
// The gap ends that same search in about a millisecond: the bound sits 1e-10
// above the optimum, and only an exact search has to tell the two apart. It
// is set before set_budget(), which re-derives everything else: a reset()
// dropping it would leave the search exact, and running out its second.
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_dp, relative_gap_ends_a_search_and_survives_set_budget) {
    const auto instance = tied_classes(4'000'000);
    auto alg =
        knapsack_dp(instance.items, instance.values, instance.costs, 0LL);
    alg.set_relative_gap(1e-6).set_budget(instance.budget);
    ASSERT_TRUE(alg.run_with_timeout(std::chrono::seconds(1)));
    ASSERT_GT(alg.solution_value(), 0);
    ASSERT_LE(alg.solution_cost(), instance.budget);
}

////////////////////////////////////////////////////////////////////////////////
// set_state_limit makes the search give up, incumbent kept, at the state
// that would exceed the limit, and both run() and run_with_timeout() say so.
// The limit is a setting, like the gap: it is set before set_budget(), and a
// reset() dropping it would let this search run to its end. Lifted, the
// search completes -- in milliseconds, on purpose: a limit that is ignored
// then fails the assertions instead of exhausting the memory.
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(knapsack_dp, state_limit_stops_the_search_with_its_incumbent) {
    const auto instance = tied_classes(20'000);
    auto alg =
        knapsack_dp(instance.items, instance.values, instance.costs, 0LL);
    alg.set_state_limit(1000).set_budget(instance.budget);

    alg.run();
    ASSERT_TRUE(alg.state_limit_reached());
    long long value = 0, cost = 0;
    for(auto && i : alg.solution_items()) {
        value += instance.values[i];
        cost += instance.costs[i];
    }
    ASSERT_GT(value, 0);
    ASSERT_EQ(value, alg.solution_value());
    ASSERT_LE(cost, instance.budget);

    // Stopped by the limit long before the timeout, and still not completed.
    ASSERT_FALSE(alg.run_with_timeout(std::chrono::seconds(30)));
    ASSERT_TRUE(alg.state_limit_reached());

    alg.set_state_limit(std::numeric_limits<std::size_t>::max());
    auto reference = knapsack_bnb(instance.items, instance.values,
                                  instance.costs, instance.budget);
    ASSERT_EQ(alg.run().solution_value(), reference.run().solution_value());
    ASSERT_FALSE(alg.state_limit_reached());
    ASSERT_TRUE(alg.run_with_timeout(std::chrono::seconds(30)));
}

// A limit the search stays under changes nothing.
GTEST_TEST(knapsack_dp, state_limit_out_of_reach_leaves_the_optimum) {
    for(std::size_t round = 0; round < NUM_INSTANCES; ++round) {
        const auto instance = random_instance(round);
        auto alg = knapsack_dp(instance.items, instance.values, instance.costs,
                               instance.budget);
        alg.set_state_limit(1'000'000).run();
        ASSERT_FALSE(alg.state_limit_reached());
        ASSERT_EQ(alg.solution_value(), reference_optimum(instance));
    }
}
