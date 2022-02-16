#ifndef MELON_STATIC_MAP_HPP
#define MELON_STATIC_MAP_HPP

#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <vector>

namespace fhamonic {
namespace melon {

template <typename T>
class StaticMap {
public:
    using value_type = T;
    using reference = T &;
    using const_reference = const T &;
    using iterator = T *;
    using const_iterator = const T *;
    using size_type = std::size_t;

private:
    std::unique_ptr<value_type[]> _data;
    value_type * _data_end;

public:
    StaticMap() : _data(nullptr), _data_end(nullptr){};
    StaticMap(size_type size)
        : _data(std::make_unique_for_overwrite<value_type[]>(size))
        , _data_end(_data.get() + size){};

    StaticMap(size_type size, value_type init_value) : StaticMap(size) {
        std::ranges::fill(*this, init_value);
    }

    template <std::ranges::random_access_range R>
    explicit StaticMap(R && r) : StaticMap(std::ranges::size(r)) {
        std::ranges::copy(r, _data.get());
    }
    StaticMap(StaticMap &&) = default;

    StaticMap & operator=(const StaticMap & other) {
        resize(other.size());
        std::ranges::copy(other, _data);
    }
    StaticMap & operator=(StaticMap &&) = default;

    iterator begin() noexcept { return _data.get(); }
    iterator end() noexcept { return _data_end; }
    const_iterator begin() const noexcept { return _data.get(); }
    const_iterator end() const noexcept { return _data_end; }

    size_type size() const noexcept {
        return static_cast<size_type>(std::distance(begin(), end()));
    }
    void resize(size_type n) {
        if(n == size()) return;
        _data = std::make_unique_for_overwrite<value_type[]>(n);
        _data_end = _data.get() + n;
    }

    reference operator[](size_type i) noexcept {
        assert(i < size());
        return _data[i];
    }
    const_reference operator[](size_type i) const noexcept {
        assert(i < size());
        return _data[i];
    }
};

}  // namespace melon
}  // namespace fhamonic

#include "melon/static_map_bool.hpp"
#include "melon/static_map_atomic_bool.hpp"

#endif  // MELON_STATIC_MAP_HPP