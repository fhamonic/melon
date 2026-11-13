#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
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

// knapsack_bnb's interface and preconditions -- values, costs and the budget
// non-negative -- with one more, on the types: Cost must represent twice the
// budget and Value twice the optimum. States overshoot the budget before
// they come back under it; in a narrower type their cost wraps around and
// passes for a feasible one.
//
// Time and memory follow the number of states, at most one per cost an item
// set can reach: twice the budget for an integral Cost, unbounded for a
// floating-point one. set_state_limit() caps them.
//
// random_access_range where a forward range would do: relaxed, a call site
// written for this solver no longer compiles when knapsack_bnb is swapped in.
template <std::ranges::random_access_range ItemRange,
          mapping_view<std::ranges::range_value_t<ItemRange>> ValueMap,
          mapping_view<std::ranges::range_value_t<ItemRange>> CostMap>
    requires std::is_arithmetic_v<mapped_value_t<
                 ValueMap, std::ranges::range_value_t<ItemRange>>> &&
             std::is_arithmetic_v<
                 mapped_value_t<CostMap, std::ranges::range_value_t<ItemRange>>>
class knapsack_dp {
private:
    using Item = std::ranges::range_value_t<ItemRange>;
    using Value = mapped_value_t<ValueMap, Item>;
    using Cost = mapped_value_t<CostMap, Item>;

    using item_iterator =
        typename std::vector<std::pair<Value, Cost>>::const_iterator;

    // Copies of one item decided together. 1, 2, 4... copies, then the rest,
    // reach every count of the item: decided one by one, m copies are m
    // passes over the states where these are log m.
    struct item_chunk {
        Value value;
        Cost cost;
        double ratio;
        std::size_t first;
        std::size_t count;
    };

    struct state {
        Cost cost;
        Value value;
        std::size_t last_change;
    };

    // The solution a state stands for is the break solution -- the chunks
    // that fit one after the other in ratio order -- with some chunks
    // flipped, in or out. A change flips one, on top of those of `previous`.
    struct change {
        std::size_t previous;
        std::size_t chunk;
    };
    static constexpr std::size_t no_change = static_cast<std::size_t>(-1);

    ItemRange _items_range;
    ValueMap _value_map;
    CostMap _cost_map;

    Cost _budget;
    // _budget lowered to a multiple of the costs' GCD (integral Cost only).
    // Searched with _budget itself, the remainder no item set can spend
    // keeps every bound above the optimum: ten to a hundred times slower.
    Cost _search_budget;
    double _relative_gap = 0;
    std::size_t _state_limit = std::numeric_limits<std::size_t>::max();
    bool _state_limit_reached = false;
    std::vector<std::ranges::iterator_t<const ItemRange>> _permuted_items;
    std::vector<std::pair<Value, Cost>> _value_cost_pairs;
    std::vector<item_chunk> _chunks;
    std::vector<item_iterator> _best_sol;

private:
    static double value_cost_ratio(const std::pair<Value, Cost> & p) {
        if constexpr(!std::numeric_limits<double>::is_iec559) {
            if(p.second == 0) return std::numeric_limits<double>::max();
        }
        return static_cast<double>(p.first) / static_cast<double>(p.second);
    }

    // For an integral Value both bounds are rounded to integers, from a
    // product first nudged toward the looser bound: the plain one can land
    // an ulp on the wrong side of an exact integer and prune the state that
    // attains the optimum. Summed as integers: in double, a value above 2^53
    // comes back rounded.

    static bool cannot_beat(Value threshold, Value value, Cost room,
                            double ratio) {
        const double gain = static_cast<double>(room) * ratio;
        if constexpr(std::is_integral_v<Value>) {
            return value +
                       static_cast<Value>(
                           gain *
                           (1 + 4 * std::numeric_limits<double>::epsilon())) <=
                   threshold;
        } else {
            return static_cast<double>(value) + gain <=
                   static_cast<double>(threshold);
        }
    }

    // ceil, not floor: on an exact product -- items of equal ratio -- the
    // floor of the nudged loss is one short, and no state over the budget is
    // ever pruned (118 passes for 29 on 200 items of one ratio).
    static bool cannot_beat_once_feasible(Value threshold, Value value,
                                          Cost excess, double ratio) {
        const double loss = static_cast<double>(excess) * ratio;
        if constexpr(std::is_integral_v<Value>) {
            return value <=
                   threshold +
                       static_cast<Value>(std::ceil(
                           loss *
                           (1 - 4 * std::numeric_limits<double>::epsilon())));
        } else {
            return static_cast<double>(value) - loss <=
                   static_cast<double>(threshold);
        }
    }

