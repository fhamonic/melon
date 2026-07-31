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
        __builtin_prefetch(std::ranges::data(range));
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

// The preamble every Dijkstra-shaped advance() opens with: warm the incidence
// range, then each map it is about to subscript with those keys. Written out
// identically in dijkstra, network_voronoi, competing_dijkstras,
// biobjective_dijkstra and both flow algorithms. Variadic so a caller naming
// two length maps says so in one line; each call inlines to exactly the
// sequence it replaces.
template <std::ranges::range Keys, typename... ValueMaps>
    requires(mapping<ValueMaps, std::ranges::range_value_t<Keys>> && ...)
constexpr void prefetch_keys_and_values(const Keys & keys,
                                        const ValueMaps &... value_maps) {
    prefetch_range(keys);
    (prefetch_mapped_values(keys, value_maps), ...);
}

}  // namespace melon
