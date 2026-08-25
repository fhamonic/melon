#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace melon {
// bounded_value, const_value, rational and integer live in melon::numeric
// rather than at namespace scope: `melon::integer` and `melon::const_value`
// are generic enough to collide with a user's own names, and `integer` is not
// even one -- it is a rational with a unit denominator.
namespace numeric {

namespace detail {

template <typename... Ts>
struct type_list {};

template <template <typename> typename Pred, typename TList>
struct first_matching;

template <template <typename> typename Pred, typename First, typename... Rest>
struct first_matching<Pred, type_list<First, Rest...>> {
    using type = std::conditional_t<
        Pred<First>::value, First,
        typename first_matching<Pred, type_list<Rest...>>::type>;
};

template <template <typename> typename Pred>
struct first_matching<Pred, type_list<>> {
    using type = void;
};

template <template <typename> typename Pred, typename TList>
using first_matching_t = first_matching<Pred, TList>::type;

template <typename From, typename To>
concept narrowing_conversion = !requires(From f) { To{f}; };

template <typename From, typename To>
concept non_narrowing = !narrowing_conversion<From, To>;

template <typename A, typename B>
    requires(non_narrowing<A, B> || non_narrowing<B, A>)
using common_number_type = std::conditional_t<non_narrowing<A, B>, B, A>;

// Every comparison of a bound or a value against one of another
// specialization goes through these, never through the plain operator: the
// usual arithmetic conversions turn a negative signed operand into a huge
// unsigned one, so `-10 >= 20u` holds. That silently folds the comparison
// operators' bound pruning to the wrong answer and lets the conversion
// constraints admit int [-10,10] into unsigned [0,20].
template <typename A, typename B>
[[nodiscard]] constexpr bool cmp_less(const A & a, const B & b) {
    if constexpr(std::integral<A> && std::integral<B>)
        return std::cmp_less(a, b);
    else
        return a < b;
}
template <typename A, typename B>
[[nodiscard]] constexpr bool cmp_less_equal(const A & a, const B & b) {
    if constexpr(std::integral<A> && std::integral<B>)
        return std::cmp_less_equal(a, b);
    else
        return a <= b;
}
template <typename A, typename B>
[[nodiscard]] constexpr bool cmp_greater(const A & a, const B & b) {
    if constexpr(std::integral<A> && std::integral<B>)
        return std::cmp_greater(a, b);
    else
        return a > b;
}
template <typename A, typename B>
[[nodiscard]] constexpr bool cmp_greater_equal(const A & a, const B & b) {
    if constexpr(std::integral<A> && std::integral<B>)
        return std::cmp_greater_equal(a, b);
    else
        return a >= b;
}
template <typename A, typename B>
[[nodiscard]] constexpr bool cmp_equal(const A & a, const B & b) {
    if constexpr(std::integral<A> && std::integral<B>)
        return std::cmp_equal(a, b);
    else
        return a == b;
}
template <typename A, typename B>
[[nodiscard]] constexpr bool cmp_not_equal(const A & a, const B & b) {
    if constexpr(std::integral<A> && std::integral<B>)
        return std::cmp_not_equal(a, b);
    else
        return a != b;
}

}  // namespace detail

// clang-format off
template <typename Traits, typename T>
concept promotion_strategy = requires(const T & v) {
    { Traits::plus_overflows(v, v) } -> std::convertible_to<bool>;
    { Traits::substract_overflows(v, v) } -> std::convertible_to<bool>;
    { Traits::multiply_overflows(v, v) } -> std::convertible_to<bool>;
};
// clang-format on

// Integer-only: the predicates below reason with numeric_limits<T>::min() as
// the most negative value, which a floating-point T does not satisfy -- it
// would report overflow-free where the result is not representable. Give a
// non-integral value type its own promotion_strategy.
struct default_promotion_strategy {
    using type_hierarchy = detail::type_list<int8_t, int16_t, int32_t, int64_t>;

