#ifndef MELON_CONCEPTS_ID_VALUE_MAP_HPP
#define MELON_CONCEPTS_ID_VALUE_MAP_HPP

#include <concepts>

namespace fhamonic {
namespace melon {

template <typename M, typename I, typename V>
concept id_value_read_map = std::same_as<typename M::value_type, V> &&
    requires(M m, I id) {
    { m[id] } -> std::convertible_to<V>;
};

template <typename M, typename I, typename V>
concept id_value_map = id_value_read_map<M, I, V> && requires(M m, I id, V v) {
    { m[id] = v } -> std::convertible_to<V>;
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_ID_VALUE_MAP_HPP