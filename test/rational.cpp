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

// ############ regression: make_rational return type deduction ################

// `-a` / `-b` are integer promoted, so the sign-flipping branch deduced
// rational<int, int> while the other two deduced rational<short, short>:
// "inconsistent deduction for auto return type", i.e. make_rational did not
// compile at all for any type narrower than int.
GTEST_TEST(rational, make_rational_with_narrow_integer_types) {
    const short a = 3, b = -4;
    auto r = make_rational(a, b);
    static_assert(std::same_as<decltype(r), rational<short, short>>);
    ASSERT_EQ(r.num(), -3);
    ASSERT_EQ(r.den(), 4);

    const std::int8_t c = 5, d = -10;
    auto r8 = make_rational(c, d);
    static_assert(std::same_as<decltype(r8), rational<std::int8_t, std::int8_t>>);
    ASSERT_EQ(r8.num(), -5);
    ASSERT_EQ(r8.den(), 10);
}

GTEST_TEST(rational, make_rational_normalizes_sign_and_keeps_operand_types) {
    auto neg = make_rational(3, -4);
    static_assert(std::same_as<decltype(neg), rational<int, int>>);
    ASSERT_EQ(neg.num(), -3);
    ASSERT_EQ(neg.den(), 4);

    auto pos = make_rational(6, 4);
    ASSERT_EQ(pos.num(), 6);
    ASSERT_EQ(pos.den(), 4);

    // a zero denominator is kept as the 1/0 sentinel rather than normalized
    auto inf = make_rational(5, 0);
    ASSERT_EQ(inf.num(), 1);
    ASSERT_EQ(inf.den(), 0);

    // mixed operand types keep their own numerator / denominator types
    auto mixed = make_rational(3, 4L);
    static_assert(std::same_as<decltype(mixed), rational<int, long>>);
    ASSERT_EQ(mixed.num(), 3);
    ASSERT_EQ(mixed.den(), 4L);
}
