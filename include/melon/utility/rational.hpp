#pragma once

#include <cassert>
#include <concepts>
#include <limits>
#include <numeric>

#include "melon/detail/specialization_of.hpp"
#include "melon/utility/bounded_value.hpp"

namespace melon {

template <typename NumT, typename DenT = const_value<int, 1>>
struct rational {
private:
    // Not mutable: normalize() rewrites both, so with `mutable` + a const
    // normalize() a const rational could be changed under the caller and two
    // threads reading a shared const rational raced. num()/den() hand out
    // const references straight into these.
    [[no_unique_address]] NumT _num;
    [[no_unique_address]] DenT _den;

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
    explicit constexpr operator rational<ON, OD>() {
        return rational<ON, OD>(_num, _den);
    }

    constexpr const NumT & num() const { return _num; }
    constexpr const DenT & den() const { return _den; }

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

    constexpr auto operator+() const { return rational(_num, _den); }
    constexpr auto operator-() const { return rational(-_num, _den); }
};

// The return type is spelled out rather than deduced: `-a` / `-b` are integer
// promoted, so the sign-flipping branch would otherwise deduce a different
// specialization than the other two (rational<int, int> vs rational<short,
// short>) and the function would not compile at all for narrow types.
template <typename T1, typename T2>
constexpr rational<T1, T2> make_rational(T1 a, T2 b) {
    if(b == 0) {
        return rational<T1, T2>(T1{1}, b);
    }
    if(b < 0) {
        return rational<T1, T2>(static_cast<T1>(-a), static_cast<T2>(-b));
    }
    return rational<T1, T2>(a, b);
}

#define DEFINE_RATIONAL_OPERATOR(op, expr)                              \
    template <typename N1, typename D1, typename N2, typename D2>       \
    constexpr auto operator op(const rational<N1, D1> & r1,             \
                               const rational<N2, D2> & r2) {           \
        return expr;                                                    \
    }                                                                   \
    template <typename T, typename N, typename D>                       \
        requires(!detail::specialization_of<T, rational>)               \
    constexpr auto operator op(const T & a, const rational<N, D> & r) { \
        return rational(a) op r;                                        \
    }                                                                   \
    template <typename T, typename N, typename D>                       \
        requires(!detail::specialization_of<T, rational>)               \
    constexpr auto operator op(const rational<N, D> & r, const T & a) { \
        return r op rational(a);                                        \
    }

DEFINE_RATIONAL_OPERATOR(+, rational(r1.num() * r2.den() + r2.num() * r1.den(),
                                     r1.den() * r2.den()))
DEFINE_RATIONAL_OPERATOR(-, rational(r1.num() * r2.den() - r2.num() * r1.den(),
                                     r1.den() * r2.den()))
DEFINE_RATIONAL_OPERATOR(*, rational(r1.num() * r2.num(), r1.den() * r2.den()))
DEFINE_RATIONAL_OPERATOR(/, make_rational(r1.num() * r2.den(),
                                          r1.den() * r2.num()))
DEFINE_RATIONAL_OPERATOR(<, (r1.num() * r2.den()) < (r2.num() * r1.den()))
DEFINE_RATIONAL_OPERATOR(<=, (r1.num() * r2.den()) <= (r2.num() * r1.den()))
DEFINE_RATIONAL_OPERATOR(>, (r1.num() * r2.den()) > (r2.num() * r1.den()))
DEFINE_RATIONAL_OPERATOR(>=, (r1.num() * r2.den()) >= (r2.num() * r1.den()))
DEFINE_RATIONAL_OPERATOR(==, (r1.num() * r2.den()) == (r2.num() * r1.den()))
DEFINE_RATIONAL_OPERATOR(!=, (r1.num() * r2.den()) != (r2.num() * r1.den()))

template <typename T = int>
using integer = rational<T, const_value<int, 1>>;

}  // namespace melon
