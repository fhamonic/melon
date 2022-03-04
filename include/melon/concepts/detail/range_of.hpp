#ifndef MELON_CONCEPTS_DETAIL_RANGE_OF_HPP
#define MELON_CONCEPTS_DETAIL_RANGE_OF_HPP

#include <concepts>
#include <ranges>

namespace fhamonic {
namespace melon {
namespace concepts {
namespace detail {

template <typename T, typename V>
concept range_of = std::ranges::range<T> && std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept random_access_range_of = std::ranges::random_access_range<T> && std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept contiguous_range_of = std::ranges::contiguous_range<T> && std::same_as<std::ranges::range_value_t<T>, V>;

}  // namespace detail
}  // namespace concepts
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_DETAIL_RANGE_OF_HPP