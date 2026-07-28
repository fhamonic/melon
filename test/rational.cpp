#undef NDEBUG
#include <gtest/gtest.h>

#include <concepts>
#include <cstdint>

#include "melon/utility/bounded_value.hpp"
#include "melon/utility/rational.hpp"

using namespace melon;

GTEST_TEST(rational, constructors) {
    rational<int> zero;
    ASSERT_EQ(zero.num(), 0);
    ASSERT_EQ(zero.den(), 1);

    rational<int> three(3);
    ASSERT_EQ(three.num(), 3);
    ASSERT_EQ(three.den(), 1);

    rational r(3, 4);
    ASSERT_EQ(r.num(), 3);
    ASSERT_EQ(r.den(), 4);
}

GTEST_TEST(rational, arithmetic) {
    rational a(1, 2);
    rational b(1, 3);

    ASSERT_TRUE(a + b == rational(5, 6));
    ASSERT_TRUE(a - b == rational(1, 6));
    ASSERT_TRUE(a * b == rational(1, 6));
    ASSERT_TRUE(a / b == rational(3, 2));

    ASSERT_TRUE(-a == rational(-1, 2));
    ASSERT_TRUE(+a == a);
}

GTEST_TEST(rational, scalar_mixed_arithmetic) {
    rational half(1, 2);

    ASSERT_TRUE(2 + half == rational(5, 2));
    ASSERT_TRUE(half + 2 == rational(5, 2));
    ASSERT_TRUE(2 * half == rational(1, 1));
    ASSERT_TRUE(1 - half == half);
}

GTEST_TEST(rational, comparisons) {
    ASSERT_TRUE(rational(1, 2) < rational(2, 3));
    ASSERT_TRUE(rational(2, 3) > rational(1, 2));
    ASSERT_TRUE(rational(1, 2) <= rational(2, 4));
    ASSERT_TRUE(rational(1, 2) >= rational(2, 4));
    ASSERT_TRUE(rational(1, 2) == rational(2, 4));
    ASSERT_TRUE(rational(1, 2) != rational(1, 3));
}

GTEST_TEST(rational, equivalence_of_unnormalized_fractions) {
    ASSERT_TRUE(rational(6, 4) == rational(3, 2));
    ASSERT_TRUE(rational(0, 4) == rational(0, 7));
}

GTEST_TEST(rational, normalize) {
    rational r(6, 4);
    r.normalize();
    ASSERT_EQ(r.num(), 3);
    ASSERT_EQ(r.den(), 2);
}

GTEST_TEST(rational, make_rational_normalizes_signs) {
    auto r = make_rational(1, -2);
    ASSERT_EQ(r.num(), -1);
    ASSERT_EQ(r.den(), 2);

    auto z = make_rational(5, 0);
    ASSERT_EQ(z.num(), 1);
    ASSERT_EQ(z.den(), 0);
}

GTEST_TEST(rational, bounded_value_components) {
    auto a = bounded_value<int8_t, -10, 21>(5);
    auto b = bounded_value<int8_t, -1, 15>(15);

    auto r = rational(a, b);
    ASSERT_EQ(r.num().value(), 5);
    ASSERT_EQ(r.den().value(), 15);
    ASSERT_TRUE(r == rational(1, 3));
    ASSERT_TRUE(r + r == rational(2, 3));
}
