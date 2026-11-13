#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <future>
#include <limits>
#include <numeric>
#include <ranges>
#include <stop_token>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/detail/stdlib_check.hpp"
#include "melon/mapping.hpp"

namespace melon {

// Preconditions on the mapped values, uncheckable by any concept but asserted
// where reset() reads each item: values must be non-negative, costs strictly
// positive, and the budget non-negative. A zero cost divides by zero in the
// take-count below, and a negative value or cost makes the ratio bound
// unsound, so in release builds run() answers with a suboptimal solution
// instead of failing. Value must also be wide enough for the instance's
// optimum plus one item's value: the take-count and dominance products and
// the bounds reach that magnitude and overflow -- not saturate -- beyond it.
//
// random_access_range, not range: reset() orders iterators with `<` to keep
// the first of two identical items, which only random-access iterators
// provide -- with a plain range the error surfaces inside the member instead
// of at the constraint.
template <std::ranges::random_access_range ItemRange,
          mapping_view<std::ranges::range_value_t<ItemRange>> ValueMap,
          mapping_view<std::ranges::range_value_t<ItemRange>> CostMap>
    requires std::is_arithmetic_v<mapped_value_t<
                 ValueMap, std::ranges::range_value_t<ItemRange>>> &&
             std::is_arithmetic_v<
                 mapped_value_t<CostMap, std::ranges::range_value_t<ItemRange>>>
class unbounded_knapsack_bnb {
private:
    using Item = std::ranges::range_value_t<ItemRange>;
    using Value = mapped_value_t<ValueMap, Item>;
    using Cost = mapped_value_t<CostMap, Item>;

    ItemRange _items_range;
    ValueMap _value_map;
    CostMap _cost_map;

    Cost _budget;
    // _budget lowered to a multiple of the costs' GCD (integral Cost only).
    // Searched with _budget itself, a remainder no item can spend keeps
    // every bound above the optimum and nothing is pruned.
    Cost _search_budget;
    std::vector<std::ranges::iterator_t<const ItemRange>> _permuted_items;
    std::vector<std::pair<Value, Cost>> _value_cost_pairs;
    std::vector<double> _bound_ratios;
    std::vector<std::pair<typename decltype(_value_cost_pairs)::const_iterator,
                          std::size_t>>
        _best_sol;

private:
    // Rounded upward for an integral Value: the bound truncates
    // budget_left * ratio, and a product landing one ulp under an exact
    // integer prunes the branch that attains it.
    static double bound_ratio(const std::pair<Value, Cost> & p) {
        double ratio;
        if constexpr(std::numeric_limits<float>::is_iec559) {
            ratio =
                static_cast<double>(p.first) / static_cast<double>(p.second);
        } else {
            if(p.second == 0) return std::numeric_limits<double>::max();
            ratio =
                static_cast<double>(p.first) / static_cast<double>(p.second);
        }
        if constexpr(std::is_integral_v<Value>)
            ratio *= 1 + 4 * std::numeric_limits<double>::epsilon();
        return ratio;
    }

    // Truncated for an integral Value: compared as a real number, a bound a
    // fraction above `best` keeps its branch alive, and among items of nearly
    // equal ratio that is every branch.
    static bool cannot_beat(Value best, Value value, Cost budget_left,
                            double ratio) {
        const double fraction = static_cast<double>(budget_left) * ratio;
        if constexpr(std::is_integral_v<Value>) {
            return value + static_cast<Value>(fraction) <= best;
        } else {
            return static_cast<double>(value) + fraction <=
                   static_cast<double>(best);
        }
    }

    static std::size_t copies_that_fit(Cost budget_left, Cost cost) {
        if constexpr(std::is_integral_v<Cost>) {
            return static_cast<std::size_t>(budget_left / cost);
        } else {
            // The quotient alone can be one copy off either way: one short
            // leaves out a copy that fits, one over overdraws the budget.
            auto copies = static_cast<std::size_t>(budget_left / cost) + 1;
            while(static_cast<Cost>(copies) * cost > budget_left) --copies;
            return copies;
        }
    }

    // As many copies of `kept` as `cost` pays for. The product needs no
    // overflow guard -- the copies fit the budget, so it is the value of a
    // solution -- and comparing by division instead doubles reset().
    static bool copies_are_worth(const std::pair<Value, Cost> & kept,
                                 Value value, Cost cost) {
        return static_cast<Value>(copies_that_fit(cost, kept.second)) *
                   kept.first >=
               value;
    }

