#undef NDEBUG
#include <gtest/gtest.h>

#include <random>
#include <unordered_map>
#include <vector>

#include "melon/utility/alias_method_sampler.hpp"

#include "random_ranges_helper.hpp"
#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// the sampler is built from a range of values and a probability map
////////////////////////////////////////////////////////////////////////////////

static std::vector<double> make_weights(std::size_t n) {
    std::vector<double> w(n);
    std::mt19937 rng(180);
    std::uniform_real_distribution<double> dist(1, 10);
    // std::exponential_distribution<double> dist(1);
    // std::normal_distribution<double> dist(5, 2);
    for(auto & x : w) x = dist(rng);
    return w;
}

GTEST_TEST(alias_method_sampler, construct) {
    auto weights = make_weights(10ul);
    const double weights_sum = std::reduce(weights.begin(), weights.end());
    auto prob_map = [&](std::size_t i) { return weights[i] / weights_sum; };
    alias_method_sampler sampler(std::views::iota(0ul, 10ul), prob_map);
}

////////////////////////////////////////////////////////////////////////////////
// drawn samples follow the requested distribution
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(alias_method_sampler, statistics) {
    std::vector<int> vec = {2, 4, 8, 16, 16};
    alias_method_sampler sampler(vec, [](const int & i) { return 1.0 / i; });

    std::unordered_map<int, double> count_map;
    for(auto && i : vec) count_map[i] = 0.0;

    std::random_device dev;
    std::mt19937 rng(dev());
    for(int i = 0; i < 10000; ++i) count_map[sampler(rng)] += 1e-4;

    ASSERT_NEAR(count_map[2], 0.5, 0.05);
    ASSERT_NEAR(count_map[4], 0.25, 0.05);
    ASSERT_NEAR(count_map[8], 0.125, 0.05);
    ASSERT_NEAR(count_map[16], 0.125, 0.05);
}

// regression: weights that do not sum to one silently built a garbage table;
// like std::discrete_distribution, they are now normalized by their sum.
GTEST_TEST(alias_method_sampler, unnormalized_weights_are_normalized) {
    std::vector<int> vec = {2, 4, 8, 16, 16};
    // Same distribution as above, scaled by 8: weights 4, 2, 1, 1/2, 1/2.
    alias_method_sampler sampler(vec, [](const int & i) { return 8.0 / i; });

    std::unordered_map<int, double> count_map;
    for(auto && i : vec) count_map[i] = 0.0;

    std::mt19937 rng(180);
    for(int i = 0; i < 10000; ++i) count_map[sampler(rng)] += 1e-4;

    ASSERT_NEAR(count_map[2], 0.5, 0.05);
    ASSERT_NEAR(count_map[4], 0.25, 0.05);
    ASSERT_NEAR(count_map[8], 0.125, 0.05);
    ASSERT_NEAR(count_map[16], 0.125, 0.05);
}

// An integer prob map must fail melon's floating_point constraint, not
// libstdc++'s static_assert inside <random>.
namespace int_prob {
template <typename Prob>
concept valid_prob = requires {
    typename alias_method_sampler<std::views::all_t<std::vector<int> &>, Prob>;
};
}  // namespace int_prob
static_assert(int_prob::valid_prob<double>);
static_assert(!int_prob::valid_prob<int>);

////////////////////////////////////////////////////////////////////////////////
// regression (2.3): the Traits default lives on the class, not the deduction
// guide
////////////////////////////////////////////////////////////////////////////////

// Same as the dijkstra family: the default lived on the deduction guide only,
// so `alias_method_sampler<R, P>` did not compile and CTAD's result could not
// be named. It is on the class now, and the guide no longer carries one.
namespace traits_default {
using R = std::views::all_t<std::vector<int> &>;

using written_out = alias_method_sampler<R, double>;

struct heuristic_traits {
    static constexpr bool heuristic_preprocessing = true;
};
}  // namespace traits_default

static_assert(alias_method_sampler_traits<alias_method_sampler_default_traits>);
static_assert(alias_method_sampler_traits<traits_default::heuristic_traits>);
static_assert(
    std::same_as<traits_default::written_out,
                 alias_method_sampler<traits_default::R, double,
                                      alias_method_sampler_default_traits>>);

// CTAD lands on the type the two-argument spelling names
static_assert(std::same_as<decltype(alias_method_sampler(
                               std::declval<std::vector<int> &>(),
                               std::declval<double (&)(const int &)>())),
                           traits_default::written_out>);

// and an explicit Traits still wins over the default
static_assert(
    std::same_as<decltype(alias_method_sampler(
                     std::declval<traits_default::heuristic_traits>(),
                     std::declval<std::vector<int> &>(),
                     std::declval<double (&)(const int &)>())),
                 alias_method_sampler<traits_default::R, double,
                                      traits_default::heuristic_traits>>);

GTEST_TEST(alias_method_sampler, default_traits_spelling_runs) {
    std::vector<int> vec = {2, 4, 8, 16, 16};
    auto prob_map = [](const int & i) { return 1.0 / i; };

    alias_method_sampler<std::views::all_t<std::vector<int> &>, double> sampler(
        vec, prob_map);

    std::mt19937 rng(180);
    for(int i = 0; i < 100; ++i)
        ASSERT_NE(std::ranges::find(vec, sampler(rng)), vec.end());
}
