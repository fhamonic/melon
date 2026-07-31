#undef NDEBUG
#include <gtest/gtest.h>

#include <compare>
#include <concepts>
#include <cstdint>
#include <string>

#include "melon/numeric/bounded_value.hpp"
#include "melon/numeric/rational.hpp"

using namespace melon;
using namespace melon::numeric;

////////////////////////////////////////////////////////////////////////////////
// a rational is built from a numerator and a denominator, defaulting to 0/1
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// arithmetic is exact, for rational and mixed scalar operands alike
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// comparisons see through unnormalized representations
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// normalize() reduces to lowest terms, and requires a mutable object
// (regression: it used to be const over mutable members)
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(rational, normalize) {
    rational r(6, 4);
    r.normalize();
    ASSERT_EQ(r.num(), 3);
    ASSERT_EQ(r.den(), 2);
}

// _num/_den were `mutable` and normalize() was `const`, so a const rational
// could be rewritten under its owner and two threads reading a shared const
// rational raced -- num()/den() hand out const references into that state.
namespace {
template <typename R>
concept normalizable = requires(R & r) { r.normalize(); };
}  // namespace

static_assert(normalizable<rational<int, int>>);
static_assert(!normalizable<const rational<int, int>>);

GTEST_TEST(rational, normalize_requires_a_mutable_object) {
    rational r(6, 4);
    r.normalize();
    ASSERT_EQ(r.num(), 3);
    ASSERT_EQ(r.den(), 2);

    // reading a const rational leaves it untouched
    const rational cr(6, 4);
    ASSERT_EQ(cr.num(), 6);
    ASSERT_EQ(cr.den(), 4);
    ASSERT_TRUE(cr == rational(3, 2));  // comparison normalizes nothing
    ASSERT_EQ(cr.num(), 6);
    ASSERT_EQ(cr.den(), 4);
}

////////////////////////////////////////////////////////////////////////////////
// make_rational moves the sign to the numerator and preserves the operand
// types (regression: its auto return type deduction used to be inconsistent)
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(rational, make_rational_normalizes_signs) {
    auto r = make_rational(1, -2);
    ASSERT_EQ(r.num(), -1);
    ASSERT_EQ(r.den(), 2);

    auto z = make_rational(5, 0);
    ASSERT_EQ(z.num(), 1);
    ASSERT_EQ(z.den(), 0);
}

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
    static_assert(
        std::same_as<decltype(r8), rational<std::int8_t, std::int8_t>>);
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

////////////////////////////////////////////////////////////////////////////////
// rational composes with bounded_value components
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(rational, bounded_value_components) {
    auto a = bounded_value<int8_t, -10, 21>(5);
    auto b = bounded_value<int8_t, -1, 15>(15);

    auto r = rational(a, b);
    ASSERT_EQ(r.num().value(), 5);
    ASSERT_EQ(r.den().value(), 15);
    ASSERT_TRUE(r == rational(1, 3));
    ASSERT_TRUE(r + r == rational(2, 3));
}

////////////////////////////////////////////////////////////////////////////////
// regression (2.8): rational hygiene -- no leaked macro, const-correct
// conversion operator
////////////////////////////////////////////////////////////////////////////////

// DEFINE_RATIONAL_OPERATOR was never #undef'd, so an unprefixed function-like
// macro leaked out of rational.hpp into every TU that included it -- and
// therefore into every TU including all.hpp.
#ifdef DEFINE_RATIONAL_OPERATOR
#error "DEFINE_RATIONAL_OPERATOR escaped rational.hpp"
#endif

// The converting operator was non-const. Named explicitly rather than through
// static_cast, which would pick the (non-explicit, already const-correct)
// converting *constructor* and prove nothing.
template <typename R, typename Target>
concept has_const_conversion_operator =
    requires(const R & r) { r.operator Target(); };

static_assert(
    has_const_conversion_operator<rational<int, int>, rational<long, long>>);

