#ifndef RATIONAL_HPP
#define RATIONAL_HPP

#include <limits>

namespace fhamonic {
namespace melon {

template <typename Num, typename Denom = Num>
struct rational {
    mutable Num num;
    mutable Denom denom;

    constexpr rational(Num n = Num{0}, Denom d = Denom{1}) : num(n), denom(d) {
        if constexpr(std::numeric_limits<Num>::is_signed) {
            if(denom < 0) {
                num = -num;
                denom = -denom;
            }
        }
        // normalize();
    }

    void normalize() const {
        const auto & g = std::gcd(num, denom);
        num /= g;
        denom /= g;
        if constexpr(std::numeric_limits<Num>::is_signed) {
            if(denom < 0) {
                num = -num;
                denom = -denom;
            }
        }
    }

    rational operator+(const rational & other) const {
        return rational(num * other.denom + other.num * denom,
                        denom * other.denom);
    }
    rational operator-(const rational & other) const {
        return rational(num * other.denom - other.num * denom,
                        denom * other.denom);
    }
    rational operator*(const rational & other) const {
        return rational(num * other.num, denom * other.denom);
    }
    rational operator/(const rational & other) const {
        return rational(num * other.denom, denom * other.num);
    }
    bool operator==(const rational & other) const {
        return (num * other.denom) == (other.num * denom);
    }
    template <typename ONum, typename ODenom>
    std::strong_ordering operator<=>(
        const rational<ONum, ODenom> & other) const {
        return (num * other.denom) <=> (other.num * denom);
    }
    template <typename O>
    std::strong_ordering operator<=>(const O & other) const {
        return num <=> (other * denom);
    }
};
}  // namespace melon
}  // namespace fhamonic

#endif  // RATIONAL_HPP