    // What a bound must exceed for its state to be kept. The gap-free case
    // skips the multiplication: through double, an integral value above 2^53
    // can round upward and prune the optimum.
    Value prune_threshold(Value incumbent_value) const {
        if(_relative_gap == 0) return incumbent_value;
        return static_cast<Value>(static_cast<double>(incumbent_value) *
                                  (1 + _relative_gap));
    }

    // Decides the chunks outward from the break solution, alternately the
    // next one it lacks and the last one it holds: an optimal solution rarely
    // flips the far ones, and the states are usually all pruned before they
    // are reached. Returns false when stopped or at the state limit.
    template <typename StopRequested>
    bool search(StopRequested && stop_requested) {
        _best_sol.resize(0);
        _state_limit_reached = false;
        const std::size_t num_chunks = _chunks.size();

        // Costs are summed up from zero, here and in every state, never
        // subtracted from the budget: in floating point, a budget that is
        // the sum of some costs holds them exactly one way, and can come out
        // one ulp short of the last of them the other.
        std::size_t break_chunk = 0;
        Value best_value = 0;
        Cost break_cost = 0;
        while(break_chunk < num_chunks &&
              break_cost + _chunks[break_chunk].cost <= _search_budget) {
            best_value =
                static_cast<Value>(best_value + _chunks[break_chunk].value);
            break_cost =
                static_cast<Cost>(break_cost + _chunks[break_chunk].cost);
            ++break_chunk;
        }
        std::size_t best_change = no_change;
        std::vector<change> changes;
        bool finished = true;

        if(break_chunk < num_chunks) {
            Value threshold = prune_threshold(best_value);
            std::vector<state> states{state{break_cost, best_value, no_change}};
            std::vector<state> next_states;

            // Chunks of no cost sort first and are never dropped: it frees
            // no budget, and their infinite ratio, cast to an integral Value
            // in the bound of a state over the budget, is undefined.
            std::size_t num_kept = 0;
            while(num_kept < break_chunk &&
                  _chunks[num_kept].cost <= static_cast<Cost>(0))
                ++num_kept;

            // Chunks [num_kept, num_droppable) are held by every state and
            // may still be dropped; chunks [next_offered, num_chunks) are
            // held by none and may still be taken.
            std::size_t num_droppable = break_chunk;
            Cost droppable_cost = break_cost;
            std::size_t next_offered = break_chunk;

            // Asked once per so many states, not once per pass: a pass over
            // a hundred million states lasts seconds, and a timeout would
            // be honoured that late.
            constexpr std::size_t stop_check_period = std::size_t{1} << 16;
            std::size_t until_stop_check = stop_check_period;

            // Merges the states with their copies flipped on `flipped`, in
            // cost order, keeping those worth more than every cheaper one
            // whose bound beats the threshold. Returns false when stopped or
            // at the state limit, leaving the states half merged.
            auto decide = [&](const std::size_t flipped, const bool dropping) {
                const item_chunk & chunk = _chunks[flipped];
                const double fill_ratio = next_offered < num_chunks
                                              ? _chunks[next_offered].ratio
                                              : 0.0;
                const bool can_drop = num_droppable > num_kept;
                const double shed_ratio =
                    can_drop ? _chunks[num_droppable - 1].ratio : 0.0;
                // A state over the budget by more than the droppable chunks
                // cost never comes back under it. Not creating one is also
                // what keeps every cost within twice the budget.
                const Cost heaviest_taker = static_cast<Cost>(
                    static_cast<Cost>(_search_budget + droppable_cost) -
                    chunk.cost);

                bool any_emitted = false;
                Value highest_value{};
                auto emit = [&](const Cost cost, const Value value,
                                std::size_t last_change, bool flips) {
                    if(any_emitted && value <= highest_value) return true;
                    any_emitted = true;
                    highest_value = value;
                    // Recorded as soon as the state becomes the incumbent:
                    // most incumbents are pruned by their own value right
                    // after, and are then the only trace of their solution.
                    auto record = [&] {
                        if(!flips) return;
                        changes.push_back(change{last_change, flipped});
                        last_change = changes.size() - 1;
                        flips = false;
                    };
                    if(cost <= _search_budget) {
                        if(value > best_value) {
                            record();
                            best_value = value;
                            best_change = last_change;
                            threshold = prune_threshold(value);
                        }
                        if(cannot_beat(threshold, value,
                                       static_cast<Cost>(_search_budget - cost),
                                       fill_ratio))
                            return true;
                    } else {
                        if(!can_drop ||
                           cannot_beat_once_feasible(
                               threshold, value,
                               static_cast<Cost>(cost - _search_budget),
                               shed_ratio))
                            return true;
                    }
                    // Checked here, at the state that would exceed the
                    // limit, not with the stop request: a small limit would
                    // be overshot by a whole period of states.
                    if(next_states.size() >= _state_limit) {
                        _state_limit_reached = true;
                        return false;
                    }
                    record();
                    next_states.push_back(state{cost, value, last_change});
                    return true;
                };

                const std::size_t num_states = states.size();
                std::size_t as_is = 0, to_flip = 0;
                for(;;) {
                    if(--until_stop_check == 0) {
                        if(stop_requested()) return false;
                        until_stop_check = stop_check_period;
                    }
                    if(to_flip < num_states &&
                       (dropping || states[to_flip].cost <= heaviest_taker)) {
                        const state & from = states[to_flip];
                        const Cost cost = static_cast<Cost>(
                            dropping ? from.cost - chunk.cost
                                     : from.cost + chunk.cost);
                        const Value value = static_cast<Value>(
                            dropping ? from.value - chunk.value
                                     : from.value + chunk.value);
                        if(as_is == num_states || cost < states[as_is].cost ||
                           (cost == states[as_is].cost &&
                            value > states[as_is].value)) {
                            if(!emit(cost, value, from.last_change, true))
                                return false;
                            ++to_flip;
                            continue;
                        }
                    } else if(as_is == num_states) {
                        break;
                    }
                    if(!emit(states[as_is].cost, states[as_is].value,
                             states[as_is].last_change, false))
                        return false;
                    ++as_is;
                }
                states.swap(next_states);
                next_states.resize(0);
                return true;
            };

            // Drops the changes no state leads to any more. Left alone they
            // grow with the running time, not with the states: a long search
            // exhausts the memory on changes of states pruned long ago.
            std::size_t max_changes = 1024;
            std::vector<bool> reachable;
            std::vector<std::size_t> moved_to;
            auto forget_dead_changes = [&] {
                reachable.assign(changes.size(), false);
                auto mark = [&](std::size_t c) {
                    for(; c != no_change && !reachable[c];
                        c = changes[c].previous)
                        reachable[c] = true;
                };
                mark(best_change);
                for(const state & s : states) mark(s.last_change);
                moved_to.resize(changes.size());
                std::size_t num_reachable = 0;
                // A change is recorded after the one it builds on, so its
                // predecessor has already moved when its turn comes.
                for(std::size_t c = 0; c < changes.size(); ++c) {
                    if(!reachable[c]) continue;
                    const std::size_t previous = changes[c].previous;
                    changes[num_reachable] = change{
                        previous == no_change ? no_change : moved_to[previous],
                        changes[c].chunk};
                    moved_to[c] = num_reachable++;
                }
                changes.resize(num_reachable);
                if(best_change != no_change)
                    best_change = moved_to[best_change];
                for(state & s : states)
                    if(s.last_change != no_change)
                        s.last_change = moved_to[s.last_change];
            };

            while(finished && !states.empty() &&
                  (next_offered < num_chunks || num_droppable > num_kept)) {
                if(next_offered < num_chunks)
                    finished = decide(next_offered++, false);
                if(finished && num_droppable > num_kept && !states.empty()) {
                    --num_droppable;
                    droppable_cost = static_cast<Cost>(
                        droppable_cost - _chunks[num_droppable].cost);
                    finished = decide(num_droppable, true);
                }
                if(!finished) break;
                if(changes.size() > max_changes) {
                    forget_dead_changes();
                    max_changes = 2 * changes.size() + 1024;
                }
            }
        }

        std::vector<bool> taken(num_chunks, false);
        for(std::size_t c = 0; c < break_chunk; ++c) taken[c] = true;
        for(std::size_t c = best_change; c != no_change;
            c = changes[c].previous)
            taken[changes[c].chunk] = !taken[changes[c].chunk];
        for(std::size_t c = 0; c < num_chunks; ++c) {
            if(!taken[c]) continue;
            auto item = _value_cost_pairs.cbegin() +
                        static_cast<std::ptrdiff_t>(_chunks[c].first);
            for(std::size_t copy = 0; copy < _chunks[c].count; ++copy, ++item)
                _best_sol.push_back(item);
        }
        return finished;
    }

public:
    // ---- Construction -------------------------------------------------------

