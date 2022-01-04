#ifndef RANGES_TEST_HELPER_HPP
#define RANGES_TEST_HELPER_HPP

#include <ranges>

#include <range/v3/view/zip.hpp>

template <typename R, typename T>
void range_equals(R && range, std::initializer_list<T> l) {
    EXPECT_EQ(std::ranges::size(range), std::ranges::size(l));
    for(const auto & [e1, e2] : ranges::views::zip(range, l)) {
        EXPECT_EQ(e1, e2);
    }
}

#endif //RANGES_TEST_HELPER_HPP