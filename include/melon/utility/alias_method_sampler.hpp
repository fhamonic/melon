#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <memory>
#include <random>
#include <ranges>
#include <utility>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/container/static_map.hpp"
#include "melon/detail/stdlib_check.hpp"

namespace melon {

// clang-format off
template <typename Traits>
concept alias_method_sampler_traits = requires() {
    { Traits::heuristic_preprocessing } -> std::convertible_to<bool>;
};
// clang-format on

struct alias_method_sampler_default_traits {
    static constexpr bool heuristic_preprocessing = false;
};

template <typename ItemRange, typename Prob,
          alias_method_sampler_traits Traits =
              alias_method_sampler_default_traits>
class alias_method_sampler {
private:
    using index_type = int;

    ItemRange _items;
    static_map<index_type, Prob> _probs;
    static_map<index_type, index_type> _aliases;
    index_type _last_index;

public:
    template <std::ranges::random_access_range R,
              std::invocable<std::ranges::range_value_t<R>> P>
    constexpr alias_method_sampler(R && items, P && prob_map)
        : _items(std::views::all(std::forward<R>(items)))
        , _probs(_items.size())
        , _aliases(_items.size())
        , _last_index(static_cast<index_type>(_items.size()) - index_type{1}) {
        // An empty item range would give the index distribution the range
        // [0, -1], whose precondition is a <= b.
        assert(!std::ranges::empty(_items));
        const std::size_t n = _items.size();
        auto overfull_buckets = std::make_unique_for_overwrite<index_type[]>(n);
        auto underfull_buckets =
            std::make_unique_for_overwrite<index_type[]>(n);

        auto overfull_end = overfull_buckets.get();
        auto underfull_end = underfull_buckets.get();

        for(auto && [i, item] : std::views::enumerate(_items)) {
            const auto prob = prob_map(item) * static_cast<Prob>(n);
            _probs[static_cast<index_type>(i)] = prob;
            *overfull_end = *underfull_end = static_cast<index_type>(i);
            const bool is_underfull = (prob < 1.0);
            underfull_end += is_underfull;
            overfull_end += !is_underfull;
        }

        auto overfull_it = overfull_buckets.get();
        auto underfull_it = underfull_buckets.get();

        if constexpr(Traits::heuristic_preprocessing) {
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
    template <typename... Args>
    constexpr alias_method_sampler(Traits, Args &&... args)
        : alias_method_sampler(std::forward<Args>(args)...) {}

    constexpr alias_method_sampler(const alias_method_sampler &) = default;
    constexpr alias_method_sampler(alias_method_sampler &&) = default;

    constexpr alias_method_sampler & operator=(const alias_method_sampler &) =
        default;
    constexpr alias_method_sampler & operator=(alias_method_sampler &&) =
        default;

    // The two distributions are locals, not `mutable` members. A distribution
    // carries its own state, so writing one through a const operator() made
    // two threads sampling from the same const sampler race -- the same
    // defect erdos_renyi's function-local statics had, in a per-object form.
    // Both are trivially constructed from their bounds.
    template <typename Generator>
    [[nodiscard]] decltype(auto) operator()(Generator & gen) const {
        std::uniform_int_distribution<index_type> index_distribution(
            index_type{0}, _last_index);
        std::uniform_real_distribution<Prob> prob_distribution(0.0, 1.0);
        const index_type i = index_distribution(gen);
        const auto prob = _probs[i];
        const auto alias = _aliases[i];
        return _items[i + (prob_distribution(gen) > prob) * (alias - i)];
    }
};

// No Traits parameter: the class template's own default supplies it, so the
// deduced type and the explicitly written `alias_method_sampler<R, P>` agree.
template <typename Range, typename ProbMap>
alias_method_sampler(Range &&, ProbMap &&)
    -> alias_method_sampler<
        std::views::all_t<Range>,
        std::invoke_result_t<ProbMap, std::ranges::range_value_t<Range>>>;

template <typename Range, typename ProbMap, typename Traits>
alias_method_sampler(Traits, Range &&, ProbMap &&)
    -> alias_method_sampler<
        std::views::all_t<Range>,
        std::invoke_result_t<ProbMap, std::ranges::range_value_t<Range>>,
        Traits>;

}  // namespace melon