    // Constrained on what the mem-initializers actually do, so
    // std::is_constructible answers what construction actually does instead of
    // hard-erroring outside the immediate context.
    template <std::ranges::viewable_range IR, mapping_for<ValueMap> VM,
              mapping_for<CostMap> CM, std::convertible_to<Cost> B>
        requires std::constructible_from<ItemRange, std::views::all_t<IR>>
    knapsack_dp(IR && items_range, VM && value_map, CM && cost_map, B && budget)
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
    knapsack_dp(const knapsack_dp &) = delete;
    knapsack_dp(knapsack_dp &&) = default;

    knapsack_dp & operator=(const knapsack_dp &) = delete;
    knapsack_dp & operator=(knapsack_dp &&) = default;

    // ---- Setup --------------------------------------------------------------

    knapsack_dp & reset() {
        if constexpr(std::is_integral_v<Cost>)
            assert(_budget <= std::numeric_limits<Cost>::max() / 2);
        // Cleared with the pairs it points into: a kept solution would read
        // the rebuilt, or reallocated-away, entries.
        _best_sol.resize(0);
        _state_limit_reached = false;
        _permuted_items.resize(0);
        _value_cost_pairs.resize(0);
        _chunks.resize(0);
        std::vector<double> ratios;
        if constexpr(std::ranges::sized_range<ItemRange>) {
            auto num_items = std::ranges::size(_items_range);
            _permuted_items.reserve(num_items);
            _value_cost_pairs.reserve(num_items);
            ratios.reserve(num_items);
        }
        for(auto it = _items_range.begin(); it != _items_range.end(); ++it) {
            const auto & i = *it;
            const Value value = _value_map[i];
            if(value == static_cast<Value>(0)) continue;
            const Cost cost = _cost_map[i];
            if(cost > _budget) continue;
            _permuted_items.emplace_back(it);
            _value_cost_pairs.emplace_back(value, cost);
            ratios.push_back(value_cost_ratio(_value_cost_pairs.back()));
        }
        // Not the ratio alone: the chunks below are built from adjacent
        // identical items, which an arbitrary order among equal ratios
        // interleaves with their neighbours.
        std::ranges::sort(
            std::views::zip(_permuted_items, _value_cost_pairs, ratios),
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
            // > 1, not != 1: the divisor is 0 when every kept cost is, and
            // % 0 traps.
            if(divisor > 1)
                _search_budget = static_cast<Cost>(_budget - _budget % divisor);
        }

        const std::size_t num_items = _value_cost_pairs.size();
        for(std::size_t first = 0; first < num_items;) {
            const auto [value, cost] = _value_cost_pairs[first];
            std::size_t last = first + 1;
            while(last < num_items &&
                  _value_cost_pairs[last] == _value_cost_pairs[first])
                ++last;
            // No more copies than the budget holds together: the others are
            // never taken with them, and totalling them overflows a Cost
            // only wide enough for twice the budget.
            std::size_t copies = last - first;
            if(static_cast<Cost>(0) < cost) {
                if constexpr(std::is_integral_v<Cost>) {
                    copies = std::min(copies, static_cast<std::size_t>(
                                                  _search_budget / cost));
                } else {
                    // One more than the quotient, which can round one copy
                    // short of what the budget holds: that copy would never
                    // be offered.
                    const Cost fit = _search_budget / cost;
                    if(fit + 1 < static_cast<Cost>(copies))
                        copies = static_cast<std::size_t>(fit) + 1;
                }
            }
            std::size_t offset = first;
            for(std::size_t power = 1; copies > 0; power *= 2) {
                const std::size_t count = std::min(power, copies);
                _chunks.push_back(item_chunk{
                    static_cast<Value>(value * static_cast<Value>(count)),
                    static_cast<Cost>(cost * static_cast<Cost>(count)),
                    ratios[first], offset, count});
                offset += count;
                copies -= count;
            }
            first = last;
        }
        return *this;
    }

