#undef NDEBUG
#include <gtest/gtest.h>

#include <numeric>
#include <random>
#include <unordered_map>
#include <vector>

#include "melon/container/static_digraph.hpp"
#include "melon/utility/alias_method_sampler.hpp"
#include "melon/utility/static_digraph_builder.hpp"

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

    std::mt19937 rng(test_rng()());
    for(int i = 0; i < 10000; ++i) count_map[sampler(rng)] += 1e-4;

    ASSERT_NEAR(count_map[2], 0.5, 0.05);
    ASSERT_NEAR(count_map[4], 0.25, 0.05);
    ASSERT_NEAR(count_map[8], 0.125, 0.05);
    ASSERT_NEAR(count_map[16], 0.125, 0.05);
}

// regression: like std::discrete_distribution, weights are normalized by
// their sum -- taken raw, weights that do not sum to one silently build a
// garbage table.
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

////////////////////////////////////////////////////////////////////////////////
// the probability map is a mapping: any const-readable map, callable included
////////////////////////////////////////////////////////////////////////////////

static void assert_distribution(auto & sampler, const auto & items,
                                const std::vector<double> & expected) {
    std::unordered_map<std::decay_t<decltype(*items.begin())>, double> count;
    std::mt19937 rng(180);
    for(int i = 0; i < 10000; ++i) count[sampler(rng)] += 1e-4;
    std::size_t k = 0;
    for(auto && item : items) ASSERT_NEAR(count[item], expected[k++], 0.05);
}

// A vector indexed by the item is a map on its own; the lambda that used to
// be needed to subscript it is gone.
GTEST_TEST(alias_method_sampler, vector_as_prob_map) {
    std::vector<double> weight = {0.5, 0.25, 0.125, 0.125};
    auto items = std::views::iota(0ul, weight.size());
    alias_method_sampler sampler(items, weight);
    static_assert(std::same_as<decltype(sampler),
                               alias_method_sampler<decltype(items), double>>);
    assert_distribution(sampler, items, weight);
}

// A graph's vertex map keyed by the vertices being sampled.
GTEST_TEST(alias_method_sampler, vertex_map_as_prob_map) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc({0, 1}).add_arc({1, 2}).add_arc({2, 3});
    auto [graph] = builder.build();
    auto weight = create_vertex_map<float>(graph);
    weight[0] = 4.0f;
    weight[1] = 2.0f;
    weight[2] = 1.0f;
    weight[3] = 1.0f;
    alias_method_sampler sampler(vertices(graph), weight);
    static_assert(std::same_as<typename decltype(sampler)::result_type,
                               vertex_t<static_digraph>>);
    assert_distribution(sampler, vertices(graph), {0.5, 0.25, 0.125, 0.125});
}

// A map handing out a reference deduces the decayed Prob: invoke_result_t
// would have made it `const double &`, which floating_point rejects.
GTEST_TEST(alias_method_sampler, reference_yielding_map_deduces_value_type) {
    std::vector<double> weight = {0.5, 0.25, 0.125, 0.125};
    auto items = std::views::iota(0ul, weight.size());
    alias_method_sampler sampler(
        items, [&](std::size_t i) -> const double & { return weight[i]; });
    static_assert(std::same_as<decltype(sampler),
                               alias_method_sampler<decltype(items), double>>);
    assert_distribution(sampler, items, weight);
}

// The const-readability rule of every melon mapping applies to an owned map:
// an rvalue mutable lambda is not a prob map. An lvalue one goes through the
// shallow-const mapping_ref_view, like everywhere else in melon. The mutable
// lambda captures on purpose: a captureless one converts to a function
// pointer, whose surrogate call is const, and would be accepted.
namespace prob_map_shapes {
using R = std::vector<std::size_t> &;
template <typename P>
concept accepted =
    requires(R r, P && p) { alias_method_sampler(r, std::forward<P>(p)); };
inline auto const_lambda = [](std::size_t i) {
    return 1.0 / static_cast<double>(i);
};
inline auto mutable_lambda = [calls = 0](std::size_t i) mutable {
    ++calls;
    return 1.0 / static_cast<double>(i);
};
}  // namespace prob_map_shapes
static_assert(
    prob_map_shapes::accepted<decltype(prob_map_shapes::const_lambda)>);
static_assert(prob_map_shapes::accepted<std::vector<double> &>);
static_assert(
    prob_map_shapes::accepted<decltype(prob_map_shapes::mutable_lambda) &>);
static_assert(
    !prob_map_shapes::accepted<decltype(prob_map_shapes::mutable_lambda)>);

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

// Same as the dijkstra family: with the default on the deduction guide only,
// `alias_method_sampler<R, P>` does not compile and CTAD's result cannot be
// named. It lives on the class, and the guide carries none.
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
                               std::declval<double (*)(const int &)>())),
                           traits_default::written_out>);

// and an explicit Traits still wins over the default
static_assert(
    std::same_as<decltype(alias_method_sampler(
                     std::declval<traits_default::heuristic_traits>(),
                     std::declval<std::vector<int> &>(),
                     std::declval<double (*)(const int &)>())),
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
