#ifndef MELON_VALUE_MAP_HPP
#define MELON_VALUE_MAP_HPP

#include <concepts>
#include <type_traits>
#include <utility>

namespace fhamonic {
namespace melon {

template <typename M, typename K>
using mapped_reference_t = decltype(std::declval<M>()[std::declval<K>()]);

template <typename M, typename K>
using mapped_value_t = std::decay_t<mapped_reference_t<M, K>>;

template <typename M, typename K>
concept input_value_map = requires(M m, K k) {
    m[k];
};

template <typename M, typename K, typename V>
concept input_value_map_of =
    input_value_map<M, K> && std::same_as<mapped_value_t<M, K>, V>;

template <typename M, typename K>
concept output_value_map = input_value_map<M, K> &&
    requires(M map, K key, mapped_value_t<M, K> value) {
    { map[key] = value } -> std::same_as<mapped_reference_t<M, K>>;
};

template <typename M, typename K, typename V>
concept output_value_map_of =
    output_value_map<M, K> && std::same_as<mapped_value_t<M, K>, V>;

// TODO refactor with the same rationale as views::all
namespace views {
template <typename F>
class map {
private:
    F _func;

public:
    [[nodiscard]] constexpr map(F && f) : _func(std::forward<F>(f)) {}
    [[nodiscard]] constexpr auto operator[](const auto & k) const noexcept {
        return _func(k);
    }
};
}  // namespace views

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_VALUE_MAP_HPP