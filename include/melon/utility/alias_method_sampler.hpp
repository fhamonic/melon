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
        auto overfull_buckets = std::make_unique_for_overwrite<index_type[]>(n);
        auto underfull_buckets =
            std::make_unique_for_overwrite<index_type[]>(n);

        auto overfull_end = overfull_buckets.get();
        auto underfull_end = underfull_buckets.get();

        for(auto && [i, item] : std::views::enumerate(_items)) {
            const auto prob = prob_map(item) * static_cast<_Prob>(n);
            _probs[static_cast<index_type>(i)] = prob;
            *overfull_end = *underfull_end = static_cast<index_type>(i);
            const bool is_underfull = (prob < 1.0);
            underfull_end += is_underfull;
            overfull_end += !is_underfull;
        }

        auto overfull_it = overfull_buckets.get();
        auto underfull_it = underfull_buckets.get();

        if constexpr(_Traits::heuristic_preprocessing) {
            std::make_heap(
                overfull_it, overfull_end,
                [this](auto && i, auto && j) { return _probs[i] < _probs[j]; });
            std::make_heap(
                underfull_it, underfull_end,
                [this](auto && i, auto && j) { return _probs[i] > _probs[j]; });
        }

        for(; overfull_it != overfull_end && underfull_it != underfull_end;
            ++overfull_it, ++underfull_it) {
            const index_type overfull_index = *overfull_it;
            const index_type underfull_index = *underfull_it;
            auto & overfull_prob = _probs[overfull_index];
            const auto & underfull_prob = _probs[underfull_index];
            auto & underfull_alias = _aliases[underfull_index];

            overfull_prob = (overfull_prob + underfull_prob) - 1.0;
            underfull_alias = overfull_index;

            *overfull_end = *underfull_end = overfull_index;
            const bool became_underfull = (overfull_prob < 1.0);
            underfull_end += became_underfull;
            overfull_end += !became_underfull;
        }
        for(; overfull_it != overfull_end; ++overfull_it)
            _probs[*overfull_it] = 1.0;
        for(; underfull_it != underfull_end; ++underfull_it)
            _probs[*underfull_it] = 1.0;
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
