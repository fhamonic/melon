#ifndef MELON_UTILS_PREFETCH_HPP
#define MELON_UTILS_PREFETCH_HPP

#include <concepts>
#include <ranges>
#include <type_traits>

namespace fhamonic {
namespace melon {

template <std::ranges::range R>
constexpr void prefetch_range(const R & range) {
    if constexpr(std::ranges::contiguous_range<R>) {
        __builtin_prefetch(range.data());
    }
}

template <std::ranges::range V, typename M>
constexpr void prefetch_map_values(const V & values, const M & map) {
    using key_type = typename std::ranges::range_value_t<V>;
    if constexpr(std::integral<key_type> && requires() { map.data(); }) {
        if(values.size()) {
            __builtin_prefetch(map.data() + values.front());
        }
    }
}

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILS_PREFETCH_HPP
