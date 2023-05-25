#ifndef MELON_DETAIL_PREFETCH_HPP
#define MELON_DETAIL_PREFETCH_HPP

#include <concepts>
#include <ranges>
#include <type_traits>

#include "melon/utility/value_map.hpp"

namespace fhamonic {
namespace melon {

template <std::ranges::range R>
constexpr void prefetch_range(const R & range) {
    if constexpr(requires { __builtin_prefetch(nullptr); } &&
                 std::ranges::contiguous_range<R>) {
        __builtin_prefetch(range.data());
    }
}

template <std::ranges::range _Keys,
          value_map<std::ranges::range_value_t<_Keys>> _ValueMap>
constexpr void prefetch_mapped_values(const _Keys & __keys,
                                      const _ValueMap & __map) {
    if constexpr(requires {
                     __builtin_prefetch(nullptr);
                     std::ranges::begin(__keys);
                     std::ranges::end(__keys);
                 } && contiguous_value_map<_ValueMap,
                                           std::ranges::range_value_t<_Keys>>) {
        if(std::ranges::begin(__keys) != std::ranges::end(__keys)) {
            __builtin_prefetch(__map.data() + *std::ranges::begin(__keys));
        }
    }
}

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DETAIL_PREFETCH_HPP
