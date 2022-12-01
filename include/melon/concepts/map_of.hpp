#ifndef MELON_CONCEPTS_output_map_of_HPP
#define MELON_CONCEPTS_output_map_of_HPP

#include <concepts>
#include <type_traits>

namespace fhamonic {
namespace melon {

template <typename M, typename K>
using mapped_value_t =
    std::decay_t<decltype(std::declval<M>()[std::declval<K>()])>;

// clang-format off
namespace concepts {

template <typename M, typename K>
concept input_map = requires(M m, K k) {
    m[k];
};

template <typename M, typename K, typename V>
concept input_map_of = input_map<M, K> && std::convertible_to<mapped_value_t<M,K>,V>;

template <typename M, typename K>
concept output_map = input_map<M, K> &&
    requires(M map, K key, mapped_value_t<M,K> value) {
        { map[key] = value } -> std::convertible_to<mapped_value_t<M,K>>;
    };

template <typename M, typename K, typename V>
concept output_map_of = output_map<M, K> && std::convertible_to<mapped_value_t<M,K>,V>;

}  // namespace concepts
// clang-format on

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_output_map_of_HPP