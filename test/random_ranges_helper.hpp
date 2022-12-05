#ifndef RANDOM_RANGES_HELPER_HPP
#define RANDOM_RANGES_HELPER_HPP

#include <algorithm>
#include <random>
#include <ranges>
#include <vector>

template <typename T>
std::vector<T> random_vector(std::size_t size, T min_value = 0,
                             T max_value = 100) {
    std::uniform_int_distribution<T> distr{min_value, max_value};
    std::mt19937 engine{std::random_device{}()};

    std::vector<T> out(size);
    std::ranges::generate(out, [&]() { return distr(engine); });
    return out;
}

template <typename T>
std::vector<T> random_vector_all_diff(std::size_t size, T min_value = 0,
                                      T max_value = 100) {
    assert(size <= static_cast<std::size_t>(max_value - min_value + 1));
    std::mt19937 engine{std::random_device{}()};

    std::vector<T> out;
    out.reserve(size);
    std::ranges::copy(std::ranges::iota_view(min_value, max_value + 1),
                      std::back_inserter(out));
    std::ranges::shuffle(out, engine);
    out.resize(size);
    return out;
}

template <std::ranges::forward_range T>
std::ranges::range_value_t<T> random_element(T && r) {
    const auto size = static_cast<std::size_t>(std::ranges::distance(r));
    std::uniform_int_distribution<std::size_t> distr{0ul, size - 1};
    std::mt19937 engine{std::random_device{}()};
    auto it = r.begin();
    auto pos = distr(engine);
    std::advance(it, pos);
    return *it;
}

#endif  // RANDOM_RANGES_HELPER_HPP