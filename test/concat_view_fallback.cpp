#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/detail/concat_view.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(concat_view, empty) {
    auto v =
        detail::views::concat(std::views::empty<int>, std::views::empty<int>);
    ASSERT_TRUE(EMPTY(v));
}

GTEST_TEST(concat_view, single) {
    auto v1 = detail::views::concat(std::ranges::views::single(1),
                                    std::views::empty<int>);
    ASSERT_TRUE(EQ_RANGES(v1, {1}));
    auto v2 = detail::views::concat(std::views::empty<int>,
                                    std::ranges::views::single(2));
    ASSERT_TRUE(EQ_RANGES(v2, {2}));
}

GTEST_TEST(concat_view, vectors) {
    std::vector<int> r1 = {1,3,4};
    std::vector<int> r2 = {2, 6};
    auto v = detail::views::concat(r1,r2);
    ASSERT_TRUE(EQ_RANGES(v, {1,3,4,2,6}));
}
