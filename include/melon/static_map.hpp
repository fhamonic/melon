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
    StaticMap(std::size_t size)
        : _data(std::make_unique<value_type[]>(size))
        , _data_end(_data + size){};

    StaticMap(std::size_t size, value_type init_value) : StaticMap(size) {
        std::ranges::fill(*this, init_value);
    };

    StaticMap(const StaticMap & other) : StaticMap(other.size()) {
        std::ranges::copy(other, _data);
    };
    StaticMap(StaticMap &&) = default;

    iterator begin() noexcept { return _data.get(); }
    iterator end() noexcept { return _data_end; }
    const_iterator cbegin() const noexcept { return _data.get(); }
    const_iterator cend() const noexcept { return _data_end; }

    size_type size() const noexcept { return std::distance(begin(), end()); }

    reference operator[](size_type i) noexcept { return _data[i]; }
    const_reference operator[](size_type i) const noexcept { return _data[i]; }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STATIC_MAP_HPP