    template <typename A, typename B>
    static constexpr bool plus_overflows(const A & a, const B & b) {
        using T = detail::common_number_type<A, B>;
        return (b > 0 && a > std::numeric_limits<T>::max() - b) ||
               (b < 0 && a < std::numeric_limits<T>::min() - b);
    }

    template <typename A, typename B>
    static constexpr bool substract_overflows(const A & a, const B & b) {
        using T = detail::common_number_type<A, B>;
        return (b > 0 && a < std::numeric_limits<T>::min() + b) ||
               (b < 0 && a > std::numeric_limits<T>::max() + b);
    }

    template <typename A, typename B>
    static constexpr bool multiply_overflows(const A & a, const B & b) {
        using T = detail::common_number_type<A, B>;
        if(a == 0 || b == 0) return false;
        if(a > 0 && b > 0) return a > std::numeric_limits<T>::max() / b;
        if(a < 0 && b < 0) return a < std::numeric_limits<T>::max() / b;
        if(a > 0 && b < 0) return b < std::numeric_limits<T>::min() / a;
        return a < std::numeric_limits<T>::min() / b;
    }

    template <typename A, A A_MIN, A A_MAX, typename B, B B_MIN, B B_MAX>
    struct predicates {
        template <typename T>
        struct can_hold_plus {
            static constexpr bool value = false;
        };
        template <typename T>
            requires(detail::non_narrowing<A, T> && detail::non_narrowing<B, T>)
        struct can_hold_plus<T> {
            static constexpr bool value =
                !(plus_overflows(T{A_MIN}, T{B_MIN}) ||
                  plus_overflows(T{A_MAX}, T{B_MAX}));
        };
        template <typename T>
        struct can_hold_substract {
            static constexpr bool value = false;
        };
        template <typename T>
            requires(detail::non_narrowing<A, T> && detail::non_narrowing<B, T>)
        struct can_hold_substract<T> {
            static constexpr bool value =
                !(substract_overflows(T{A_MIN}, T{B_MAX}) ||
                  substract_overflows(T{A_MAX}, T{B_MIN}));
        };
        template <typename T>
        struct can_hold_multiply {
            static constexpr bool value = false;
        };
        template <typename T>
            requires(detail::non_narrowing<A, T> && detail::non_narrowing<B, T>)
        struct can_hold_multiply<T> {
            static constexpr bool value =
                !(multiply_overflows(T{A_MIN}, T{B_MIN}) ||
                  multiply_overflows(T{A_MIN}, T{B_MAX}) ||
                  multiply_overflows(T{A_MAX}, T{B_MIN}) ||
                  multiply_overflows(T{A_MAX}, T{B_MAX}));
        };
    };
};

template <typename T, T Min, T Max, promotion_strategy<T> PS>
class bounded_value;

struct bounded_value_base_base {};

template <typename CRTP, typename T, T Min, T Max, typename PS>
class bounded_value_base : public bounded_value_base_base {
private:
    // Whether negating the *range* stays inside T. Min > numeric_limits<T>::
    // min() also gives Max >= Min > min(), so one check covers both bounds.
    static constexpr bool negation_is_representable =
        std::signed_integral<T> && (Min != std::numeric_limits<T>::min());

public:
    using value_type = T;
    using promotion_strategy_t = PS;

    // static_cast, not reinterpret_cast: this is a base-to-derived downcast
    // along a real inheritance path. reinterpret_cast does not adjust the
    // pointer, so it is undefined behaviour and lands on the wrong subobject
    // the moment CRTP gains a second base.
    constexpr value_type value() const {
        return static_cast<const CRTP &>(*this).value();
    }
    constexpr operator T() const { return value(); }

    // Constrained rather than fixed, because there is nothing to fix: the
    // negation of an unsigned range is not an unsigned range. For unsigned T,
    // -Max wraps to a huge value and `bounded_value<T, -Max, -Min, PS>` names
    // bounds that bracket nothing; for signed T with Min ==
    // numeric_limits<T>::min(), -Min is not representable and the template-id
    // is ill-formed, which breaks the class at the point of use rather than
    // here.
    [[nodiscard]] constexpr auto operator-() const
        requires negation_is_representable
    {
        return bounded_value<T, -Max, -Min, PS>(-value());
    }

