#pragma once

#include <concepts>
#include <ranges>
#include <type_traits>

#include "melon/mapping.hpp"

namespace melon {

template <std::ranges::range R>
constexpr void prefetch_range(const R & range) {
    if constexpr(std::ranges::contiguous_range<R>) {
#if defined(__GNUC__)
        __builtin_prefetch(range.data());
#endif
    }
}

template <std::ranges::range Keys,
          mapping<std::ranges::range_value_t<Keys>> ValueMap>
constexpr void prefetch_mapped_values(const Keys & keys,
                                      const ValueMap & value_map) {
    if constexpr(requires {
                     std::ranges::begin(keys);
                     std::ranges::end(keys);
                 } && contiguous_mapping<ValueMap,
                                         std::ranges::range_value_t<Keys>>) {
#if defined(__GNUC__)
        if(std::ranges::begin(keys) != std::ranges::end(keys)) {
            __builtin_prefetch(value_map.data() + *std::ranges::begin(keys));
        }
#endif
    }
}

}  // namespace melon