GTEST_TEST(rational, converting_a_const_rational) {
    const rational<int, int> r(3, 4);
    const auto wide = r.operator rational<long, long>();
    ASSERT_EQ(wide.num(), 3L);
    ASSERT_EQ(wide.den(), 4L);
}

////////////////////////////////////////////////////////////////////////////////
// std alignment: == / <=> replace the six relationals, compound assignments
// exist, and the mixed-operand operators are constrained to number-like types
////////////////////////////////////////////////////////////////////////////////

// weak_ordering, not strong: unnormalized representations make 1/2 and 2/4
// equivalent, not equal.
static_assert(std::same_as<decltype(rational(1, 2) <=> rational(1, 3)),
                           std::weak_ordering>);

// the scalar comparisons work in both argument orders through the compiler's
// rewritten candidates -- there is no (T, rational) comparison overload
GTEST_TEST(rational, scalar_comparisons_rewrite_both_orders) {
    ASSERT_TRUE(rational(1, 2) < 1);
    ASSERT_TRUE(1 > rational(1, 2));
    ASSERT_TRUE(rational(4, 2) == 2);
    ASSERT_TRUE(2 == rational(4, 2));
    ASSERT_TRUE(2 != rational(1, 2));
}

GTEST_TEST(rational, compound_assignments) {
    rational c(1, 2);
    c += rational(1, 3);
    ASSERT_TRUE(c == rational(5, 6));
    c -= rational(1, 3);
    ASSERT_TRUE(c == rational(1, 2));
    c *= rational(2, 3);
    ASSERT_TRUE(c == rational(1, 3));
    c /= rational(1, 3);
    ASSERT_TRUE(c == rational(1, 1));
    c += 1;
    ASSERT_TRUE(c == 2);
    c *= 3;
    ASSERT_TRUE(c == 6);
    c /= 2;
    ASSERT_TRUE(c == 3);
    c -= 2;
    ASSERT_TRUE(c == 1);
}

// /= keeps the class invariants like binary /: the sign moves to the
// numerator and a zero divisor collapses to the 1/0 sentinel
GTEST_TEST(rational, compound_division_normalizes_signs) {
    rational d(1, 2);
    d /= -2;
    ASSERT_TRUE(d == rational(-1, 4));
    ASSERT_TRUE(d.den() > 0);
    rational z(1, 2);
    z /= rational(0, 1);
    ASSERT_TRUE(z.den() == 0);
}

// integer's const_value denominator survives the compound forms whose result
// keeps den == 1
GTEST_TEST(rational, integer_compound_assignments) {
    integer<int> i(3);
    i += 2;
    ASSERT_TRUE(i == 5);
    i *= integer<int>(2);
    ASSERT_TRUE(i == 10);
}

// the mixed-operand operators are constrained to number-like T: arithmetic
// types and bounded_value, nothing that merely makes rational<T> a legal type
template <typename R, typename S>
concept has_mixed_arithmetic = requires(const R & r, const S & s) {
    r + s;
    s * r;
};
template <typename R, typename S>
concept has_mixed_comparison = requires(const R & r, const S & s) {
    r == s;
    r < s;
};
static_assert(has_mixed_arithmetic<rational<int, int>, int>);
static_assert(has_mixed_comparison<rational<int, int>, int>);
static_assert(!has_mixed_arithmetic<rational<int, int>, std::string>);
static_assert(!has_mixed_comparison<rational<int, int>, std::string>);

// heterogeneous specializations keep working through the rational-rational
// friends, not the scalar path
static_assert(has_mixed_arithmetic<rational<int, int>, rational<short, short>>);
GTEST_TEST(rational, heterogeneous_specializations) {
    short sn = 1, sd = 2;
    rational<short, short> s(sn, sd);
    ASSERT_TRUE(s + rational(1, 2) == 1);
    ASSERT_TRUE(rational(1, 2) + s == 1);
    ASSERT_TRUE(s == rational(1, 2));
}
