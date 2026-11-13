#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <future>
#include <limits>
#include <memory>
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

// Preconditions on the mapped values, uncheckable by any concept: values and
// costs must be non-negative and the budget must be non-negative. The pruning
// bound is the fractional relaxation, which only dominates the integral
// optimum under those signs; a negative one prunes the true optimum away, so
// run() answers with a suboptimal solution instead of failing.
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

    using item_iterator =
        typename std::vector<std::pair<Value, Cost>>::const_iterator;

    // Identical items, searched as one entry with a count: taken one by one,
    // m copies cost the search 2^m subsets where only m + 1 counts differ.
    struct item_run {
        Value value;
        Cost cost;
        Value whole_value;
        Cost whole_cost;
        std::size_t count;
    };
    using run_iterator = typename std::vector<item_run>::const_iterator;

    ItemRange _items_range;
    ValueMap _value_map;
    CostMap _cost_map;

    Cost _budget;
    // _budget lowered to a multiple of the costs' GCD (integral Cost only).
    // Searched with _budget itself, a remainder no item set can spend keeps
    // every bound above the optimum and nothing is pruned.
    Cost _search_budget;
    double _relative_gap = 0;
    std::vector<std::ranges::iterator_t<const ItemRange>> _permuted_items;
    std::vector<std::pair<Value, Cost>> _value_cost_pairs;
    std::vector<double> _bound_ratios;
    // Empty when no two items are identical: the search then runs on the
    // items themselves, which costs fewer instructions per node.
    std::vector<item_run> _runs;
    std::vector<std::size_t> _run_first;
    std::vector<item_iterator> _best_sol;