    template <typename StopRequested>
    void search(StopRequested && stop_requested) {
        _best_sol.resize(0);
        const auto first = _value_cost_pairs.cbegin();
        const auto end = _value_cost_pairs.cend();
        auto it = first;
        // Guards the goto: with no feasible item, jumping into the loop body
        // dereferences end.
        if(it == end) return;
        std::vector<std::pair<decltype(it), std::size_t>> current_sol;
        Value current_sol_value = 0;
        Value best_sol_value = 0;
        Cost budget_left = _search_budget;
        goto begin;
    backtrack:
        while(!current_sol.empty()) {
            if(stop_requested()) return;
            it = current_sol.back().first;
            // One copy fewer, the freed budget spent at the next ratio at
            // best: if that cannot beat the best, no lower count can, and the
            // whole entry goes at once. Copy by copy, a budget of 1e9 on an
            // item of cost 10 unwinds in 1e8 steps.
            // Braced: `goto begin` may not jump past an initialized variable.
            {
                const auto next = static_cast<std::size_t>(it - first) + 1;
                if(cannot_beat(
                       best_sol_value,
                       static_cast<Value>(current_sol_value - it->first),
                       static_cast<Cost>(budget_left + it->second),
                       next < _bound_ratios.size() ? _bound_ratios[next]
                                                   : 0.0)) {
                    const std::size_t copies = current_sol.back().second;
                    current_sol_value = static_cast<Value>(
                        current_sol_value -
                        static_cast<Value>(copies) * it->first);
                    budget_left = static_cast<Cost>(
                        budget_left + static_cast<Cost>(copies) * it->second);
                    current_sol.pop_back();
                    continue;
                }
            }
            if(--current_sol.back().second == 0) current_sol.pop_back();
            current_sol_value =
                static_cast<Value>(current_sol_value - it->first);
            budget_left = static_cast<Cost>(budget_left + it->second);
            for(++it; it < end; ++it) {
                if(budget_left < it->second) continue;
                if(cannot_beat(
                       best_sol_value, current_sol_value, budget_left,
                       _bound_ratios[static_cast<std::size_t>(it - first)]))
                    goto backtrack;
            begin: {
                const std::size_t num_take =
                    copies_that_fit(budget_left, it->second);
                current_sol_value = static_cast<Value>(
                    current_sol_value +
                    static_cast<Value>(num_take) * it->first);
                budget_left = static_cast<Cost>(
                    budget_left - static_cast<Cost>(num_take) * it->second);
                current_sol.emplace_back(it, num_take);
            }
            }
            if(current_sol_value <= best_sol_value) continue;
            best_sol_value = current_sol_value;
            _best_sol = current_sol;
        }
    }

public:
    // ---- Construction -------------------------------------------------------

    // Constrained on what the mem-initializers actually do, so
    // std::is_constructible answers what construction actually does instead of
    // hard-erroring outside the immediate context.
    template <std::ranges::viewable_range IR, mapping_for<ValueMap> VM,
              mapping_for<CostMap> CM, std::convertible_to<Cost> B>
        requires std::constructible_from<ItemRange, std::views::all_t<IR>>
    unbounded_knapsack_bnb(IR && items_range, VM && value_map, CM && cost_map,
                           B && budget)
        : _items_range(std::views::all(std::forward<IR>(items_range)))
        , _value_map(maps::mapping_all(std::forward<VM>(value_map)))
        , _cost_map(maps::mapping_all(std::forward<CM>(cost_map)))
        , _budget(std::forward<B>(budget)) {
        reset();
    }

public:
    // Move-only; see the melon::traversal_algorithm concept.
    // Moves stay defaulted: _best_sol holds iterators into _value_cost_pairs,
    // whose buffer transfers with the move. A copy would have to rebase them,
    // and could not be declared honestly -- for a move-only ItemRange
    // std::copyable would answer true and the copy hard-error in the
    // mem-initializer.
    unbounded_knapsack_bnb(const unbounded_knapsack_bnb &) = delete;
    unbounded_knapsack_bnb(unbounded_knapsack_bnb &&) = default;

    unbounded_knapsack_bnb & operator=(const unbounded_knapsack_bnb &) = delete;
    unbounded_knapsack_bnb & operator=(unbounded_knapsack_bnb &&) = default;

    // ---- Setup --------------------------------------------------------------

