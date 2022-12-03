#include <gtest/gtest.h>

#include <vector>

#include "melon/concepts/range_of.hpp"
#include "melon/utils/intrusive_input_range.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(intrusive_range, test) {
    std::vector<int> values = {1, 2, 6, 3, 7};

    auto r = intrusive_input_range(
        0ul, [&values](const std::size_t i) -> int & { return values[i]; },
        [](const std::size_t i) -> std::size_t { return i + 1; },
        [n = values.size()](const std::size_t i) -> std::size_t {
            return i < n;
        });

    auto it = r.begin();
    auto end = r.end();

    using R = decltype(r);
    using I = decltype(it);
    using S = decltype(end);

    static_assert(requires(I i) {
        typename std::iter_difference_t<I>;
        { ++i } -> std::same_as<I &>;
        i++;
    });
    static_assert(std::movable<I>);
    static_assert(std::weakly_incrementable<I>);
    static_assert(std::input_or_output_iterator<I>);

    static_assert(requires(const I in) {
        typename std::iter_value_t<I>;
        typename std::iter_reference_t<I>;
        typename std::iter_rvalue_reference_t<I>;
        { *in } -> std::same_as<std::iter_reference_t<I>>;
        {
            std::ranges::iter_move(in)
            } -> std::same_as<std::iter_rvalue_reference_t<I>>;
    });
    static_assert(std::indirectly_readable<I>);
    static_assert(std::input_iterator<I>);
    static_assert(std::semiregular<S>);
    static_assert(std::sentinel_for<S, I>);
    static_assert(std::ranges::range<R>);

    ASSERT_FALSE(it == end);
    ASSERT_EQ(*it, 1);
    ++it;
    ASSERT_FALSE(it == end);
    ASSERT_EQ(*it, 2);
    ++it;
    ASSERT_FALSE(it == end);
    ASSERT_EQ(*it, 6);
    ++it;
    ASSERT_FALSE(it == end);
    ASSERT_EQ(*it, 3);
    ++it;
    ASSERT_FALSE(it == end);
    ASSERT_EQ(*it, 7);
    ++it;
    ASSERT_TRUE(it == end);
}
