#ifndef RATIONAL_HPP
#define RATIONAL_HPP

#include <cassert>
#include <concepts>
#include <limits>
#include <numeric>

#include "melon/detail/specialization_of.hpp"
#include "melon/utility/bounded_value.hpp"

namespace fhamonic {
namespace melon {

template <typename NumT, typename DenT = const_value<int, 1>>
struct rational {
private:
    [[no_unique_address]] mutable NumT _num;
    [[no_unique_address]] mutable DenT _den;

public:
    constexpr rational()
        requires std::default_initializable<NumT> &&
                     std::constructible_from<DenT, int>
        : _num(), _den(1) {}
    constexpr rational(NumT n)
        requires std::constructible_from<DenT, int>
        : _num(n), _den(1) {}
    constexpr rational(NumT n, DenT d) : _num(n), _den(d) {
        assert(_den >= 0);
    }

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

    constexpr void normalize() const {
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

template <typename T1, typename T2>
constexpr auto make_rational(T1 a, T2 b) {
    if(b == 0) {
        return rational(T1{1}, b);
    }
    if(b < 0) {
        return rational(-a, -b);
    }
    return rational(a, b);
}

#define DEFINE_RATIONAL_OPERATOR(op, expr)                              \
    template <typename N1, typename D1, typename N2, typename D2>       \
    constexpr auto operator op(const rational<N1, D1> & r1,             \
                               const rational<N2, D2> & r2) {           \
        return expr;                                                    \
    }                                                                   \
    template <typename T, typename N, typename D>                       \
        requires(!__detail::__specialization_of<T, rational>)           \
    constexpr auto operator op(const T & a, const rational<N, D> & r) { \
        return rational(a) op r;                                        \
    }                                                                   \
    template <typename T, typename N, typename D>                       \
        requires(!__detail::__specialization_of<T, rational>)           \
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
}  // namespace fhamonic

#endif  // RATIONAL_HPP