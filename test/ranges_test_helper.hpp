#ifndef RANGES_TEST_HELPER_HPP
#define RANGES_TEST_HELPER_HPP

#include <ranges>

#include <range/v3/view/zip.hpp>

template <typename R1, typename R2>
void AssertRangesAreEqual(R1 && r1, R2 && r2) {
    ASSERT_EQ(std::ranges::distance(r1), std::ranges::distance(r2));
    for(const auto & [e1, e2] : ranges::views::zip(r1, r2)) {
        ASSERT_EQ(e1, e2);
    }
}

#endif //RANGES_TEST_HELPER_HPP