    // Deleted, not merely absent. Leaving it out is not enough: `operator T()`
    // above is implicit, so `-x` would quietly convert to T and use the
    // built-in negation -- wrapping around for unsigned T, and throwing away
    // the bound tracking that is the whole point of the class. Deleting it
    // makes `-x` name this overload and diagnose. Negating anyway means
    // spelling the intent: widen with .bound<...>(), subtract from a
    // zero-valued bounded_value, or cast to T.
    constexpr void operator-() const
        requires(!negation_is_representable)
    = delete;
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
    [[nodiscard]] constexpr auto operator<(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) const {
        if constexpr(detail::cmp_less(Max, OMIN)) return true;
        if constexpr(detail::cmp_greater_equal(Min, OMAX)) return false;
        return detail::cmp_less(value(), o.value());
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
    [[nodiscard]] constexpr auto operator<=(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) const {
        if constexpr(detail::cmp_less_equal(Max, OMIN)) return true;
        if constexpr(detail::cmp_greater(Min, OMAX)) return false;
        return detail::cmp_less_equal(value(), o.value());
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
    [[nodiscard]] constexpr auto operator>(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) const {
        if constexpr(detail::cmp_less_equal(Max, OMIN)) return false;
        if constexpr(detail::cmp_greater(Min, OMAX)) return true;
        return detail::cmp_greater(value(), o.value());
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
    [[nodiscard]] constexpr auto operator>=(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) const {
        if constexpr(detail::cmp_less(Max, OMIN)) return false;
        if constexpr(detail::cmp_greater_equal(Min, OMAX)) return true;
        return detail::cmp_greater_equal(value(), o.value());
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
    [[nodiscard]] constexpr auto operator==(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) const {
        if constexpr(detail::cmp_less(Max, OMIN)) return false;
        if constexpr(detail::cmp_greater(Min, OMAX)) return false;
        return detail::cmp_equal(value(), o.value());
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
    [[nodiscard]] constexpr auto operator!=(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) const {
        if constexpr(detail::cmp_less(Max, OMIN)) return true;
        if constexpr(detail::cmp_greater(Min, OMAX)) return true;
        return detail::cmp_not_equal(value(), o.value());
    }
};

// The three operators below are the overflow contract: the result type is the
// first type in the strategy's hierarchy that provably holds the *bounds* of
// the result, so the operation itself never overflows -- and fails to compile
// when no such type exists. This holds only while every operand's value is
// within its declared bounds, which the constructors merely assert.
template <typename T1, T1 MIN1, T1 MAX1, typename PS1, typename T2, T2 MIN2,
          T2 MAX2, typename PS2>
    requires std::same_as<PS1, PS2>
[[nodiscard]] constexpr auto operator+(
    const bounded_value<T1, MIN1, MAX1, PS1> & a,
    const bounded_value<T2, MIN2, MAX2, PS2> & b) {
    using return_value_type = detail::first_matching_t<
        PS1::template predicates<T1, MIN1, MAX1, T2, MIN2,
                                 MAX2>::template can_hold_plus,
        typename PS1::type_hierarchy>;
    // Without the static_assert the failure is a raw "compound literal of
    // non-object type ... {aka 'void'}".
    static_assert(!std::is_void_v<return_value_type>,
                  "melon: no type in the promotion strategy's hierarchy can "
                  "hold this sum's bounds; tighten the operands' bounds with "
                  ".bound<Min, Max>() or use a wider hierarchy.");
    return bounded_value<
        return_value_type, return_value_type{MIN1} + return_value_type{MIN2},
        return_value_type{MAX1} + return_value_type{MAX2}, PS1>(
        return_value_type{a.value()} + return_value_type{b.value()});
}
template <typename T1, T1 MIN1, T1 MAX1, typename PS1, typename T2, T2 MIN2,
          T2 MAX2, typename PS2>
    requires std::same_as<PS1, PS2>
[[nodiscard]] constexpr auto operator-(
    const bounded_value<T1, MIN1, MAX1, PS1> & a,
    const bounded_value<T2, MIN2, MAX2, PS2> & b) {
    using return_value_type = detail::first_matching_t<
        PS1::template predicates<T1, MIN1, MAX1, T2, MIN2,
                                 MAX2>::template can_hold_substract,
        typename PS1::type_hierarchy>;
    static_assert(!std::is_void_v<return_value_type>,
                  "melon: no type in the promotion strategy's hierarchy can "
                  "hold this difference's bounds; tighten the operands' "
                  "bounds with .bound<Min, Max>() or use a wider hierarchy.");
    return bounded_value<
        return_value_type, return_value_type{MIN1} - return_value_type{MAX2},
        return_value_type{MAX1} - return_value_type{MIN2}, PS1>(
        return_value_type{a.value()} - return_value_type{b.value()});
}
template <typename T1, T1 MIN1, T1 MAX1, typename PS1, typename T2, T2 MIN2,
          T2 MAX2, typename PS2>
    requires std::same_as<PS1, PS2>
[[nodiscard]] constexpr auto operator*(
    const bounded_value<T1, MIN1, MAX1, PS1> & a,
    const bounded_value<T2, MIN2, MAX2, PS2> & b) {
    using return_value_type = detail::first_matching_t<
        PS1::template predicates<T1, MIN1, MAX1, T2, MIN2,
                                 MAX2>::template can_hold_multiply,
        typename PS1::type_hierarchy>;
    static_assert(!std::is_void_v<return_value_type>,
                  "melon: no type in the promotion strategy's hierarchy can "
                  "hold this product's bounds; tighten the operands' bounds "
                  "with .bound<Min, Max>() or use a wider hierarchy.");
    return bounded_value<
        return_value_type,
        std::min({return_value_type{MIN1} * return_value_type{MIN2},
                  return_value_type{MIN1} * return_value_type{MAX2},
                  return_value_type{MAX1} * return_value_type{MIN2},
                  return_value_type{MAX1} * return_value_type{MAX2}}),
        std::max({return_value_type{MIN1} * return_value_type{MIN2},
                  return_value_type{MIN1} * return_value_type{MAX2},
                  return_value_type{MAX1} * return_value_type{MIN2},
                  return_value_type{MAX1} * return_value_type{MAX2}}),
        PS1>(return_value_type{a.value()} * return_value_type{b.value()});
}

template <typename T, T Min = std::numeric_limits<T>::min(),
          T Max = std::numeric_limits<T>::max(),
          promotion_strategy<T> PS = default_promotion_strategy>
class bounded_value : public bounded_value_base<bounded_value<T, Min, Max, PS>,
                                                T, Min, Max, PS> {
public:
    using value_type = T;
    using promotion_strategy_t = PS;

private:
    value_type _value;

public:
    constexpr bounded_value() : _value(Min) {}

    // Precondition, asserted in debug builds only: Min <= v <= Max. Every
    // result type the operators compute is derived from the declared bounds,
    // so a value outside them makes those bounds a lie and the arithmetic can
    // then overflow the type chosen to hold it.
    template <std::convertible_to<T> V>
        requires(!std::derived_from<V, bounded_value_base_base>)
    constexpr bounded_value(V v) : _value(static_cast<T>(v)) {
        if constexpr(std::integral<T> && std::integral<V>) {
            assert(std::cmp_less_equal(Min, v) && std::cmp_less_equal(v, Max));
        } else {
            assert(Min <= v && v <= Max);
        }
    }

    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(detail::cmp_greater_equal(OMIN, Min) &&
                 detail::cmp_less_equal(OMAX, Max))
    constexpr bounded_value(bounded_value<OT, OMIN, OMAX, OPS> && o)
        : _value(std::move(o.value())) {}
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(detail::cmp_greater_equal(OMIN, Min) &&
                 detail::cmp_less_equal(OMAX, Max))
    constexpr bounded_value(const bounded_value<OT, OMIN, OMAX, OPS> & o)
        : _value(o.value()) {}

    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(detail::cmp_greater_equal(OMIN, Min) &&
                 detail::cmp_less_equal(OMAX, Max))
    constexpr bounded_value & operator=(
        bounded_value<OT, OMIN, OMAX, OPS> && o) {
        _value = std::move(o.value());
        return *this;
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(detail::cmp_greater_equal(OMIN, Min) &&
                 detail::cmp_less_equal(OMAX, Max))
    constexpr bounded_value & operator=(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) {
        _value = o.value();
        return *this;
    }

    static constexpr value_type min() { return Min; }
    static constexpr value_type max() { return Max; }
    constexpr value_type value() const { return _value; }

    template <T NMIN, T NMAX>
    constexpr auto bound() const {
        // The converting constructor admits widening only, so tightening --
        // the direction the overflow static_asserts recommend this member
        // for -- routes through the value instead, trading the compile-time
        // bound proof for that constructor's runtime range assert.
        if constexpr(NMIN <= Min && Max <= NMAX)
            return bounded_value<T, NMIN, NMAX, PS>(*this);
        else
            return bounded_value<T, NMIN, NMAX, PS>(value());
    }
};

template <typename T, T V, typename PS>
class bounded_value<T, V, V, PS>
    : public bounded_value_base<bounded_value<T, V, V, PS>, T, V, V, PS> {
public:
    using value_type = T;
    using promotion_strategy_t = PS;

    // Deliberately discards v: this specialization holds a single compile-time
    // value, so there is nothing to store, and the assert is all that stands
    // between a caller and a silently ignored argument. It must stay implicit
    // and one-argument -- that is what lets a const_value<int, 1> denominator
    // be initialized `_den(1)`, as rational's constructors do.
    //
    // A constrained template rather than a plain `T` parameter, mirroring the
    // primary template: taking `T` lets any *other* bounded_value in through
    // the implicit operator T(), so a bound violation the general template
    // rejects at compile time -- `bounded_value<int, 1, 2>` from
    // `bounded_value<int, 0, 10>` is ill-formed -- reaches a runtime assert
    // here instead.
    template <std::convertible_to<T> V_>
        requires(!std::derived_from<std::remove_cvref_t<V_>,
                                    bounded_value_base_base>)
    constexpr bounded_value(V_ v) {
        assert(detail::cmp_equal(v, V));
        (void)v;
    }
    constexpr bounded_value() = default;
    constexpr bounded_value(const bounded_value &) = default;
    constexpr bounded_value(bounded_value &&) = default;
    // Explicit, not implicit: the defaulted move constructor above deletes
    // the implicit copy assignment, which made `integer<T>` fail
    // std::copyable while its copy *construction* still worked.
    constexpr bounded_value & operator=(const bounded_value &) = default;
    constexpr bounded_value & operator=(bounded_value &&) = default;

    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(detail::cmp_equal(OMIN, V) && detail::cmp_equal(OMAX, V))
    constexpr bounded_value(const bounded_value<OT, OMIN, OMAX, OPS> &) {}

    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(detail::cmp_equal(OMIN, V) && detail::cmp_equal(OMAX, V))
    constexpr bounded_value & operator=(
        const bounded_value<OT, OMIN, OMAX, OPS> &) {
        return *this;
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(detail::cmp_equal(OMIN, V) && detail::cmp_equal(OMAX, V))
    constexpr bounded_value & operator=(bounded_value<OT, OMIN, OMAX, OPS> &&) {
        return *this;
    }

    static constexpr value_type min() { return V; }
    static constexpr value_type max() { return V; }
    static constexpr value_type value() { return V; }
};

template <typename T, T V, typename PS = default_promotion_strategy>
using const_value = bounded_value<T, V, V, PS>;

}  // namespace numeric
}  // namespace melon
