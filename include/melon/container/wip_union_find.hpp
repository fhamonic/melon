#ifndef MELON_UNION_FIND_HPP
#define MELON_UNION_FIND_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "melon/mapping.hpp"

namespace fhamonic {
namespace melon {

template <typename K, typename V,
          output_map_of<K, std::size_t> M =
              std::unordered_map<K, std::size_t>>
class union_find {
public:
    using key_type = K;
    using component_type = V;

public:
    std::vector<component_type> _array;
    M _component_map;

public:
    template <typename... indice_map_args>
    [[nodiscard]] constexpr explicit union_find(indice_map_args &&... args)
        : _array(), _component_map(std::forward<indice_map_args>(args)...) {}

    [[nodiscard]] constexpr union_find(const union_find & bin) = default;
    [[nodiscard]] constexpr union_find(union_find && bin) = default;

    union_find & operator=(const union_find &) = default;
    union_find & operator=(union_find &&) = default;

    [[nodiscard]] constexpr size_type size() const noexcept {
        return _array.size();
    }
    [[nodiscard]] constexpr bool empty() const noexcept {
        return _array.empty();
    }
    constexpr void clear() noexcept { _array.resize(0); }

    void push(const key_type & k) noexcept {}
    [[nodiscard]] constexpr component_type find(
        const key_type & k) const noexcept {}
    constexpr component_type join(const key_type & k1,
                                  const key_type & k2) const noexcept {}
};  // class union_find

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UNION_FIND_HPP