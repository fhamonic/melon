#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <future>
#include <limits>
#include <memory>
#include <numeric>
#include <ranges>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/detail/stdlib_check.hpp"
#include "melon/mapping.hpp"

namespace melon {

// Preconditions on the mapped values, uncheckable by any concept: values and
// costs must be non-negative and the budget must be non-negative. The bound
// computeUpperBound() computes is the fractional relaxation, which only
// dominates the integral optimum under those signs; a negative one prunes the
// true optimum away, so run() answers with a suboptimal solution instead of
// failing.
//
// random_access_range, not range: the branch-and-bound loops order iterators
// with `<`, which only random-access iterators provide -- with a plain range
// the error surfaces inside the members instead of at the constraint.
template <std::ranges::random_access_range ItemRange,
          mapping_view<std::ranges::range_value_t<ItemRange>> ValueMap,
          mapping_view<std::ranges::range_value_t<ItemRange>> CostMap>
    requires std::is_arithmetic_v<mapped_value_t<
                 ValueMap, std::ranges::range_value_t<ItemRange>>> &&
             std::is_arithmetic_v<
                 mapped_value_t<CostMap, std::ranges::range_value_t<ItemRange>>>
class knapsack_bnb {
private:
    using Item = std::ranges::range_value_t<ItemRange>;
    using Value = mapped_value_t<ValueMap, Item>;
    using Cost = mapped_value_t<CostMap, Item>;

    ItemRange _items_range;
    ValueMap _value_map;
    CostMap _cost_map;

    Cost _budget;
    std::vector<std::ranges::iterator_t<const ItemRange>> _permuted_items;
    std::vector<std::pair<Value, Cost>> _value_cost_pairs;
    std::vector<typename decltype(_value_cost_pairs)::const_iterator> _best_sol;

private:
    double value_cost_ratio(const std::pair<Value, Cost> & p) const {
        if constexpr(std::numeric_limits<float>::is_iec559) {
            return p.first / static_cast<double>(p.second);
        } else {
            return (p.second == 0) ? std::numeric_limits<double>::max()
                                   : (p.first / static_cast<double>(p.second));
        }
    }

    Value computeUpperBound(auto it, const auto end, Value bound_value,
                            Cost bound_budget_left) const {
        for(; it < end; ++it) {
            if(bound_budget_left < it->second)
                // The ratio is taken in double *before* the multiplication:
                // budget * value first would multiply in the integer operand
                // types, whose product overflows on instances whose bound
                // itself fits comfortably.
                return static_cast<Value>(
                    bound_value +
                    bound_budget_left *
                        (it->first / static_cast<double>(it->second)));
            bound_budget_left -= it->second;
            bound_value += it->first;
        }

        return bound_value;
    }

    void iterative_bnb() {
        _best_sol.resize(0);
        auto it = _value_cost_pairs.cbegin();
        const auto end = _value_cost_pairs.cend();
        if(it == end) return;
        std::vector<decltype(it)> current_sol;
        Value current_sol_value = 0;
        Value best_sol_value = 0;
        Cost budget_left = _budget;
        goto begin;
    backtrack:
        while(!current_sol.empty()) {
            it = current_sol.back();
            current_sol_value -= it->first;
            budget_left += it->second;
            current_sol.pop_back();
            for(++it; it < end; ++it) {
                if(budget_left < it->second) continue;
                if(computeUpperBound(it, end, current_sol_value, budget_left) <=
                   best_sol_value)
                    goto backtrack;
            begin:
                current_sol_value += it->first;
                budget_left -= it->second;
                current_sol.push_back(it);
            }
            if(current_sol_value <= best_sol_value) continue;
            best_sol_value = current_sol_value;
            _best_sol = current_sol;
        }
    }
    bool iterative_bnb_timeout(std::stop_token stoken) {
        _best_sol.resize(0);
        auto it = _value_cost_pairs.cbegin();
        const auto end = _value_cost_pairs.cend();
        if(it == end) return true;
        std::vector<decltype(it)> current_sol;
        Value current_sol_value = 0;
        Value best_sol_value = 0;
        Cost budget_left = _budget;
        goto begin;
    backtrack:
        while(!current_sol.empty() && !stoken.stop_requested()) {
            it = current_sol.back();
            current_sol_value -= it->first;
            budget_left += it->second;
            current_sol.pop_back();
            for(++it; it < end; ++it) {
                if(budget_left < it->second) continue;
                if(computeUpperBound(it, end, current_sol_value, budget_left) <=
                   best_sol_value)
                    goto backtrack;
            begin:
                current_sol_value += it->first;
                budget_left -= it->second;
                current_sol.push_back(it);
            }
            if(current_sol_value <= best_sol_value) continue;
            best_sol_value = current_sol_value;
            _best_sol = current_sol;
        }
        return current_sol.empty();
    }

public:
    // ---- Construction -------------------------------------------------------

