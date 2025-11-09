#undef NDEBUG
#include <gtest/gtest.h>

#include <random>
#include <unordered_map>
#include <vector>

#include "melon/utility/alias_method_sampler.hpp"

#include "random_ranges_helper.hpp"
#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

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

GTEST_TEST(alias_method_sampler, statistics) {
    std::vector<int> vec = {2, 4, 8, 16, 16};
    alias_method_sampler sampler(vec, [](const int & i) { return 1.0 / i; });

    std::unordered_map<int, double> count_map;
    for(auto && i : vec) count_map[i] = 0.0;

    std::random_device dev;
    std::mt19937 rng(dev());
    for(int i = 0; i < 1000; ++i) count_map[sampler(rng)] += 1e-3;

    ASSERT_NEAR(count_map[2], 0.5, 0.05);
    ASSERT_NEAR(count_map[4], 0.25, 0.05);
    ASSERT_NEAR(count_map[8], 0.125, 0.05);
    ASSERT_NEAR(count_map[16], 0.125, 0.05);
}
