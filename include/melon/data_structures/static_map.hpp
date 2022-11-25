#ifndef MELON_STATIC_MAP_HPP
#define MELON_STATIC_MAP_HPP

#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <ranges>

namespace fhamonic {
namespace melon {

template <typename T>
class static_map {
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
    static_map() : _data(nullptr), _data_end(nullptr){};
    static_map(const size_type & size)
        : _data(std::make_unique_for_overwrite<value_type[]>(size))
        , _data_end(begin() + size){};

    static_map(const size_type & size, const value_type & init_value) : static_map(size) {
        std::ranges::fill(*this, init_value);
    }

    template <std::random_access_iterator IT>
    static_map(IT && it_begin, IT && it_end)
        : static_map(static_cast<size_type>(std::distance(it_begin, it_end))) {
        std::copy(it_begin, it_end, begin());
    }
    template <std::ranges::random_access_range R>
    explicit static_map(R && r) : static_map(std::ranges::size(r)) {
        std::ranges::copy(r, begin());
    }
    static_map(const static_map & other)
        : static_map(other.begin(), other.end()){};
    static_map(static_map &&) = default;

    static_map & operator=(const static_map & other) {
        resize(other.size());
        std::ranges::copy(other, begin());
    }
    static_map & operator=(static_map &&) = default;

    iterator begin() noexcept { return _data.get(); }
    iterator end() noexcept { return _data_end; }
    const_iterator begin() const noexcept { return _data.get(); }
    const_iterator end() const noexcept { return _data_end; }

    size_type size() const noexcept {
        return static_cast<size_type>(std::distance(begin(), end()));
    }
    void resize(const size_type & n) {
        if(n == size()) return;
        _data = std::make_unique_for_overwrite<value_type[]>(n);
        _data_end = begin() + n;
    }

    reference operator[](const size_type & i) noexcept {
        assert(i < size());
        return _data[i];
    }
    const_reference operator[](const size_type & i) const noexcept {
        assert(i < size());
        return _data[i];
    }
};

}  // namespace melon
}  // namespace fhamonic

#include "melon/data_structures/static_map_atomic_bool.hpp"
#include "melon/data_structures/static_map_bool.hpp"

#endif  // MELON_STATIC_MAP_HPP