    // Constrained on what the mem-initializers actually do, so
    // std::is_constructible answers what construction actually does instead of
    // hard-erroring outside the immediate context.
    template <std::ranges::viewable_range IR, mapping_for<ValueMap> VM,
              mapping_for<CostMap> CM, std::convertible_to<Cost> B>
        requires std::constructible_from<ItemRange, std::views::all_t<IR>>
    knapsack_bnb(IR && items_range, VM && value_map, CM && cost_map,
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
    knapsack_bnb(const knapsack_bnb &) = delete;
    knapsack_bnb(knapsack_bnb &&) = default;

    knapsack_bnb & operator=(const knapsack_bnb &) = delete;
    knapsack_bnb & operator=(knapsack_bnb &&) = default;

    // ---- Setup --------------------------------------------------------------

    knapsack_bnb & reset() {
        _permuted_items.resize(0);
        _value_cost_pairs.resize(0);
        if constexpr(std::ranges::sized_range<ItemRange>) {
            auto num_items = std::ranges::size(_items_range);
            _permuted_items.reserve(num_items);
            _value_cost_pairs.reserve(num_items);
        }
        for(auto it = _items_range.begin(); it != _items_range.end(); ++it) {
            const auto & i = *it;
            const Value value = _value_map[i];
            if(value == static_cast<Value>(0)) continue;
            const Cost cost = _cost_map[i];
            if(cost > _budget) continue;
            _permuted_items.emplace_back(it);
            _value_cost_pairs.emplace_back(value, cost);
        }
        auto zip_view = std::views::zip(_permuted_items, _value_cost_pairs);
        std::ranges::sort(zip_view, [this](auto p1, auto p2) {
            return value_cost_ratio(std::get<1>(p1)) >
                   value_cost_ratio(std::get<1>(p2));
        });
        return *this;
    }

    // Re-derives through reset(): the item filter built there depends on the
    // budget, so assigning _budget alone would leave newly-affordable items
    // excluded and run() would answer a wrong optimum after a raise.
    knapsack_bnb & set_budget(Cost b) {
        _budget = b;
        return reset();
    }

    // ---- Execution ----------------------------------------------------------

    knapsack_bnb & run() {
        iterative_bnb();
        return *this;
    }

    template <typename Rep, typename Period>
    bool run_with_timeout(const std::chrono::duration<Rep, Period> & timeout) {
        std::jthread t([this](std::stop_token stoken) {
            return iterative_bnb_timeout(stoken);
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
        return std::views::transform(_best_sol, [this](auto && it) {
            return *_permuted_items[static_cast<std::size_t>(
                std::distance(_value_cost_pairs.cbegin(), it))];
        });
    }

    [[nodiscard]] auto solution_value() const {
        Value sum = 0;
        for(auto && it : _best_sol) sum += it->first;
        return sum;
    }

    [[nodiscard]] auto solution_cost() const {
        Cost sum = 0;
        for(auto && it : _best_sol) sum += it->second;
        return sum;
    }
};

template <typename ItemRange, typename ValueMap, typename CostMap>
knapsack_bnb(ItemRange &&, ValueMap &&, CostMap &&, auto &&)
    -> knapsack_bnb<std::views::all_t<ItemRange>, maps::mapping_all_t<ValueMap>,
                    maps::mapping_all_t<CostMap>>;

}  // namespace melon
