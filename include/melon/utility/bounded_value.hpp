#ifndef MELON_BOUNDED_VALUE_HPP
#define MELON_BOUNDED_VALUE_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <limits>
#include <type_traits>

#include "melon/detail/specialization_of.hpp"

namespace fhamonic {
namespace melon {

template <typename T1, typename T2>
using larger_type_t = std::conditional_t<sizeof(T1) < sizeof(T2), T2, T1>;

// clang-format off
template <typename _Traits, typename T>
concept promotion_strategy = requires(const T & v) {
    { _Traits::plus_overflows(v, v) } -> std::convertible_to<bool>;
    { _Traits::substract_overflows(v, v) } -> std::convertible_to<bool>;
    { _Traits::multiply_overflows(v, v) } -> std::convertible_to<bool>;
    { v } -> std::convertible_to<typename _Traits::promoted_type<T>>;
};
// clang-format on

// Default promotion strategy for integer types
struct default_promotion_strategy {
    template <typename T>
    using promoted_type = std::conditional_t<
        std::same_as<T, int8_t>, int16_t,
        std::conditional_t<
            std::same_as<T, int16_t>, int32_t,
            std::conditional_t<std::same_as<T, int32_t>, int64_t,
                               std::conditional_t<std::same_as<T, int64_t>,
                                                  __int128_t, void>>>>;

    template <typename A, typename B>
    static constexpr bool plus_overflows(const A & a, const B & b) {
        using T = larger_type_t<A, B>;
        return (b > 0 && a > std::numeric_limits<T>::max() - b) ||
               (b < 0 && a < std::numeric_limits<T>::min() - b);
    }

    template <typename A, typename B>
    static constexpr bool substract_overflows(const A & a, const B & b) {
        using T = larger_type_t<A, B>;
        return (b > 0 && a < std::numeric_limits<T>::min() + b) ||
               (b < 0 && a > std::numeric_limits<T>::max() + b);
    }

    template <typename A, typename B>
    static constexpr bool multiply_overflows(const A & a, const B & b) {
        using T = larger_type_t<A, B>;
        if(a == 0 || b == 0) return false;
        if(a > 0 && b > 0) return a > std::numeric_limits<T>::max() / b;
        if(a < 0 && b < 0) return a < std::numeric_limits<T>::max() / b;
        if(a > 0 && b < 0) return b < std::numeric_limits<T>::min() / a;
        return a < std::numeric_limits<T>::min() / b;
    }
};

template <typename T, T _MIN, T _MAX, promotion_strategy<T> _PS>
class bounded_value;

struct bounded_value_base_base {};

template <typename _CRTP, typename T, T _MIN, T _MAX, typename _PS>
class bounded_value_base : public bounded_value_base_base {
public:
    using value_type = T;
    using promotion_strategy_t = _PS;

    constexpr value_type value() const {
        return reinterpret_cast<const _CRTP &>(*this).value();
    }
    constexpr operator T() const { return value(); }