private:
    // Rounded upward for an integral Value: the bound truncates
    // budget_left * ratio, and a product landing one ulp under an exact
    // integer prunes the branch that attains it.
    double bound_ratio(const std::pair<Value, Cost> & p) const {
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

    // The fractional bound once the first item that does not fit is reached.
    // The fraction joins an integral Value after truncation: summed in double,
    // a value above 2^53 comes back rounded and may fall under the optimum.
    static Value fractional_bound(Value value, Cost budget_left, double ratio) {
        const double fraction = static_cast<double>(budget_left) * ratio;
        if constexpr(std::is_integral_v<Value>)
            return static_cast<Value>(value + static_cast<Value>(fraction));
        else
            return static_cast<Value>(static_cast<double>(value) + fraction);
    }

    // What a bound must exceed for its branch to be searched. The gap-free
    // case skips the multiplication: through double, an integral value above
    // 2^53 can round upward and prune the optimum.
    Value prune_threshold(Value incumbent_value) const {
        if(_relative_gap == 0) return incumbent_value;
        return static_cast<Value>(static_cast<double>(incumbent_value) *
                                  (1 + _relative_gap));
    }

    static std::size_t copies_that_fit(Cost budget_left, Cost cost,
                                       std::size_t count) {
        if constexpr(std::is_integral_v<Cost>) {
            return static_cast<std::size_t>(budget_left / cost);
        } else {
            // The quotient alone can be one copy off either way: one short
            // leaves out a copy that fits, one over overdraws the budget.
            auto copies = std::min(
                count, static_cast<std::size_t>(budget_left / cost) + 1);
            while(static_cast<Cost>(copies) * cost > budget_left) --copies;
            return copies;
        }
    }

    // Both searches share their shape. Items that fit one after the other
    // have the same bound, so it is evaluated once for the whole stretch,
    // which is then taken at once: evaluated at each item, a dive costs the
    // square of its length. They return as soon as the incumbent meets the
    // root bound, and false only when stopped.

    template <typename StopRequested>
    bool search_items(StopRequested && stop_requested) {
        const auto begin = _value_cost_pairs.cbegin();
        const auto end = _value_cost_pairs.cend();
        std::vector<item_iterator> current_sol;
        Value current_sol_value = 0;
        Cost budget_left = _search_budget;
        Value threshold = 0;
        Value root_bound = std::numeric_limits<Value>::max();
        auto it = begin;
        for(;;) {
            for(; it < end; ++it) {
                if(budget_left < it->second) continue;
                auto stretch_end = it;
                Value stretch_value = current_sol_value;
                Cost stretch_budget = budget_left;
                do {
                    stretch_value += stretch_end->first;
                    stretch_budget -= stretch_end->second;
                    ++stretch_end;
                } while(stretch_end < end &&
                        stretch_end->second <= stretch_budget);
                const Value bound =
                    stretch_end < end
                        ? fractional_bound(
                              stretch_value, stretch_budget,
                              _bound_ratios[static_cast<std::size_t>(
                                  stretch_end - begin)])
                        : stretch_value;
                if(it == begin) root_bound = bound;
                if(bound <= threshold) goto backtrack;
                do current_sol.push_back(it);
                while(++it < stretch_end);
                current_sol_value = stretch_value;
                budget_left = stretch_budget;
                if(it == end) break;
            }
            if(current_sol_value > threshold) {
                _best_sol = current_sol;
                threshold = prune_threshold(current_sol_value);
                if(threshold >= root_bound) return true;
            }
        backtrack:
            if(current_sol.empty()) return true;
            if(stop_requested()) return false;
            it = current_sol.back();
            current_sol.pop_back();
            current_sol_value -= it->first;
            budget_left += it->second;
            ++it;
        }
    }

    template <typename StopRequested>
    bool search_runs(StopRequested && stop_requested) {
        struct taken_run {
            run_iterator run;
            std::size_t count;
        };
        const auto begin = _runs.cbegin();
        const auto end = _runs.cend();
        std::vector<taken_run> current_sol;
        std::vector<taken_run> best_sol;
        Value current_sol_value = 0;
        Cost budget_left = _search_budget;
        Value threshold = 0;
        Value root_bound = std::numeric_limits<Value>::max();
        bool finished = true;
        auto it = begin;
        for(;;) {
            for(; it < end; ++it) {
                if(budget_left < it->cost) continue;
                auto stretch_end = it;
                Value stretch_value = current_sol_value;
                Cost stretch_budget = budget_left;
                while(stretch_end->whole_cost <= stretch_budget) {
                    stretch_value += stretch_end->whole_value;
                    stretch_budget -= stretch_end->whole_cost;
                    if(++stretch_end == end) break;
                }
                const Value bound =
                    stretch_end < end
                        ? fractional_bound(
                              stretch_value, stretch_budget,
                              _bound_ratios[_run_first[static_cast<std::size_t>(
                                  stretch_end - begin)]])
                        : stretch_value;
                if(it == begin) root_bound = bound;
                if(bound <= threshold) goto backtrack;
                for(; it < stretch_end; ++it)
                    current_sol.push_back(taken_run{it, it->count});
                current_sol_value = stretch_value;
                budget_left = stretch_budget;
                if(it == end) break;
                if(it->cost <= budget_left) {
                    const std::size_t copies =
                        copies_that_fit(budget_left, it->cost, it->count);
                    current_sol.push_back(taken_run{it, copies});
                    current_sol_value += static_cast<Value>(
                        static_cast<Value>(copies) * it->value);
                    budget_left -=
                        static_cast<Cost>(static_cast<Cost>(copies) * it->cost);
                }
            }
            if(current_sol_value > threshold) {
                best_sol = current_sol;
                threshold = prune_threshold(current_sol_value);
                if(threshold >= root_bound) break;
            }
        backtrack:
            if(current_sol.empty()) break;
            if(stop_requested()) {
                finished = false;
                break;
            }
            taken_run & last = current_sol.back();
            it = last.run;
            current_sol_value -= it->value;
            budget_left += it->cost;
            if(--last.count == 0) current_sol.pop_back();
            ++it;
        }
        for(const taken_run & taken : best_sol) {
            auto item =
                _value_cost_pairs.cbegin() +
                static_cast<std::ptrdiff_t>(
                    _run_first[static_cast<std::size_t>(taken.run - begin)]);
            for(std::size_t copy = 0; copy < taken.count; ++copy, ++item)
                _best_sol.push_back(item);
        }
        return finished;
    }

    template <typename StopRequested>
    bool search(StopRequested && stop_requested) {
        _best_sol.resize(0);
        return _runs.empty() ? search_items(stop_requested)
                             : search_runs(stop_requested);
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
        // Cleared with the pairs it points into: a kept solution would read
        // the rebuilt, or reallocated-away, entries.
        _best_sol.resize(0);
        _permuted_items.resize(0);
        _value_cost_pairs.resize(0);
        _bound_ratios.resize(0);
        _runs.resize(0);
        _run_first.resize(0);
        if constexpr(std::ranges::sized_range<ItemRange>) {
            auto num_items = std::ranges::size(_items_range);
            _permuted_items.reserve(num_items);
            _value_cost_pairs.reserve(num_items);
            _bound_ratios.reserve(num_items);
        }
        for(auto it = _items_range.begin(); it != _items_range.end(); ++it) {
            const auto & i = *it;
            const Value value = _value_map[i];
            if(value == static_cast<Value>(0)) continue;
            const Cost cost = _cost_map[i];
            if(cost > _budget) continue;
            _permuted_items.emplace_back(it);
            _value_cost_pairs.emplace_back(value, cost);
            _bound_ratios.push_back(bound_ratio(_value_cost_pairs.back()));
        }
        // Not the ratio alone: the runs below are built from adjacent
        // identical items, which an arbitrary order among equal ratios
        // interleaves with their neighbours. Costlier first, because filling
        // the budget with the small items of a tie leaves the search to
        // backtrack through each of them.
        std::ranges::sort(
            std::views::zip(_permuted_items, _value_cost_pairs, _bound_ratios),
            [](const auto & p1, const auto & p2) {
                if(std::get<2>(p1) != std::get<2>(p2))
                    return std::get<2>(p1) > std::get<2>(p2);
                const auto & [value1, cost1] = std::get<1>(p1);
                const auto & [value2, cost2] = std::get<1>(p2);
                if(cost1 != cost2) return cost1 > cost2;
                return value1 > value2;
            });

        _search_budget = _budget;
        if constexpr(std::is_integral_v<Cost> && !std::is_same_v<Cost, bool>) {
            Cost divisor = 0;
            for(const auto & p : _value_cost_pairs) {
                divisor = std::gcd(divisor, p.second);
                if(divisor == 1) break;
            }
            // 0 when every kept cost is 0: nothing to round to.
            if(divisor > 1)
                _search_budget = static_cast<Cost>(_budget - _budget % divisor);
        }

        if(std::ranges::adjacent_find(_value_cost_pairs) !=
           _value_cost_pairs.end()) {
            for(std::size_t i = 0; i < _value_cost_pairs.size(); ++i) {
                const auto & [value, cost] = _value_cost_pairs[i];
                if(i == 0 || _value_cost_pairs[i - 1] != _value_cost_pairs[i]) {
                    _runs.push_back(item_run{value, cost, value, cost, 1});
                    _run_first.push_back(i);
                    continue;
                }
                // A run stops at the copies the budget holds together. The
                // others can never join them, and summing them overflows a
                // Cost only wide enough for the budget.
                item_run & run = _runs.back();
                if(cost > _search_budget - run.whole_cost) continue;
                ++run.count;
                run.whole_value = static_cast<Value>(run.whole_value + value);
                run.whole_cost = static_cast<Cost>(run.whole_cost + cost);
            }
        }
        return *this;
    }

    // Re-derives through reset(): the item filter built there depends on the
    // budget, so assigning _budget alone would leave newly-affordable items
    // excluded and run() would answer a wrong optimum after a raise.
    knapsack_bnb & set_budget(Cost b) {
        _budget = b;
        return reset();
    }

    // run() settles for a solution within this relative gap of the optimum:
    // solution_value() >= optimum / (1 + gap). 0, the default, is exact.
    // Floating-point items of equal ratio need a positive gap: at 0 the
    // search enumerates the subsets that fill the budget to the last digits,
    // each a rounding error better than the one before.
    knapsack_bnb & set_relative_gap(double gap) {
        assert(gap >= 0);
        _relative_gap = gap;
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    knapsack_bnb & run() {
        search([] { return false; });
        return *this;
    }

    template <typename Rep, typename Period>
    bool run_with_timeout(const std::chrono::duration<Rep, Period> & timeout) {
        std::jthread t([this](std::stop_token stoken) {
            return search([&stoken] { return stoken.stop_requested(); });
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
