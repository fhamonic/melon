#undef NDEBUG
#include <gtest/gtest.h>

#include <concepts>

#include "melon/utility/bounded_value.hpp"

using namespace fhamonic::melon;

GTEST_TEST(bounded_value, add_test) {
    auto a = bounded_value<int8_t, -10, 21>(5);
    auto b = bounded_value<int8_t, -1, 15>(15);
    {
        auto r = a + b;
        static_assert(std::is_same_v<decltype(r)::value_type, int8_t>);
        static_assert(r.min() == -11);
        static_assert(r.max() == 36);
        ASSERT_EQ(r.value(), 20);
    }
    auto c = bounded_value<int8_t, -121, 99>(64);
    {
        auto r = a + c;
        static_assert(std::is_same_v<decltype(r)::value_type, int16_t>);
        static_assert(r.min() == -131);
        static_assert(r.max() == 120);
        ASSERT_EQ(r.value(), 69);
    }
    auto d = bounded_value<int16_t, -851, 32760>(17500);
    {
        auto r = a + d;
        static_assert(std::is_same_v<decltype(r)::value_type, int32_t>);
        static_assert(r.min() == -861);
        static_assert(r.max() == 32781);
        ASSERT_EQ(r.value(), 17505);
    }
}
GTEST_TEST(bounded_value, mult_8x8_test) {
    auto a = bounded_value<int8_t, -10, 21>(5);
    auto b = bounded_value<int8_t, -1, 4>(2);
    {
        auto r = a * b;
        static_assert(std::is_same_v<decltype(r)::value_type, int8_t>);
        static_assert(r.min() == -40);
        static_assert(r.max() == 84);
        ASSERT_EQ(r.value(), 10);
    }
    auto c = bounded_value<int8_t, -1, 15>(15);
    {
        auto r = a * c;
        static_assert(std::is_same_v<decltype(r)::value_type, int16_t>);
        static_assert(r.min() == -150);
        static_assert(r.max() == 315);
        ASSERT_EQ(r.value(), 75);
    }
    {
        auto r = c * a;
        static_assert(std::is_same_v<decltype(r)::value_type, int16_t>);
        static_assert(r.min() == -150);
        static_assert(r.max() == 315);
        ASSERT_EQ(r.value(), 75);
    }
}
GTEST_TEST(bounded_value, mult_8x16_test) {
    auto a = bounded_value<int8_t, -20, 21>(5);
    auto b = bounded_value<int16_t, -314, 265>(15);
    {
        auto r = a * b;
        static_assert(std::is_same_v<decltype(r)::value_type, int16_t>);
        static_assert(r.min() == -314 * 21);
        static_assert(r.max() == 20 * 314);
        ASSERT_EQ(r.value(), 75);
    }
    {
        auto r = b * a;
        static_assert(std::is_same_v<decltype(r)::value_type, int16_t>);
        static_assert(r.min() == -314 * 21);
        static_assert(r.max() == 20 * 314);
        ASSERT_EQ(r.value(), 75);
    }
    auto c = bounded_value<int16_t, -1, 16452>(15);
    {
        auto r = a * c;
        static_assert(std::is_same_v<decltype(r)::value_type, int32_t>);
        static_assert(r.min() == -20 * 16452);
        static_assert(r.max() == 21 * 16452);
        ASSERT_EQ(r.value(), 75);
    }
    {
        auto r = c * a;
        static_assert(std::is_same_v<decltype(r)::value_type, int32_t>);
        static_assert(r.min() == -20 * 16452);
        static_assert(r.max() == 21 * 16452);
        ASSERT_EQ(r.value(), 75);
    }
}
GTEST_TEST(bounded_value, mult_8x32_test) {
    auto a = bounded_value<int8_t, -20, 21>(5);
    auto b = bounded_value<int32_t, -314, 265>(15);
    {
        auto r = a * b;
        static_assert(std::is_same_v<decltype(r)::value_type, int32_t>);
        static_assert(r.min() == -314 * 21);
        static_assert(r.max() == 20 * 314);
        ASSERT_EQ(r.value(), 75);
    }
    {
        auto r = b * a;
        static_assert(std::is_same_v<decltype(r)::value_type, int32_t>);
        static_assert(r.min() == -314 * 21);
        static_assert(r.max() == 20 * 314);
        ASSERT_EQ(r.value(), 75);
    }
    auto c = bounded_value<int32_t, -500000000, 254>(15);
    {
        auto r = a * c;
        static_assert(std::is_same_v<decltype(r)::value_type, int64_t>);
        static_assert(r.min() == -10500000000l);
        static_assert(r.max() == 10000000000l);
        ASSERT_EQ(r.value(), 75);
    }
    {
        auto r = c * a;
        static_assert(std::is_same_v<decltype(r)::value_type, int64_t>);
        static_assert(r.min() == -10500000000l);
        static_assert(r.max() == 10000000000l);
        ASSERT_EQ(r.value(), 75);
    }
}

GTEST_TEST(bounded_value, conversions_test) {
    using A = bounded_value<int8_t, -78, 10>;
    using B = bounded_value<int8_t, -78, 21>;

    static_assert(std::convertible_to<A, B>);
    static_assert(!std::convertible_to<B, A>);

    auto a = A(5);
    B b = a;

    static_assert(std::constructible_from<A::value_type, A>);
    static_assert(std::convertible_to<std::pair<A, A>, std::pair<B, B>>);
}