    unbounded_knapsack_bnb & reset() {
        assert(_budget >= static_cast<Cost>(0));
        // Cleared with the pairs it points into: a kept solution would read
        // the rebuilt, or reallocated-away, entries.
        _best_sol.resize(0);
        _permuted_items.resize(0);
        _value_cost_pairs.resize(0);
        _bound_ratios.resize(0);
        if constexpr(std::ranges::sized_range<ItemRange>) {
            auto num_items = std::ranges::size(_items_range);
            _permuted_items.reserve(num_items);
            _value_cost_pairs.reserve(num_items);
        }
        for(auto it = _items_range.begin(); it != _items_range.end(); ++it) {
            const auto & i = *it;
            const Value value = _value_map[i];
            assert(value >= static_cast<Value>(0));
            if(value == static_cast<Value>(0)) continue;
            const Cost cost = _cost_map[i];
            assert(cost > static_cast<Cost>(0));
            if(cost > _budget) continue;
            _permuted_items.emplace_back(it);
            _value_cost_pairs.emplace_back(value, cost);
        }

        // An item is dropped when copies of a cheaper one, as many as its
        // cost pays for, are worth as much. Kept items are enough to compare
        // with: what dropped an item outdoes what that item would drop.
        // Range order breaks the last tie, so that the first of identical
        // items is kept whatever the sort does with equal elements.
        std::ranges::sort(std::views::zip(_permuted_items, _value_cost_pairs),
                          [](const auto & p1, const auto & p2) {
                              const auto & [value1, cost1] = std::get<1>(p1);
                              const auto & [value2, cost2] = std::get<1>(p2);
                              if(cost1 != cost2) return cost1 < cost2;
                              if(value1 != value2) return value1 > value2;
                              return std::get<0>(p1) < std::get<0>(p2);
                          });
        std::size_t num_kept = 0;
        Value highest_value = 0;
        for(std::size_t i = 0; i < _value_cost_pairs.size(); ++i) {
            const auto [value, cost] = _value_cost_pairs[i];
            if(value <= highest_value) continue;
            bool dominated = false;
            for(std::size_t kept = 0; kept < num_kept && !dominated; ++kept)
                dominated =
                    copies_are_worth(_value_cost_pairs[kept], value, cost);
            if(dominated) continue;
            highest_value = value;
            _permuted_items[num_kept] = _permuted_items[i];
            _value_cost_pairs[num_kept] = _value_cost_pairs[i];
            ++num_kept;
        }
        _permuted_items.resize(num_kept);
        _value_cost_pairs.resize(num_kept);

        for(const auto & p : _value_cost_pairs)
            _bound_ratios.push_back(bound_ratio(p));
        // Costlier first among equal ratios: cheaper first, the search backs
        // out of the small items of a tie before a large one gets its turn --
        // three times slower on 10 000 items of one ratio.
        std::ranges::sort(
            std::views::zip(_permuted_items, _value_cost_pairs, _bound_ratios),
            [](const auto & p1, const auto & p2) {
                if(std::get<2>(p1) != std::get<2>(p2))
                    return std::get<2>(p1) > std::get<2>(p2);
                return std::get<1>(p1).second > std::get<1>(p2).second;
            });

        _search_budget = _budget;
        if constexpr(std::is_integral_v<Cost> && !std::is_same_v<Cost, bool>) {
            Cost divisor = 0;
            for(const auto & p : _value_cost_pairs) {
                divisor = std::gcd(divisor, p.second);
                if(divisor == 1) break;
            }
            // > 1, not != 1: with no item kept the divisor is 0, and % 0 traps.
            if(divisor > 1)
                _search_budget = static_cast<Cost>(_budget - _budget % divisor);
        }
        return *this;
    }

    // Re-derives through reset(): the item filter built there depends on the
    // budget, so assigning _budget alone would leave newly-affordable items
    // excluded and run() would answer a wrong optimum after a raise.
    unbounded_knapsack_bnb & set_budget(Cost b) {
        _budget = b;
        return reset();
    }

    // ---- Execution ----------------------------------------------------------

    unbounded_knapsack_bnb & run() {
        search([] { return false; });
        return *this;
    }

    template <typename Rep, typename Period>
    bool run_with_timeout(const std::chrono::duration<Rep, Period> & timeout) {
        std::jthread t([this](std::stop_token stoken) {
            search([&stoken] { return stoken.stop_requested(); });
        });
        // The extra thread exists only to make the join waitable: jthread has
        // no timed join, so the wait_for below needs a future to wait on.
        auto future = std::async(std::launch::async, &std::jthread::join, &t);
        if(future.wait_for(timeout) == std::future_status::timeout) {
            t.request_stop();
            return false;
        }
        return true;
    }

    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] auto solution_items() const {
        return std::views::transform(_best_sol, [this](auto && p) {
            return std::make_pair(
                *_permuted_items[static_cast<std::size_t>(
                    std::distance(_value_cost_pairs.cbegin(), p.first))],
                p.second);
        });
    }

    [[nodiscard]] auto solution_value() const {
        Value sum = 0;
        for(auto && [it, count] : _best_sol)
            sum =
                static_cast<Value>(sum + it->first * static_cast<Value>(count));
        return sum;
    }

    [[nodiscard]] auto solution_cost() const {
        Cost sum = 0;
        for(auto && [it, count] : _best_sol)
            sum =
                static_cast<Cost>(sum + it->second * static_cast<Cost>(count));
        return sum;
    }
};

template <typename ItemRange, typename ValueMap, typename CostMap>
unbounded_knapsack_bnb(ItemRange &&, ValueMap &&, CostMap &&, auto &&)
    -> unbounded_knapsack_bnb<std::views::all_t<ItemRange>,
                              maps::mapping_all_t<ValueMap>,
                              maps::mapping_all_t<CostMap>>;

}  // namespace melon
