#ifndef MELON_DETAIL_PREFETCH_HPP
#define MELON_DETAIL_PREFETCH_HPP

#include <concepts>
#include <ranges>
#include <type_traits>

#include "melon/mapping.hpp"

namespace fhamonic {
namespace melon {

template <std::ranges::range R>
constexpr void prefetch_range(const R & range) {
    if constexpr(std::ranges::contiguous_range<R>) {
#if defined(__GNUC__)
        __builtin_prefetch(range.data());
#endif
    }
}

template <std::ranges::range _Keys,
          mapping<std::ranges::range_value_t<_Keys>> _ValueMap>
constexpr void prefetch_mapped_values(const _Keys & __keys,
                                      const _ValueMap & __map) {  
    if constexpr(requires {
                    std::ranges::begin(__keys);
                    std::ranges::end(__keys);
                 } && contiguous_mapping<_ValueMap,
                                           std::ranges::range_value_t<_Keys>>) {          
#if defined(__GNUC__)
        if(std::ranges::begin(__keys) != std::ranges::end(__keys)) {
            __builtin_prefetch(__map.data() + *std::ranges::begin(__keys));
        }
#endif
    }
}

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DETAIL_PREFETCH_HPP
