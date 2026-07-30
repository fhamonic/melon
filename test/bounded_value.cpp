#undef NDEBUG
#include <gtest/gtest.h>

#include <concepts>
#include <cstdint>
#include <limits>

#include "melon/numeric/bounded_value.hpp"

using namespace melon;
using namespace melon::numeric;

////////////////////////////////////////////////////////////////////////////////
// addition widens the value type just enough to hold the exact bound interval
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// multiplication bounds are the extreme cross products, identical in either
// operand order
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// conversion is allowed exactly when the bounds fit inside the target's
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// regression (2.8): value() reaches the CRTP base through a static_cast, not
// a reinterpret_cast
////////////////////////////////////////////////////////////////////////////////

// value() reached the derived class through reinterpret_cast, which does not
// perform a derived-to-base downcast: it is UB, and would have been wrong
// outright had bounded_value ever gained a second base. static_cast now.
GTEST_TEST(bounded_value, value_through_the_crtp_base) {
    const auto b = bounded_value<int8_t, -10, 21>(7);
    using base = bounded_value_base<bounded_value<int8_t, -10, 21>, int8_t, -10,
                                    21, default_promotion_strategy>;
    const base & as_base = b;
    ASSERT_EQ(as_base.value(), 7);
    ASSERT_EQ(static_cast<int8_t>(as_base), 7);
}

////////////////////////////////////////////////////////////////////////////////
// const_value stays implicitly constructible, but asserts on a mismatched
// argument
////////////////////////////////////////////////////////////////////////////////

// const_value's single-argument constructor silently discards its argument --
// it has to stay implicit, since it is what lets rational spell its
// const_value<int, 1> denominator `_den(1)`. An assert is the only thing
// between a caller and an ignored argument.
static_assert(std::is_constructible_v<const_value<int, 1>, int>);
static_assert(const_value<int, 1>(1).value() == 1);

GTEST_TEST(bounded_value, const_value_rejects_a_mismatched_argument) {
    // aliased: the comma in the template-id would split the macro's arguments
    using one = const_value<int, 1>;
    one ok(1);
    ASSERT_EQ(ok.value(), 1);
    EXPECT_DEATH((void)one(2), "");
}

////////////////////////////////////////////////////////////////////////////////
// regression (2.8): unary operator- is deleted where negation would produce a
// nonsense range
////////////////////////////////////////////////////////////////////////////////

// It returned `bounded_value<T, -Max, -Min, PS>` for every T. For unsigned T
// that wraps: negating a bounded_value<unsigned, 0, 10> asked for
// bounded_value<unsigned, 4294967286, 0>, whose Min exceeds its Max -- bounds
// that bracket nothing. For signed T with Min == numeric_limits<T>::min(),
// -Min is not representable and the template-id is ill-formed, so the class
// simply failed to compile at the point of use. Constrained now: neither case
// is silently wrong, both name a deleted operator-.
//
// Deleted rather than absent, and that matters: bounded_value has an implicit
// operator T(), so merely constraining the member away would let `-b` convert
// to T and use the built-in negation -- wrapping around for unsigned T, with
// no diagnostic and no bounds. This concept would still have said `true`.
template <typename B>
concept negatable = requires(const B & b) { -b; };

static_assert(negatable<bounded_value<int8_t, -10, 21>>);
static_assert(negatable<bounded_value<int, -5, 5>>);
static_assert(negatable<const_value<int, 1>>);

static_assert(!negatable<bounded_value<unsigned int, 0, 10>>);
static_assert(!negatable<bounded_value<std::uint8_t, 1, 200>>);
static_assert(
    !negatable<bounded_value<int8_t, std::numeric_limits<int8_t>::min(), 21>>);
static_assert(!negatable<bounded_value<int, std::numeric_limits<int>::min(),
                                       std::numeric_limits<int>::max()>>);
// one below the edge is fine again
static_assert(
    negatable<
        bounded_value<int8_t, std::numeric_limits<int8_t>::min() + 1, 21>>);

GTEST_TEST(bounded_value, negation_flips_the_bounds) {
    const auto a = bounded_value<int8_t, -10, 21>(5);
    const auto n = -a;
    static_assert(n.min() == -21);
    static_assert(n.max() == 10);
    ASSERT_EQ(n.value(), -5);

    // binary operator- is untouched, and is the route for unsigned operands:
    // it goes through the promotion strategy and gets honest bounds
    const auto u = bounded_value<unsigned int, 0, 10>(4);
    const auto zero = bounded_value<unsigned int, 0, 0>(0);
    const auto d = zero - u;
    ASSERT_EQ(d.value(), -4);
    static_assert(d.min() <= -10);
}
