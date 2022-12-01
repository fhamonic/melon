#ifndef MELON_CONCEPTS_RANGE_OF_HPP
#define MELON_CONCEPTS_RANGE_OF_HPP

#include <concepts>
#include <ranges>

namespace fhamonic {
namespace melon {
namespace concepts {

template <typename T, typename V>
concept range_of = 
        std::ranges::range<T> && 
        std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept borrowed_range_of = 
        std::ranges::borrowed_range<T> && 
        std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept sized_range_of = 
        std::ranges::sized_range<T> && 
        std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept view_of = 
        std::ranges::view<T> && 
        std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept input_range_of = 
        std::ranges::input_range<T> && 
        std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept output_range_of = 
        std::ranges::output_range<T, V> && 
        std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept forward_range_of = 
        std::ranges::forward_range<T> && 
        std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept bidirectional_range_of = 
        std::ranges::bidirectional_range<T> && 
        std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept random_access_range_of = 
        std::ranges::random_access_range<T> && 
        std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept contiguous_range_of = 
        std::ranges::contiguous_range<T> && 
        std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept common_range_of = 
        std::ranges::common_range<T> && 
        std::same_as<std::ranges::range_value_t<T>, V>;

template <typename T, typename V>
concept viewable_range_of = 
        std::ranges::viewable_range<T> && 
        std::same_as<std::ranges::range_value_t<T>, V>;

// template <typename T, typename V>
// concept constant_range_of = 
//         std::ranges::constant_range<T> && 
//         std::same_as<std::ranges::range_value_t<T>, V>;

}  // namespace concepts
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_RANGE_OF_HPP