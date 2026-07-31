#pragma once

#include <cassert>
#include <compare>
#include <concepts>
#include <limits>
#include <numeric>
#include <type_traits>

#include "melon/numeric/bounded_value.hpp"

namespace melon {
namespace numeric {

template <typename NumT, typename DenT>
struct rational;

// The return type is spelled out rather than deduced: `-a` / `-b` are integer
// promoted, so the sign-flipping branch would otherwise deduce a different
// specialization than the other two (rational<int, int> vs rational<short,
// short>) and the function would not compile at all for narrow types.
// A zero denominator is *deliberately* collapsed to the 1/0 sentinel, which
// compares greater than every finite rational -- that is what makes
// `operator/` total, and division by a zero rational is how it arises. It is
// not an error path: the numerator is discarded, so make_rational(5, 0) and
// make_rational(-5, 0) are the same value. Callers that need to tell the
// sentinel apart must test den() == 0 themselves.
// Defined before the class: operator/ is a hidden friend whose body names
// make_rational through ordinary lookup at the definition context.
template <typename T1, typename T2>
[[nodiscard]] constexpr rational<T1, T2> make_rational(T1 a, T2 b) {
    if(b == 0) {
        return rational<T1, T2>(T1{1}, b);
    }
    if(b < 0) {
        return rational<T1, T2>(static_cast<T1>(-a), static_cast<T2>(-b));
    }
    return rational<T1, T2>(a, b);
}

// "Number-like" for the mixed-operand operators: a built-in arithmetic type
// or a bounded_value. Deliberately a closed trait-based set rather than a
// requires-expression probing `n * a`: probing runs overload resolution, and
// ADL can drag these very scalar operators back in (any type whose associated
// classes include a rational -- a std::map<tuple<rational, ...>, _>::iterator
// in bentley_ottmann's tests -- makes the constraint re-enter itself with the
// same arguments, a hard error). Traits do no lookup, so they cannot recurse
// -- and they still keep std::string and iterators out, which construction
// alone would not (rational<std::string> is a constructible *type*).
template <typename T>
concept rational_scalar_operand =
    std::is_arithmetic_v<T> || std::derived_from<T, bounded_value_base_base>;

template <typename NumT, typename DenT = const_value<int, 1>>
struct rational {
private:
    // Not mutable: normalize() rewrites both, so with `mutable` + a const
    // normalize() a const rational could be changed under the caller and two
    // threads reading a shared const rational raced. num()/den() hand out
    // const references straight into these.
    [[no_unique_address]] NumT _num;
    [[no_unique_address]] DenT _den;

    template <typename T>
    static constexpr bool scalar_operand = rational_scalar_operand<T>;

public:
    constexpr rational()
        requires std::default_initializable<NumT> &&
                     std::constructible_from<DenT, int>
        : _num(), _den(1) {}
    constexpr rational(NumT n)
        requires std::constructible_from<DenT, int>
        : _num(n), _den(1) {}
    constexpr rational(NumT n, DenT d) : _num(n), _den(d) { assert(_den >= 0); }

    template <typename N, typename D>
        requires(std::constructible_from<NumT, N> &&
                 std::constructible_from<DenT, D>)
    constexpr rational(const rational<N, D> & o)
        : _num(o.num()), _den(o.den()) {}

    template <typename N, typename D>
        requires(std::assignable_from<NumT, N> && std::assignable_from<DenT, D>)
    constexpr rational & operator=(const rational<N, D> & o) {
        _num = o.num();
        _den = o.den();
        return *this;
    }

    template <typename ON, typename OD>
        requires std::constructible_from<rational<ON, OD>, NumT, DenT>
    explicit constexpr operator rational<ON, OD>() const {
        return rational<ON, OD>(_num, _den);
    }

    [[nodiscard]] constexpr const NumT & num() const { return _num; }
    [[nodiscard]] constexpr const DenT & den() const { return _den; }

