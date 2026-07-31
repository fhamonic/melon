#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "melon/detail/not_self.hpp"
#include "melon/mapping.hpp"

namespace melon {

template <typename K,
          output_mapping<K> M =
              mapping_owning_view<std::unordered_map<K, unsigned int>>>
    requires std::integral<mapped_value_t<M, K>>
class disjoint_sets {
public:
    using key_type = K;
    using component_type = mapped_value_t<M, K>;

private:
    M _component_map;
    std::vector<component_type> _parent_map;
    std::vector<component_type> _size_map;

public:
    constexpr disjoint_sets() : _component_map(), _parent_map(), _size_map() {}

    template <typename CM>
        requires detail::not_self<CM, disjoint_sets>
    constexpr explicit disjoint_sets(CM && component_map)
        : _component_map(std::forward<CM>(component_map))
        , _parent_map()
        , _size_map() {}

    constexpr disjoint_sets(const disjoint_sets &) = default;
    constexpr disjoint_sets(disjoint_sets &&) = default;

    disjoint_sets & operator=(const disjoint_sets &) = default;
    disjoint_sets & operator=(disjoint_sets &&) = default;

    [[nodiscard]] constexpr auto size() const noexcept {
        return _parent_map.size();
    }
    [[nodiscard]] constexpr bool empty() const noexcept {
        return _parent_map.empty();
    }
    // Drops the components, not the key-to-component map: M is only an
    // output_mapping, so there is no way to enumerate or erase the keys already
    // written into it. A key pushed before a clear() and not pushed again
    // therefore still maps to a component index this object no longer has, and
    // find() on it is out of range. Push every key you intend to query again
    // after each clear().
    constexpr void clear() noexcept {
        _parent_map.resize(0);
        _size_map.resize(0);
    }

    // None of the four below are noexcept: push_back allocates, and all of
    // them subscript the caller-supplied component map.
    constexpr void push(const key_type & k) {
        const component_type c = static_cast<component_type>(size());
        _component_map[k] = c;
        _parent_map.push_back(c);
        _size_map.push_back(1);
    }

public:
    // Precondition: `k` was pushed since the last clear(). M is only an
    // output_mapping, so an unpushed key reads back whatever the caller's map
    // holds for it -- 0 for a std::unordered_map, uninitialised memory for a
    // static_map -- and _parent_map is then indexed with that.
    [[nodiscard]] constexpr component_type find(const key_type & k) {
        component_type c = _component_map[k];
        while(_parent_map[c] != c) {
            std::tie(c, _parent_map[c]) =
                std::tie(_parent_map[c], _parent_map[_parent_map[c]]);
        }
        return c;
    }

    constexpr component_type merge(const component_type & c1,
                                   const component_type & c2) {
        if(c1 == c2) return c1;
        if(_size_map[c1] < _size_map[c2]) {
            _size_map[c2] += _size_map[c1];
            return _parent_map[c1] = c2;
        } else {
            _size_map[c1] += _size_map[c2];
            return _parent_map[c2] = c1;
        }
    }
    constexpr component_type merge_keys(const key_type & k1,
                                        const key_type & k2) {
        return merge(find(k1), find(k2));
    }
};

}  // namespace melon
