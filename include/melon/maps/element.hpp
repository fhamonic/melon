#pragma once

#include <cstddef>
#include <tuple>
#include <utility>

#include "melon/mapping.hpp"

namespace melon {
namespace maps {

template <std::size_t... I>
struct element : public mapping_view_base {
private:
    // std::get is noexcept for tuple/pair/array but *not* for std::variant,
    // where it throws std::bad_variant_access. An unconditional noexcept on the
    // chain below turns that throw into std::terminate.
    template <typename T, std::size_t First, std::size_t... Rest>
    static consteval bool chain_is_nothrow() {
        if constexpr(sizeof...(Rest) == 0) {
            return noexcept(std::get<First>(std::declval<T>()));
        } else {
            return noexcept(std::get<First>(std::declval<T>())) &&
                   chain_is_nothrow<
                       decltype(std::get<First>(std::declval<T>())), Rest...>();
        }
    }

    template <std::size_t First, std::size_t... Rest, typename T>
    static constexpr decltype(auto) get_chain(T && e) noexcept(
        chain_is_nothrow<T, First, Rest...>()) {
        if constexpr(sizeof...(Rest) == 0) {
            return std::get<First>(std::forward<T>(e));
        } else {
            return get_chain<Rest...>(std::get<First>(std::forward<T>(e)));
        }
    }

public:
    template <typename T>
    [[nodiscard]] constexpr decltype(auto) operator[](T && e) const
        noexcept(chain_is_nothrow<T, I...>()) {
        return get_chain<I...>(std::forward<T>(e));
    }
};

}  // namespace maps
}  // namespace melon
