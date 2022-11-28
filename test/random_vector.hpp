#ifndef RANDOM_VECTOR_HPP
#define RANDOM_VECTOR_HPP

#include <algorithm>
#include <random>
#include <ranges>
#include <vector>

std::vector<int> random_vector(std::size_t size, int min_value = 0,
                               int max_value = 100) {
    std::uniform_int_distribution<int> distr{min_value, max_value};
    std::mt19937 engine{std::random_device{}()};

    std::vector<int> out(size);
    std::ranges::generate(out, [&]() { return distr(engine); });
    return out;
}

std::vector<int> random_vector_all_diff(std::size_t size, int min_value = 0,
                                        int max_value = 100) {
    assert(size <= static_cast<std::size_t>(max_value - min_value));
    std::mt19937 engine{std::random_device{}()};

    std::vector<int> out;
    std::ranges::copy(std::ranges::iota_view(min_value, max_value),
                      std::back_inserter(out));
    std::ranges::shuffle(out, engine);
    out.resize(size);
    return out;
}

#endif  // RANDOM_VECTOR_HPP