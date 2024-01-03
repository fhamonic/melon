#ifndef MELON_ALGORITHM_BRANCH_AND_BOUND_HPP
#define MELON_ALGORITHM_BRANCH_AND_BOUND_HPP

#include <chrono>
#include <future>
#include <numeric>
#include <ranges>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <range/v3/algorithm/sort.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include "melon/mapping.hpp"

namespace fhamonic {
namespace melon {

template <std::ranges::range _ItemRange,
          input_mapping<std::ranges::range_value_t<_ItemRange>> _ValueMap,
          input_mapping<std::ranges::range_value_t<_ItemRange>> _CostMap>
    requires std::is_arithmetic_v<mapped_value_t<
                 _ValueMap, std::ranges::range_value_t<_ItemRange>>> &&
             std::is_arithmetic_v<mapped_value_t<
                 _CostMap, std::ranges::range_value_t<_ItemRange>>>
class knapsack_bnb {
private:
    using Item = std::ranges::range_value_t<_ItemRange>;
    using Value = mapped_value_t<_ValueMap, Item>;
    using Cost = mapped_value_t<_CostMap, Item>;

    _ItemRange _items_range;
    _ValueMap _value_map;
    _CostMap _cost_map;

    Cost _budget;
    std::vector<std::ranges::iterator_t<_ItemRange>> _permuted_items;
    std::vector<std::pair<Value, Cost>> _value_cost_pairs;
    std::vector<typename std::vector<std::pair<Value, Cost>>::const_iterator>
        _best_sol;

private:
    double value_cost_ratio(const std::pair<Value, Cost> & p) const noexcept {
        if constexpr(std::numeric_limits<float>::is_iec559) {
            return p.first / static_cast<double>(p.second);
        } else {
            return (p.second == 0) ? std::numeric_limits<double>::max()
                                   : (p.first / static_cast<double>(p.second));
        }
    }

    Value computeUpperBound(auto it, const auto end, Value bound_value,
                            Cost bound_budget_left) const noexcept {
        for(; it < end; ++it) {
            if(bound_budget_left < it->second)
                return static_cast<Value>(bound_value +
                                          bound_budget_left * it->first /
                                              static_cast<double>(it->second));
            bound_budget_left -= it->second;
            bound_value += it->first;
        }

        return bound_value;
    }

    void iterative_bnb() noexcept {
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
    bool iterative_bnb_timeout(std::stop_token stoken) noexcept {
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
    template <typename _IR, typename _VM, typename _CM, typename _B>
    knapsack_bnb(_IR && items_range, _VM && value_map, _CM && cost_map,
                 const _B budget) noexcept
        : _items_range(std::ranges::views::all(std::forward<_IR>(items_range)))
        , _value_map(views::mapping_all(std::forward<_VM>(value_map)))
        , _cost_map(views::mapping_all(std::forward<_CM>(cost_map)))
        , _budget(budget) {
        reset();
    }

    knapsack_bnb & reset() noexcept {
        _permuted_items.resize(0);
        _value_cost_pairs.resize(0);
        if constexpr(std::ranges::sized_range<_ItemRange>) {
            auto nb_items = std::ranges::size(_items_range);
            _permuted_items.reserve(nb_items);
            _value_cost_pairs.reserve(nb_items);
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
        auto zip_view = ranges::view::zip(_permuted_items, _value_cost_pairs);
        ranges::sort(zip_view, [this](auto p1, auto p2) {
            return value_cost_ratio(p1.second) > value_cost_ratio(p2.second);
        });
        return *this;
    }

    knapsack_bnb & set_budget(Cost b) noexcept {
        _budget = b;
        return *this;
    }

    knapsack_bnb & run() noexcept {
        iterative_bnb();
        return *this;
    }

    template <typename _Rep, typename _Period>
    bool run_with_timeout(
        const std::chrono::duration<_Rep, _Period> & timeout) noexcept {
        std::jthread t([this](std::stop_token stoken) {
            return iterative_bnb_timeout(stoken);
        });
        // C++23 should allow to call jthread from future and prevent launching
        // the supplementary thread for join
        auto future = std::async(std::launch::async, &std::jthread::join, &t);
        if(future.wait_for(timeout) == std::future_status::timeout) {
            t.request_stop();
            return false;
        }
        return true;
    }

    auto solution_items() const noexcept {
        return std::ranges::views::transform(_best_sol, [this](auto && it) {
            return *_permuted_items[static_cast<std::size_t>(
                std::distance(_value_cost_pairs.cbegin(), it))];
        });
    }

    auto solution_value() const noexcept {
        Value sum = 0;
        for(auto && it : _best_sol) sum += it->first;
        return sum;
    }

    auto solution_cost() const noexcept {
        Cost sum = 0;
        for(auto && it : _best_sol) sum += it->second;
        return sum;
    }
};

template <typename _ItemRange, typename _ValueMap, typename _CostMap>
knapsack_bnb(_ItemRange &&, _ValueMap &&, _CostMap &&, auto &&)
    -> knapsack_bnb<std::ranges::views::all_t<_ItemRange>,
                    views::mapping_all_t<_ValueMap>,
                    views::mapping_all_t<_CostMap>>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_BRANCH_AND_BOUND_HPP