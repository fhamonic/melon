#ifndef MELON_CONCEPTS_KEY_VALUE_MAP_HPP
#define MELON_CONCEPTS_KEY_VALUE_MAP_HPP

#include <concepts>
#include <type_traits>

namespace fhamonic {
namespace melon {

template <typename M, typename K>
using mapped_value_t = std::remove_const_t<
    std::remove_reference_t<decltype(std::declval<M>()[std::declval<K>()])>>;

// clang-format off
namespace concepts {
template <typename M, typename K>
concept map_of = !std::same_as<mapped_value_t<M,K>,void>;

template <typename M, typename K, typename V>
concept key_value_map_view = map_of<M, K> && std::convertible_to<mapped_value_t<M,K>,V>;

template <typename M, typename K, typename V>
concept key_value_map = key_value_map_view<M,K,V> &&
    requires(M map, K key, mapped_value_t<M,K> value) {
        { map[key] = value } -> std::convertible_to<V>;
    };

}  // namespace concepts
// clang-format on

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_KEY_VALUE_MAP_HPP