    constexpr auto operator-() const {
        return bounded_value<T, -_MAX, -_MIN, _PS>(-value());
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
    constexpr auto operator<(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) const {
        if constexpr(_MAX < OMIN) return true;
        if constexpr(_MIN >= OMAX) return false;
        return value() < o.value();
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
    constexpr auto operator<=(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) const {
        if constexpr(_MAX <= OMIN) return true;
        if constexpr(_MIN > OMAX) return false;
        return value() <= o.value();
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
    constexpr auto operator>(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) const {
        if constexpr(_MAX <= OMIN) return false;
        if constexpr(_MIN > OMAX) return true;
        return value() > o.value();
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
    constexpr auto operator>=(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) const {
        if constexpr(_MAX < OMIN) return false;
        if constexpr(_MIN >= OMAX) return true;
        return value() >= o.value();
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
    constexpr auto operator==(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) const {
        if constexpr(_MAX < OMIN) return false;
        if constexpr(_MIN > OMAX) return false;
        return value() == o.value();
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
    constexpr auto operator!=(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) const {
        if constexpr(_MAX < OMIN) return true;
        if constexpr(_MIN > OMAX) return true;
        return value() != o.value();
    }
};
// OPERATOR +
template <typename T1, T1 MIN1, T1 MAX1, typename PS1, typename T2, T2 MIN2,
          T2 MAX2, typename PS2>
    requires std::same_as<PS1, PS2>
constexpr auto operator+(const bounded_value<T1, MIN1, MAX1, PS1> & a,
                         const bounded_value<T2, MIN2, MAX2, PS2> & b) {
    using return_value_type =
        std::conditional_t<PS1::plus_overflows(MIN1, MIN2) ||
                               PS1::plus_overflows(MAX1, MAX2),
                           typename PS1::promoted_type<larger_type_t<T1, T2>>,
                           larger_type_t<T1, T2>>;
    return bounded_value<
        return_value_type, return_value_type{MIN1} + return_value_type{MIN2},
        return_value_type{MAX1} + return_value_type{MAX2}, PS1>(
        return_value_type{a.value()} + return_value_type{b.value()});
}
// OPERATOR -
template <typename T1, T1 MIN1, T1 MAX1, typename PS1, typename T2, T2 MIN2,
          T2 MAX2, typename PS2>
    requires std::same_as<PS1, PS2>
constexpr auto operator-(const bounded_value<T1, MIN1, MAX1, PS1> & a,
                         const bounded_value<T2, MIN2, MAX2, PS2> & b) {
    using return_value_type =
        std::conditional_t<PS1::substract_overflows(MIN1, MAX2) ||
                               PS1::substract_overflows(MAX1, MIN2),
                           typename PS1::promoted_type<larger_type_t<T1, T2>>,
                           larger_type_t<T1, T2>>;
    return bounded_value<
        return_value_type, return_value_type{MIN1} - return_value_type{MAX2},
        return_value_type{MAX1} - return_value_type{MIN2}, PS1>(
        return_value_type{a.value()} - return_value_type{b.value()});
}
// OPERATOR *
template <typename T1, T1 MIN1, T1 MAX1, typename PS1, typename T2, T2 MIN2,
          T2 MAX2, typename PS2>
    requires std::same_as<PS1, PS2>
constexpr auto operator*(const bounded_value<T1, MIN1, MAX1, PS1> & a,
                         const bounded_value<T2, MIN2, MAX2, PS2> & b) {
    using return_value_type =
        std::conditional_t<PS1::multiply_overflows(MIN1, MIN2) ||
                               PS1::multiply_overflows(MIN1, MAX2) ||
                               PS1::multiply_overflows(MAX1, MIN2) ||
                               PS1::multiply_overflows(MAX1, MAX2),
                           typename PS1::promoted_type<larger_type_t<T1, T2>>,
                           larger_type_t<T1, T2>>;
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

template <typename T, T _MIN = std::numeric_limits<T>::min(),
          T _MAX = std::numeric_limits<T>::max(),
          promotion_strategy<T> _PS = default_promotion_strategy>
class bounded_value
    : public bounded_value_base<bounded_value<T, _MIN, _MAX, _PS>, T, _MIN,
                                _MAX, _PS> {
public:
    using value_type = T;
    using promotion_strategy_t = _PS;

private:
    value_type _value;

public:
    constexpr bounded_value() : _value(_MIN) {}

    template <std::convertible_to<T> V>
        requires(!std::derived_from<V, bounded_value_base_base>)
    constexpr bounded_value(V v) : _value(static_cast<T>(v)) {
        assert(_MIN <= v && v <= _MAX);
    }

    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(OMIN >= _MIN && OMAX <= _MAX)
    constexpr bounded_value(bounded_value<OT, OMIN, OMAX, OPS> && o)
        : _value(std::move(o.value())) {}
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(OMIN >= _MIN && OMAX <= _MAX)
    constexpr bounded_value(const bounded_value<OT, OMIN, OMAX, OPS> & o)
        : _value(o.value()) {}

    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(OMIN >= _MIN && OMAX <= _MAX)
    constexpr bounded_value & operator=(
        bounded_value<OT, OMIN, OMAX, OPS> && o) {
        _value = std::move(o.value());
        return *this;
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(OMIN >= _MIN && OMAX <= _MAX)
    constexpr bounded_value & operator=(
        const bounded_value<OT, OMIN, OMAX, OPS> & o) {
        _value = o.value();
        return *this;
    }

    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(OMIN <= _MIN && OMAX >= _MAX)
    explicit constexpr operator bounded_value<OT, OMIN, OMAX, OPS>() {
        return bounded_value<OT, OMIN, OMAX, OPS>(_value);
    }

    static constexpr value_type min() { return _MIN; }
    static constexpr value_type max() { return _MAX; }
    constexpr value_type value() const { return _value; }

    template <T NMIN, T NMAX>
    constexpr auto bound() const {
        return bounded_value<T, NMIN, NMAX, _PS>(*this);
    }
};

template <typename T, T V, typename _PS>
class bounded_value<T, V, V, _PS>
    : public bounded_value_base<bounded_value<T, V, V, _PS>, T, V, V, _PS> {
public:
    using value_type = T;
    using promotion_strategy_t = _PS;

    constexpr bounded_value(T v) {};
    constexpr bounded_value() = default;
    constexpr bounded_value(const bounded_value &) = default;
    constexpr bounded_value(bounded_value &&) = default;

    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(OMIN == V && OMAX == V)
    constexpr bounded_value & operator=(const bounded_value<OT, V, V, OPS> &) {
        return *this;
    }
    template <typename OT, OT OMIN, OT OMAX, typename OPS>
        requires(OMIN == V && OMAX == V)
    constexpr bounded_value & operator=(bounded_value<OT, OMIN, OMAX, OPS> &&) {
        return *this;
    }

    static constexpr value_type min() { return V; }
    static constexpr value_type max() { return V; }
    static constexpr value_type value() { return V; }
};

template <typename T, T V, typename PS = default_promotion_strategy>
using const_value = bounded_value<T, V, V, PS>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_BOUNDED_VALUE_HPP