    constexpr void normalize() {
        const auto & g = std::gcd(_num, _den);
        _num /= g;
        _den /= g;
        if constexpr(std::numeric_limits<NumT>::is_signed) {
            if(_den < 0) {
                _num = -_num;
                _den = -_den;
            }
        }
    }

    [[nodiscard]] constexpr auto operator+() const {
        return rational(_num, _den);
    }
    [[nodiscard]] constexpr auto operator-() const {
        return rational(-_num, _den);
    }

    // Hidden friends, the ADL-hygiene shape: the old namespace-scope operator
    // templates were candidates for *every* unqualified `a op b` in melon,
    // rational or not. The first parameter anchors on this specialization, so
    // for mixed-specialization operands overload resolution prefers the left
    // operand's friend (exact on arg1) and the candidate sets never tie.
    template <typename N2, typename D2>
    [[nodiscard]] friend constexpr auto operator+(const rational & r1,
                                                  const rational<N2, D2> & r2) {
        return numeric::rational(r1.num() * r2.den() + r2.num() * r1.den(),
                                 r1.den() * r2.den());
    }
    template <typename N2, typename D2>
    [[nodiscard]] friend constexpr auto operator-(const rational & r1,
                                                  const rational<N2, D2> & r2) {
        return numeric::rational(r1.num() * r2.den() - r2.num() * r1.den(),
                                 r1.den() * r2.den());
    }
    template <typename N2, typename D2>
    [[nodiscard]] friend constexpr auto operator*(const rational & r1,
                                                  const rational<N2, D2> & r2) {
        return numeric::rational(r1.num() * r2.num(), r1.den() * r2.den());
    }
    template <typename N2, typename D2>
    [[nodiscard]] friend constexpr auto operator/(const rational & r1,
                                                  const rational<N2, D2> & r2) {
        return make_rational(r1.num() * r2.den(), r1.den() * r2.num());
    }

    template <typename T>
        requires scalar_operand<T>
    [[nodiscard]] friend constexpr auto operator+(const rational & r,
                                                  const T & a) {
        return r + numeric::rational(a);
    }
    template <typename T>
        requires scalar_operand<T>
    [[nodiscard]] friend constexpr auto operator+(const T & a,
                                                  const rational & r) {
        return numeric::rational(a) + r;
    }
    template <typename T>
        requires scalar_operand<T>
    [[nodiscard]] friend constexpr auto operator-(const rational & r,
                                                  const T & a) {
        return r - numeric::rational(a);
    }
    template <typename T>
        requires scalar_operand<T>
    [[nodiscard]] friend constexpr auto operator-(const T & a,
                                                  const rational & r) {
        return numeric::rational(a) - r;
    }
    template <typename T>
        requires scalar_operand<T>
    [[nodiscard]] friend constexpr auto operator*(const rational & r,
                                                  const T & a) {
        return r * numeric::rational(a);
    }
    template <typename T>
        requires scalar_operand<T>
    [[nodiscard]] friend constexpr auto operator*(const T & a,
                                                  const rational & r) {
        return numeric::rational(a) * r;
    }
    template <typename T>
        requires scalar_operand<T>
    [[nodiscard]] friend constexpr auto operator/(const rational & r,
                                                  const T & a) {
        return r / numeric::rational(a);
    }
    template <typename T>
        requires scalar_operand<T>
    [[nodiscard]] friend constexpr auto operator/(const T & a,
                                                  const rational & r) {
        return numeric::rational(a) / r;
    }

