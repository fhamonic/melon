#ifndef RANDOM_RANGES_HELPER_HPP
#define RANDOM_RANGES_HELPER_HPP

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <random>
#include <ranges>
#include <string>
#include <vector>

// One engine for the whole run, with a fixed default seed announced on first
// use. Seeding from std::random_device instead would make a randomized failure
// unreproducible: the input that broke it dies with the process, and rerunning
// rolls a different one. MELON_TEST_SEED replays what a red run printed.
//
// A function-local static, not a namespace-scope object: these helpers are
// included from every test translation unit, and a shared engine has to be one
// object with one seed rather than one per unit.
inline std::mt19937 & test_rng() {
    static std::mt19937 engine = [] {
        unsigned seed = 20260731u;
        if(const char * const from_env = std::getenv("MELON_TEST_SEED");
           from_env != nullptr) {
            seed = static_cast<unsigned>(std::stoul(from_env));
        }
        std::cerr << "[melon] random seed: " << seed
                  << " (set MELON_TEST_SEED to replay a failure)\n";
        return std::mt19937(seed);
    }();
    return engine;
}

template <typename T>
std::vector<T> random_vector(std::size_t size, T min_value = 0,
                             T max_value = 100) {
    std::uniform_int_distribution<T> distr{min_value, max_value};

    std::vector<T> out(size);
    std::ranges::generate(out, [&]() { return distr(test_rng()); });
    return out;
}

template <typename T>
std::vector<T> random_vector_all_diff(std::size_t size, T min_value = 0,
                                      T max_value = 100) {
    assert(size <= static_cast<std::size_t>(max_value - min_value + 1));

    std::vector<T> out;
    out.reserve(size);
    std::ranges::copy(std::views::iota(min_value, max_value + 1),
                      std::back_inserter(out));
    std::ranges::shuffle(out, test_rng());
    out.resize(size);
    return out;
}

template <std::ranges::forward_range T>
std::ranges::range_value_t<T> random_element(T && r) {
    const auto size = static_cast<std::size_t>(std::ranges::distance(r));
    std::uniform_int_distribution<std::size_t> distr{0ul, size - 1};
    auto it = r.begin();
    auto pos = distr(test_rng());
    std::advance(it, pos);
    return *it;
}

#endif  // RANDOM_RANGES_HELPER_HPP
