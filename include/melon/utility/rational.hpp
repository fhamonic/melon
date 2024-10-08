#ifndef RATIONAL_HPP
#define RATIONAL_HPP

#include <concepts>
#include <limits>

#include "melon/detail/specialization_of.hpp"

namespace fhamonic {
namespace melon {

template <typename NumT, typename DenT = NumT>
struct rational {
    mutable NumT num;
    mutable DenT den;

    constexpr rational(NumT n = NumT{0}, DenT d = DenT{1}) : num(n), den(d) {
        // if constexpr(std::numeric_limits<NumT>::is_signed) {
        if(den < 0) {
            num = -num;
            den = -den;
        }
        // }
    }

    constexpr void normalize() const {
        const auto & g = std::gcd(num, den);
        num /= g;
        den /= g;
        if constexpr(std::numeric_limits<NumT>::is_signed) {
            if(den < 0) {
                num = -num;
                den = -den;
            }
        }
    }

    rational operator+(const rational & other) const {
        return rational(num * other.den + other.num * den, den * other.den);
    }
    rational operator-(const rational & other) const {
        return rational(num * other.den - other.num * den, den * other.den);
    }
    rational operator*(const rational & other) const {
        return rational(num * other.num, den * other.den);
    }
    rational operator/(const rational & other) const {
        return rational(num * other.den, den * other.num);
    }

    rational operator+(const std::integral auto & other) const {
        return rational(num + other * den, den);
    }

    rational operator-(const std::integral auto & other) const {
        return rational(num - other * den, den);
    }

    rational operator*(const std::integral auto & other) const {
        return rational(num * other, den);
    }

    rational operator/(const std::integral auto & other) const {
        return rational(num, den * other);
    }

    rational operator+() const { return rational(num, den); }
    rational operator-() const { return rational(-num, den); }

    bool operator==(const rational & other) const {
        return (num * other.den) == (other.num * den);
    }
    // template <typename ONumT, typename ODenT>
    // std::strong_ordering operator<=>(
    //     const rational<ONumT, ODenT> & other) const {
    //     return (num * other.den) <=> (other.num * den);
    // }
    template <typename ONumT, typename ODenT>
    constexpr bool operator<(const rational<ONumT, ODenT> & other) const {
        return (num * other.den) < (other.num * den);
    }
    template <typename ONumT, typename ODenT>
    constexpr bool operator<=(const rational<ONumT, ODenT> & other) const {
        return (num * other.den) <= (other.num * den);
    }
    template <typename ONumT, typename ODenT>
    constexpr bool operator>=(const rational<ONumT, ODenT> & other) const {
        return (num * other.den) >= (other.num * den);
    }
    template <typename ONumT, typename ODenT>
    constexpr bool operator>(const rational<ONumT, ODenT> & other) const {
        return (num * other.den) > (other.num * den);
    }
    // template <typename O> requires(!__detail::__specialization_of<O,
    // rational>) std::strong_ordering operator<=>(const O & other) const {
    //     return num <=> (other * den);
    // }
    template <typename O>
        requires(!__detail::__specialization_of<O, rational>)
    constexpr bool operator<(const O & other) const {
        return num < (other * den);
    }
    template <typename O>
        requires(!__detail::__specialization_of<O, rational>)
    constexpr bool operator<=(const O & other) const {
        return num <= (other * den);
    }
    template <typename O>
        requires(!__detail::__specialization_of<O, rational>)
    constexpr bool operator>=(const O & other) const {
        return num >= (other * den);
    }
    template <typename O>
        requires(!__detail::__specialization_of<O, rational>)
    constexpr bool operator>(const O & other) const {
        return num > (other * den);
    }
};

template <typename N, typename D, typename O>
    requires(!__detail::__specialization_of<O, rational>)
constexpr bool operator<(const O & other, const rational<N, D> & r) {
    return other * r.den < r.num;
}
template <typename N, typename D, typename O>
    requires(!__detail::__specialization_of<O, rational>)
constexpr bool operator<=(const O & other, const rational<N, D> & r) {
    return other * r.den <= r.num;
}
template <typename N, typename D, typename O>
    requires(!__detail::__specialization_of<O, rational>)
constexpr bool operator>(const O & other, const rational<N, D> & r) {
    return other * r.den > r.num;
}
template <typename N, typename D, typename O>
    requires(!__detail::__specialization_of<O, rational>)
constexpr bool operator>=(const O & other, const rational<N, D> & r) {
    return other * r.den >= r.num;
}

}  // namespace melon
}  // namespace fhamonic

#endif  // RATIONAL_HPP