    // == and <=> replace the six macro-generated relationals; the compiler
    // rewrites !=, <, <=, > and >= from them, including the argument-swapped
    // scalar forms. Cross-multiplication keeps order because den() >= 0 is a
    // class invariant (asserted in the constructor, preserved by normalize()
    // and make_rational). weak_ordering, not strong: unnormalized
    // representations make 1/2 and 2/4 equivalent, not equal.
    template <typename N2, typename D2>
    [[nodiscard]] friend constexpr bool operator==(
        const rational & r1, const rational<N2, D2> & r2) {
        return r1.num() * r2.den() == r2.num() * r1.den();
    }
    template <typename N2, typename D2>
    [[nodiscard]] friend constexpr std::weak_ordering operator<=>(
        const rational & r1, const rational<N2, D2> & r2) {
        return (r1.num() * r2.den()) <=> (r2.num() * r1.den());
    }
    template <typename T>
        requires scalar_operand<T>
    [[nodiscard]] friend constexpr bool operator==(const rational & r,
                                                   const T & a) {
        return r == numeric::rational(a);
    }
    template <typename T>
        requires scalar_operand<T>
    [[nodiscard]] friend constexpr std::weak_ordering operator<=>(
        const rational & r, const T & a) {
        return r <=> numeric::rational(a);
    }

    // Compound assignments write the members directly instead of routing
    // through operator=: the result of `a op b` is a *promoted*
    // specialization, and the heterogeneous operator= is constrained on
    // assignable_from, which the promoted components need not satisfy.
    template <typename N2, typename D2>
        requires requires(NumT & n, DenT & d, const N2 & on, const D2 & od) {
            n = n * od + on * d;
            d = d * od;
        }
    constexpr rational & operator+=(const rational<N2, D2> & o) {
        _num = _num * o.den() + o.num() * _den;
        _den = _den * o.den();
        return *this;
    }
    template <typename N2, typename D2>
        requires requires(NumT & n, DenT & d, const N2 & on, const D2 & od) {
            n = n * od - on * d;
            d = d * od;
        }
    constexpr rational & operator-=(const rational<N2, D2> & o) {
        _num = _num * o.den() - o.num() * _den;
        _den = _den * o.den();
        return *this;
    }
    template <typename N2, typename D2>
        requires requires(NumT & n, DenT & d, const N2 & on, const D2 & od) {
            n = n * on;
            d = d * od;
        }
    constexpr rational & operator*=(const rational<N2, D2> & o) {
        _num = _num * o.num();
        _den = _den * o.den();
        return *this;
    }
    // Goes through make_rational so dividing by a negative or zero rational
    // keeps the class invariants: the sign moves to the numerator and a zero
    // divisor collapses to the 1/0 sentinel, exactly like binary operator/.
    template <typename N2, typename D2>
        requires requires(NumT & n, DenT & d, const N2 & on, const D2 & od) {
            n = make_rational(n * od, d * on).num();
            d = make_rational(n * od, d * on).den();
        }
    constexpr rational & operator/=(const rational<N2, D2> & o) {
        const auto r = make_rational(_num * o.den(), _den * o.num());
        _num = r.num();
        _den = r.den();
        return *this;
    }

    template <typename T>
        requires scalar_operand<T> && requires(rational & r, const T & a) {
            r += numeric::rational(a);
        }
    constexpr rational & operator+=(const T & a) {
        return *this += numeric::rational(a);
    }
    template <typename T>
        requires scalar_operand<T> && requires(rational & r, const T & a) {
            r -= numeric::rational(a);
        }
    constexpr rational & operator-=(const T & a) {
        return *this -= numeric::rational(a);
    }
    template <typename T>
        requires scalar_operand<T> && requires(rational & r, const T & a) {
            r *= numeric::rational(a);
        }
    constexpr rational & operator*=(const T & a) {
        return *this *= numeric::rational(a);
    }
    template <typename T>
        requires scalar_operand<T> && requires(rational & r, const T & a) {
            r /= numeric::rational(a);
        }
    constexpr rational & operator/=(const T & a) {
        return *this /= numeric::rational(a);
    }
};

template <typename T = int>
using integer = rational<T, const_value<int, 1>>;

}  // namespace numeric
}  // namespace melon
