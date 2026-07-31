#pragma once

#include <concepts>
#include <ranges>
#include <type_traits>

namespace melon {

namespace detail {
template <typename T, template <typename...> typename Primary>
struct is_specialization_of : std::false_type {};

template <template <typename...> typename Primary, typename... Args>
struct is_specialization_of<Primary<Args...>, Primary> : std::true_type {};

template <typename T, template <typename...> typename Primary>
concept specialization_of = is_specialization_of<T, Primary>::value;

// How cheap a range is to walk, used by the arcs / arcs_entries CPOs to pick
// between otherwise-equivalent fallbacks.
template <typename T>
inline constexpr int range_rank() {
    if constexpr(specialization_of<T, std::ranges::iota_view>)
        return 3;
    else if constexpr(std::ranges::contiguous_range<T>)
        return 2;
    else if constexpr(std::ranges::viewable_range<T>)
        return 1;
    else
        return 0;
}
}  // namespace detail

}  // namespace melon
