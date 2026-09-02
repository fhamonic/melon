#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <memory>
#include <random>
#include <ranges>
#include <utility>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/container/static_map.hpp"
#include "melon/detail/stdlib_check.hpp"
#include "melon/mapping.hpp"

namespace melon {

template <typename Traits>
concept alias_method_sampler_traits = requires {
    { Traits::heuristic_preprocessing } -> std::convertible_to<bool>;
};

struct alias_method_sampler_default_traits {
    static constexpr bool heuristic_preprocessing = false;
};

// random_access_range: operator() subscripts the items by drawn index.
// floating_point Prob: the table entries feed std::uniform_real_distribution,
// so an integer prob map must fail here, at a melon concept, rather than at
// libstdc++'s static_assert deep inside <random>.
template <std::ranges::random_access_range ItemRange, std::floating_point Prob,
          alias_method_sampler_traits Traits =
              alias_method_sampler_default_traits>
class alias_method_sampler {
public:
    // What operator() hands back is a reference into the item range, not a
    // copy, so it is valid only while the sampler's items are.
    using result_type = std::ranges::range_reference_t<ItemRange>;

private:
    using index_type = int;

    ItemRange _items;
    static_map<index_type, Prob> _probs;
    static_map<index_type, index_type> _aliases;
    index_type _last_index;

public:
    // Not `mapping_for`: the map is read here once and never stored, so
    // mapping_all serves only the subscript dispatch that lets a callable, a
    // vector or a vertex map all read as `probs[item]`.
    template <std::ranges::random_access_range R, typename P>
        requires mapping<maps::mapping_all_t<P>,
                         std::ranges::range_reference_t<R>>
    constexpr alias_method_sampler(R && items, P && prob_map)
        : _items(std::views::all(std::forward<R>(items)))
        , _probs(_items.size())
        , _aliases(_items.size())
        , _last_index(static_cast<index_type>(_items.size()) - index_type{1}) {
        // An empty item range would give the index distribution the range
        // [0, -1], whose precondition is a <= b.
        assert(!std::ranges::empty(_items));
        auto && probs = maps::mapping_all(std::forward<P>(prob_map));
        const std::size_t n = _items.size();
        auto overfull_buckets = std::make_unique_for_overwrite<index_type[]>(n);
        auto underfull_buckets =
            std::make_unique_for_overwrite<index_type[]>(n);

        auto overfull_end = overfull_buckets.get();
        auto underfull_end = underfull_buckets.get();

        // Like std::discrete_distribution, the weights need not sum to one;
        // they must be non-negative and not all zero, both asserted.
        Prob weights_sum = Prob{0};
        for(auto && [i, item] : std::views::enumerate(_items)) {
            const Prob w = probs[item];
            assert(w >= Prob{0});
            _probs[static_cast<index_type>(i)] = w;
            weights_sum += w;
        }
        assert(weights_sum > Prob{0});
        const Prob scale = static_cast<Prob>(n) / weights_sum;
        for(index_type i = 0; i < static_cast<index_type>(n); ++i) {
            const Prob prob = _probs[i] * scale;
            _probs[i] = prob;
            // Identity by default: slots the pairing loop below never writes
            // (exactly-full ones) are still read branchlessly in operator().
            _aliases[i] = i;
            *overfull_end = *underfull_end = i;
            const bool is_underfull = (prob < Prob{1});
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

            overfull_prob = (overfull_prob + underfull_prob) - Prob{1};
            underfull_alias = overfull_index;

            *overfull_end = *underfull_end = overfull_index;
            const bool became_underfull = (overfull_prob < Prob{1});
            underfull_end += became_underfull;
            overfull_end += !became_underfull;
        }
        for(; overfull_it != overfull_end; ++overfull_it)
            _probs[*overfull_it] = Prob{1};
        for(; underfull_it != underfull_end; ++underfull_it)
            _probs[*underfull_it] = Prob{1};
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

    // The two distributions are locals, not `mutable` members: a distribution
    // carries its own state, so writing one through a const operator() would
    // race between two threads sampling from the same const sampler.
    template <std::uniform_random_bit_generator Generator>
    [[nodiscard]] decltype(auto) operator()(Generator & gen) const {
        std::uniform_int_distribution<index_type> index_distribution(
            index_type{0}, _last_index);
        std::uniform_real_distribution<Prob> prob_distribution(Prob{0},
                                                               Prob{1});
        const index_type i = index_distribution(gen);
        const auto prob = _probs[i];
        const auto alias = _aliases[i];
        // `>=`, not `>`: uniform_real can return exactly 0.0, and a
        // zero-probability slot must send that draw to its alias too.
        return _items[i + (prob_distribution(gen) >= prob) * (alias - i)];
    }
};

// mapped_value_t is decayed where invoke_result_t is not: a map handing out
// `const double &` must deduce Prob = double, or `std::floating_point<Prob>`
// rejects the reference with a deduction failure naming neither.
template <typename Range, typename ProbMap>
alias_method_sampler(Range &&, ProbMap &&)
    -> alias_method_sampler<
        std::views::all_t<Range>,
        mapped_value_t<maps::mapping_all_t<ProbMap>,
                       std::ranges::range_reference_t<Range>>>;

template <typename Range, typename ProbMap, typename Traits>
alias_method_sampler(Traits, Range &&, ProbMap &&)
    -> alias_method_sampler<
        std::views::all_t<Range>,
        mapped_value_t<maps::mapping_all_t<ProbMap>,
                       std::ranges::range_reference_t<Range>>,
        Traits>;

}  // namespace melon
