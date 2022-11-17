#ifndef MELON_CONSTEXPR_TERNARY_HPP
#define MELON_CONSTEXPR_TERNARY_HPP

#include <type_traits>
#include <utility>

namespace fhamonic {
namespace melon {

template <bool B, typename T, typename F>
decltype(auto) constexpr_ternary(T && t, F && f) {
    if constexpr(B) {
        return std::forward<T>(t);
    } else {
        return std::forward<F>(f);
    }
}

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONSTEXPR_TERNARY_HPP