    // Re-derives through reset(): the item filter built there depends on the
    // budget, so assigning _budget alone would leave newly-affordable items
    // excluded and run() would answer a wrong optimum after a raise.
    knapsack_dp & set_budget(Cost b) {
        _budget = b;
        return reset();
    }

    // run() settles for a solution within this relative gap of the optimum:
    // solution_value() >= optimum / (1 + gap). 0, the default, is exact.
    knapsack_dp & set_relative_gap(double gap) {
        assert(gap >= 0);
        _relative_gap = gap;
        return *this;
    }

    // The search gives up, keeping its incumbent, rather than hold more than
    // `limit` states -- about 75 bytes each: state_limit_reached() then
    // answers true and run_with_timeout() false. No limit by default: an
    // exact solver that stops short has to be asked to.
    knapsack_dp & set_state_limit(std::size_t limit) {
        _state_limit = limit;
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    knapsack_dp & run() {
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
        // Joined in time is not completed: the search may have given up at
        // the state limit.
        return !_state_limit_reached;
    }

    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] bool state_limit_reached() const {
        return _state_limit_reached;
    }

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
knapsack_dp(ItemRange &&, ValueMap &&, CostMap &&, auto &&)
    -> knapsack_dp<std::views::all_t<ItemRange>, maps::mapping_all_t<ValueMap>,
                   maps::mapping_all_t<CostMap>>;

}  // namespace melon
