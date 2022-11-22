#ifndef MELON_CONCEPTS_KEY_VALUE_MAP_HPP
#define MELON_CONCEPTS_KEY_VALUE_MAP_HPP

#include <concepts>
#include <type_traits>

namespace fhamonic {
namespace melon {
namespace concepts {
namespace detail {

template <typename M>
concept has_mapped_type = requires { typename M::mapped_type; };

}  // namespace detail

template <typename M>
using map_key_t =
    std::conditional_t<detail::has_mapped_type<M>,
                       //typename std::remove_reference_t<M>::key_type,
                       std::size_t,
                       std::size_t>;

template <typename M>
using map_value_t =
    std::conditional_t<detail::has_mapped_type<M>,
                       //typename std::remove_reference_t<M>::mapped_type,
                       typename std::remove_reference_t<M>::value_type,
                       typename std::remove_reference_t<M>::value_type>;

template <typename M, typename K>
concept map_of = requires(M map, map_key_t<M> key, map_value_t<M> value) {
    { map[key] } -> std::convertible_to<map_value_t<M>>;
};


template <typename M, typename K, typename V>
concept key_value_map_view = map_of<M, K> && std::same_as<map_value_t<M>, V>;

template <typename M, typename K, typename V>
concept key_value_map = key_value_map_view<M,K,V> &&
    requires(M map, map_key_t<M> key, map_value_t<M> value) {
        { map[key] = value } -> std::convertible_to<V>;
    };

}  // namespace concepts

// template <typename F>
// class map_view {
// public:
//     using key_type = int;
//     using mapped_type = int;

// private:
//     F _func;

// public:
//     map_view(F && f) : _func(std::forward<F>(f)) {}

//     decltype(auto) operator[](const key_type & k) { return _func(k); }
// };

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_KEY_VALUE_MAP_HPP