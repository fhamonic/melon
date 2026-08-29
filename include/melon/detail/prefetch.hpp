#pragma once

#include <concepts>
#include <ranges>
#include <type_traits>

#if defined(_MSC_VER) && !defined(__GNUC__) && \
    (defined(_M_X64) || defined(_M_IX86))
#include <xmmintrin.h>
#endif

#include "melon/mapping.hpp"

namespace melon::detail {

template <std::ranges::range R>
constexpr void prefetch_range(const R & range) {
    if constexpr(std::ranges::contiguous_range<R>) {
#if defined(__GNUC__)
        __builtin_prefetch(std::ranges::data(range));
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        // The guard is load-bearing: _mm_prefetch is not constant-evaluable,
        // so without it any constant-evaluated caller fails to compile. Not
        // `if !consteval` -- MSVC 19.41 rejects that spelling here.
        if(!std::is_constant_evaluated()) {
            _mm_prefetch(
                reinterpret_cast<const char *>(std::ranges::data(range)),
                _MM_HINT_T0);
        }
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
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        // Same constant-evaluation guard as prefetch_range above.
        if(!std::is_constant_evaluated() &&
           std::ranges::begin(keys) != std::ranges::end(keys)) {
            _mm_prefetch(reinterpret_cast<const char *>(
                             value_map.data() + *std::ranges::begin(keys)),
                         _MM_HINT_T0);
        }
#endif
    }
}

template <std::ranges::range Keys, typename... ValueMaps>
    requires(mapping<ValueMaps, std::ranges::range_value_t<Keys>> && ...)
constexpr void prefetch_keys_and_values(const Keys & keys,
                                        const ValueMaps &... value_maps) {
    prefetch_range(keys);
    (prefetch_mapped_values(keys, value_maps), ...);
}

}  // namespace melon::detail
