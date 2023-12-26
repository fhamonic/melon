#ifndef MELON_DISJOINT_SETS_HPP
#define MELON_DISJOINT_SETS_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "melon/mapping.hpp"

namespace fhamonic {
namespace melon {

template <typename K,
          output_mapping<K> M =
              mapping_owning_view<std::unordered_map<K, unsigned int>>>
    requires std::integral<mapped_value_t<M, K>>
class disjoint_sets {
public:
    using key_type = K;
    using component_type = mapped_value_t<M, K>;

public:
    M _component_map;
    std::vector<component_type> _parent_map;
    std::vector<component_type> _size_map;

public:
    [[nodiscard]] constexpr disjoint_sets()
        : _component_map(), _parent_map(), _size_map() {}

    template <typename CM>
    [[nodiscard]] constexpr explicit disjoint_sets(CM && component_map)
        : _component_map(std::forward<CM>(component_map))
        , _parent_map()
        , _size_map() {}

    [[nodiscard]] constexpr disjoint_sets(const disjoint_sets &) = default;
    [[nodiscard]] constexpr disjoint_sets(disjoint_sets &&) = default;

    disjoint_sets & operator=(const disjoint_sets &) = default;
    disjoint_sets & operator=(disjoint_sets &&) = default;

    [[nodiscard]] constexpr auto size() const noexcept {
        return _parent_map.size();
    }
    [[nodiscard]] constexpr bool empty() const noexcept {
        return _parent_map.empty();
    }
    constexpr void clear() noexcept {
        _parent_map.resize(0);
        _size_map.resize(0);
    }

    void push(const key_type & k) noexcept {
        const component_type c = static_cast<component_type>(size());
        _component_map[k] = c;
        _parent_map.push_back(c);
        _size_map.push_back(1);
    }

private:
    [[nodiscard]] constexpr component_type recursive_find(
        const component_type & c) noexcept {
        if(_parent_map[c] == c) return c;
        return _parent_map[c] =
                   recursive_find(_parent_map[c]);  // path compression
        // return recursive_find(_parent_map[c]); // naive
    }

public:
    [[nodiscard]] constexpr component_type find(const key_type & k) noexcept {
        return recursive_find(_component_map[k]);
    }

    constexpr component_type merge(component_type c1,
                                   component_type c2) noexcept {
        if(c1 == c2) return c1;
        if(_size_map[c1] < _size_map[c2]) std::swap(c1, c2);

        _size_map[c1] += _size_map[c2];
        return _parent_map[c2] = c1;
    }
    constexpr component_type merge_keys(const key_type & k1,
                                        const key_type & k2) noexcept {
        return merge(find(k1), find(k2));
    }
};  // class disjoint_sets

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DISJOINT_SETS_HPP