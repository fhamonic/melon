#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <future>
#include <memory>
#include <numeric>
#include <ranges>
#include <thread>
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
// instead of failing.
//
// random_access_range, not range: is_dominated() orders iterators with `<`,
// which only random-access iterators provide -- with a plain range the error
// surfaces inside the member instead of at the constraint.
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
    std::vector<std::ranges::iterator_t<const ItemRange>> _permuted_items;
    std::vector<std::pair<Value, Cost>> _value_cost_pairs;
    std::vector<std::pair<typename decltype(_value_cost_pairs)::const_iterator,
                          std::size_t>>
        _best_sol;

private:
    double value_cost_ratio(const std::pair<Value, Cost> & p) const {
        if constexpr(std::numeric_limits<float>::is_iec559) {
            return p.first / static_cast<double>(p.second);
        } else {
            return (p.second == 0) ? std::numeric_limits<double>::max()
                                   : (p.first / static_cast<double>(p.second));
        }
    }

    auto iterative_bnb() {
        _best_sol.resize(0);
        auto it = _value_cost_pairs.cbegin();
        const auto end = _value_cost_pairs.cend();
        // Guards the goto: with no feasible item, jumping into the loop body
        // dereferences end.
        if(it == end) return _best_sol;
        std::vector<std::pair<decltype(it), std::size_t>> current_sol;
        Value current_sol_value = 0;
        Value best_sol_value = 0;
        Cost budget_left = _budget;
        goto begin;
    backtrack:
        while(!current_sol.empty()) {
            it = current_sol.back().first;
            if(--current_sol.back().second == 0) current_sol.pop_back();
            current_sol_value -= it->first;
            budget_left += it->second;
            for(++it; it < end; ++it) {
                if(budget_left < it->second) continue;
                if(current_sol_value + budget_left * value_cost_ratio(*it) <=
                   best_sol_value)
                    goto backtrack;
            begin: {
                const std::size_t num_take =
                    static_cast<std::size_t>(budget_left / it->second);
                current_sol_value += static_cast<Value>(num_take) * it->first;
                budget_left -= static_cast<Cost>(num_take) * it->second;
                current_sol.emplace_back(it, num_take);
            }
            }
            if(current_sol_value <= best_sol_value) continue;
            best_sol_value = current_sol_value;
            _best_sol = current_sol;
        }
        return _best_sol;
    }

    auto iterative_bnb_timeout(std::stop_token stoken) {
        _best_sol.resize(0);
        auto it = _value_cost_pairs.cbegin();
        const auto end = _value_cost_pairs.cend();
        // Guards the goto: with no feasible item, jumping into the loop body
        // dereferences end.
        if(it == end) return _best_sol;
        std::vector<std::pair<decltype(it), std::size_t>> current_sol;
        Value current_sol_value = 0;
        Value best_sol_value = 0;
        Cost budget_left = _budget;
        goto begin;
    backtrack:
        while(!current_sol.empty() && !stoken.stop_requested()) {
            it = current_sol.back().first;
            if(--current_sol.back().second == 0) current_sol.pop_back();
            current_sol_value -= it->first;
            budget_left += it->second;
            for(++it; it < end; ++it) {
                if(budget_left < it->second) continue;
                if(current_sol_value + budget_left * value_cost_ratio(*it) <=
                   best_sol_value)
                    goto backtrack;
            begin: {
                const std::size_t num_take =
                    static_cast<std::size_t>(budget_left / it->second);
                current_sol_value += static_cast<Value>(num_take) * it->first;
                budget_left -= static_cast<Cost>(num_take) * it->second;
                current_sol.emplace_back(it, num_take);
            }
            }
            if(current_sol_value <= best_sol_value) continue;
            best_sol_value = current_sol_value;
            _best_sol = current_sol;
        }
        return _best_sol;
    }

public:
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

private:
    bool is_dominated(auto && item_it) const {
        const Value item_value = _value_map[*item_it];
        const Cost item_cost = _cost_map[*item_it];
        for(auto it = _items_range.begin(); it != _items_range.end(); ++it) {
            auto && i = *it;
            if(it == item_it) continue;
            const Value i_value = _value_map[i];
            const Cost i_cost = _cost_map[i];
            // Not covered by reset()'s identical assert: that one fires only
            // as its loop reaches each item, and this scan runs ahead of it
            // to divide by the cost of every item.
            assert(i_cost > static_cast<Cost>(0));
            if(i_cost == item_cost && i_value == item_value)
                return (it < item_it);
            if(i_cost > item_cost) continue;
            int num_times = static_cast<int>(item_cost / i_cost);
            if(num_times * i_value > item_value) return true;
        }
        return false;
    }

public:
    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    // Moves stay defaulted: _best_sol holds iterators into _value_cost_pairs,
    // whose buffer transfers with the move. A copy would have to rebase them,
    // and could not be declared honestly -- for a move-only ItemRange
    // std::copyable would answer true and the copy hard-error in the
    // mem-initializer.
    unbounded_knapsack_bnb(const unbounded_knapsack_bnb &) = delete;
    unbounded_knapsack_bnb(unbounded_knapsack_bnb &&) = default;

    unbounded_knapsack_bnb & operator=(const unbounded_knapsack_bnb &) = delete;
    unbounded_knapsack_bnb & operator=(unbounded_knapsack_bnb &&) = default;

    unbounded_knapsack_bnb & reset() {
        assert(_budget >= static_cast<Cost>(0));
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
            assert(value >= static_cast<Value>(0));
            if(value == static_cast<Value>(0)) continue;
            const Cost cost = _cost_map[i];
            assert(cost > static_cast<Cost>(0));
            if(cost > _budget) continue;
            if(is_dominated(it)) continue;
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
    unbounded_knapsack_bnb & set_budget(Cost b) {
        _budget = b;
        return reset();
    }

    unbounded_knapsack_bnb & run() {
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
            sum += it->first * static_cast<Value>(count);
        return sum;
    }

    [[nodiscard]] auto solution_cost() const {
        Cost sum = 0;
        for(auto && [it, count] : _best_sol)
            sum += it->second * static_cast<Cost>(count);
        return sum;
    }
};

template <typename ItemRange, typename ValueMap, typename CostMap>
unbounded_knapsack_bnb(ItemRange &&, ValueMap &&, CostMap &&, auto &&)
    -> unbounded_knapsack_bnb<std::views::all_t<ItemRange>,
                              maps::mapping_all_t<ValueMap>,
                              maps::mapping_all_t<CostMap>>;

}  // namespace melon
