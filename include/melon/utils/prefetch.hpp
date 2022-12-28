#ifndef MELON_UTILS_PREFETCH_HPP
#define MELON_UTILS_PREFETCH_HPP

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

template <std::ranges::range K,
          input_value_map<std::ranges::range_value_t<K>> M>
constexpr void prefetch_mapped_values(const K & values, const M & map) {
    if constexpr(std::ranges::sized_range<K> &&
                 std::integral<std::ranges::range_value_t<K>> && requires {
                     __builtin_prefetch(nullptr);
                     map.data();
                 }) {
        if(values.size()) {
            __builtin_prefetch(map.data() + values.front());
        }
    }
}

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILS_PREFETCH_HPP
