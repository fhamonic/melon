#ifndef MELON_RANGE_V3_COMPATIBILITY_HPP
#define MELON_RANGE_V3_COMPATIBILITY_HPP

#include <ranges>

#include <range/v3/range/concepts.hpp>
#include <range/v3/view/concat.hpp>

template <typename... Rngs>
constexpr bool std::ranges::enable_view<::ranges::concat_view<Rngs...>> =
    ::ranges::enable_view<::ranges::concat_view<Rngs...>>;

#endif  // MELON_RANGE_V3_COMPATIBILITY_HPP