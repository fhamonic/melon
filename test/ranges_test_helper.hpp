#ifndef RANGES_TEST_HELPER_HPP
#define RANGES_TEST_HELPER_HPP

#include <initializer_list>
#include <ranges>

#include <range/v3/view/zip.hpp>

template <typename R1, typename R2>
void ASSERT_EQ_RANGES(R1 && r1, R2 && r2) {
    ASSERT_EQ(std::ranges::distance(r1), std::ranges::distance(r2));
    for(const auto & [e1, e2] : ranges::views::zip(r1, r2)) {
        ASSERT_EQ(e1, e2);
    }
    // auto it1 = r1.begin();
    // auto it2 = r2.begin();
    // while(it1 != r1.end()) {
    //     ASSERT_EQ(*it1, *it2);
    //     ++it1;
    //     ++it2;
    // }
}

template <typename R, typename T>
requires std::convertible_to<T, std::ranges::range_value_t<R>>
void ASSERT_EQ_RANGES(R && r, std::initializer_list<T> l) {
    ASSERT_EQ(std::ranges::distance(r), l.size());
    auto it1 = r.begin();
    auto it2 = l.begin();
    while(it1 != r.end()) {
        ASSERT_EQ(*it1, static_cast<std::ranges::range_value_t<R>>(*it2));
        ++it1;
        ++it2;
    }
}

#endif  // RANGES_TEST_HELPER_HPP