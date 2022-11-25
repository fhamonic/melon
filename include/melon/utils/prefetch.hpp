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
    using value_t = typename std::ranges::range_value_t<V>;
    // std::same_as<V, std::ranges::iota_view<value_t, value_t>>
    if constexpr(std::is_arithmetic_v<value_t> &&
                 !std::ranges::contiguous_range<V> &&
                 std::ranges::random_access_range<V> &&
                 std::ranges::contiguous_range<M>) {
        if(values.size()) {
            __builtin_prefetch(&map[values.front()]);
        }
    }
}

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILS_PREFETCH_HPP
