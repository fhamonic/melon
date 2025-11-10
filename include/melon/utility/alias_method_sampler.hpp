#ifndef MELON_UTILITY_ALIAS_METHOD_SAMPLER_HPP
#define MELON_UTILITY_ALIAS_METHOD_SAMPLER_HPP

#include <algorithm>
#include <concepts>
#include <memory>
#include <random>
#include <ranges>
#include <utility>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/container/static_map.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
template <typename _Traits>
concept alias_method_sampler_trait = requires() {
    { _Traits::heuristic_preprocessing } -> std::convertible_to<bool>;
};
// clang-format on

struct alias_method_sampler_default_traits {
    static constexpr bool heuristic_preprocessing = false;
};

template <typename _ItemRange, typename _Prob,
          alias_method_sampler_trait _Traits>
class alias_method_sampler {
private:
    using index_type = int;

    _ItemRange _items;
    static_map<index_type, _Prob> _probs;
    static_map<index_type, index_type> _aliases;
    mutable std::uniform_int_distribution<index_type> _index_distribution;
    mutable std::uniform_real_distribution<_Prob> _prob_distribution;

public:
    template <std::ranges::random_access_range _R,
              std::invocable<std::ranges::range_value_t<_R>> _P>
    [[nodiscard]] constexpr alias_method_sampler(_R && items, _P && prob_map)
        : _items(std::views::all(std::forward<_R>(items)))
        , _probs(_items.size())
        , _aliases(_items.size())
        , _index_distribution(
              index_type{0},
              static_cast<index_type>(_items.size()) - index_type{1})
        , _prob_distribution(0.0, 1.0) {
        const std::size_t n = _items.size();
        auto overful_buckets = std::make_unique_for_overwrite<index_type[]>(n);
        auto underful_buckets = std::make_unique_for_overwrite<index_type[]>(n);

        auto overful_end = overful_buckets.get();
        auto underful_end = underful_buckets.get();

        for(auto && [i, item] : std::views::enumerate(_items)) {
            const auto prob = prob_map(item) * static_cast<_Prob>(n);
            _probs[static_cast<index_type>(i)] = prob;
            *overful_end = *underful_end = static_cast<index_type>(i);
            const bool is_underful = (prob < 1.0);
            underful_end += is_underful;
            overful_end += !is_underful;
        }

        auto overful_it = overful_buckets.get();
        auto underful_it = underful_buckets.get();

        if constexpr(_Traits::heuristic_preprocessing) {
            std::make_heap(
                overful_it, overful_end,
                [this](auto && i, auto && j) { return _probs[i] < _probs[j]; });
            std::make_heap(
                underful_it, underful_end,
                [this](auto && i, auto && j) { return _probs[i] > _probs[j]; });
        }

        for(; overful_it != overful_end && underful_it != underful_end;
            ++overful_it, ++underful_it) {
            const index_type overful_index = *overful_it;
            const index_type underful_index = *underful_it;
            auto & overful_prob = _probs[overful_index];
            const auto & underful_prob = _probs[underful_index];
            auto & underful_alias = _aliases[underful_index];

            underful_alias = overful_index;
            overful_prob = (overful_prob + underful_prob) - 1.0;

            *overful_end = *underful_end = overful_index;
            const bool became_underful = (overful_prob < 1.0);
            underful_end += became_underful;
            overful_end += !became_underful;
        }
        for(; overful_it != overful_end; ++overful_it)
            _probs[*overful_it] = 1.0;
        for(; underful_it != underful_end; ++underful_it)
            _probs[*underful_it] = 1.0;
    }

public:
    template <typename... _Args>
    [[nodiscard]] constexpr alias_method_sampler(_Traits, _Args &&... args)
        : alias_method_sampler(std::forward<_Args>(args)...) {}

    [[nodiscard]] constexpr alias_method_sampler(const alias_method_sampler &) =
        default;
    [[nodiscard]] constexpr alias_method_sampler(alias_method_sampler &&) =
        default;

    constexpr alias_method_sampler & operator=(const alias_method_sampler &) =
        default;
    constexpr alias_method_sampler & operator=(alias_method_sampler &&) =
        default;

    template <typename Generator>
    [[nodiscard]] decltype(auto) operator()(Generator & gen) const {
        const index_type i = _index_distribution(gen);
        const auto prob = _probs[i];
        const auto alias = _aliases[i];
        return _items[i + (_prob_distribution(gen) > prob) * (alias - i)];
    }
};

template <typename _Range, typename _ProbMap,
          typename _Traits = alias_method_sampler_default_traits>
alias_method_sampler(_Range &&, _ProbMap &&) -> alias_method_sampler<
    std::views::all_t<_Range>,
    std::invoke_result_t<_ProbMap, std::ranges::range_value_t<_Range>>,
    _Traits>;

template <typename _Range, typename _ProbMap, typename _Traits>
alias_method_sampler(_Traits, _Range &&, _ProbMap &&) -> alias_method_sampler<
    std::views::all_t<_Range>,
    std::invoke_result_t<_ProbMap, std::ranges::range_value_t<_Range>>,
    _Traits>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILITY_ALIAS_METHOD_SAMPLER_HPP
