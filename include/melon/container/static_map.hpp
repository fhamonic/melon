#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "melon/detail/not_self.hpp"

namespace melon {

template <std::integral K = std::size_t, typename V = std::size_t>
class static_map {
public:
    using key_type = K;
    using mapped_type = V;
    // value_type is the mapped value, not a key-value pair: the iterators are
    // plain V pointers, so a pair-shaped value_type would put
    // std::iterator_traits and std::ranges::range_value_t at odds with the
    // container's own typedefs.
    using value_type = V;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using reference = V &;
    using const_reference = const V &;

    using iterator = mapped_type *;
    using const_iterator = const mapped_type *;

private:
    std::unique_ptr<mapped_type[]> _data;
    size_type _size;

public:
    constexpr static_map() noexcept : _data(nullptr), _size(0) {};
    constexpr explicit static_map(const size_type size)
        : _data(std::make_unique_for_overwrite<mapped_type[]>(size))
        , _size(size) {};

    constexpr static_map(const size_type size, const mapped_type & init_value)
        : static_map(size) {
        std::fill(_data.get(), _data.get() + _size, init_value);
    }

    // Taken by value, not by `IT &&`: as forwarding references these deduce
    // IT = T & for a named iterator, which does not model
    // random_access_iterator and so silently removes the constructor, and
    // they conflict outright when the two arguments differ in value category,
    // e.g. static_map(it, v.end()).
    template <std::random_access_iterator IT>
    constexpr static_map(IT it_begin, IT it_end)
        : static_map(static_cast<size_type>(std::distance(it_begin, it_end))) {
        std::copy(it_begin, it_end, _data.get());
    }
    template <std::ranges::random_access_range R>
        requires detail::not_self<R, static_map>
    constexpr explicit static_map(R && r)
        : static_map(std::ranges::begin(r), std::ranges::end(r)) {}
    // Forward ranges too, the way std containers accept forward iterators:
    // one sizing pass through ranges::distance, then the copy. The digraphs'
    // forward_range constructors forward their endpoint ranges here, so
    // without this overload std::constructible_from answers yes for a
    // forward_list while construction hard-errors in the mem-initializer.
    template <std::ranges::forward_range R>
        requires detail::not_self<R, static_map> &&
                 (!std::ranges::random_access_range<R>) &&
                 std::convertible_to<std::ranges::range_reference_t<R>,
                                     mapped_type>
    constexpr explicit static_map(R && r)
        : static_map(static_cast<size_type>(std::ranges::distance(r))) {
        std::ranges::copy(r, _data.get());
    }
    constexpr static_map(const static_map & other)
        : static_map(other.data(), other.data() + other.size()) {};
    // Hand-written so the source leaves as a valid *empty* map: a defaulted
    // move nulls _data but keeps _size, and a moved-from map then answers
    // size() == N over a null buffer -- a reachable state, since algorithms
    // take their graph, whose state this is, by value.
    constexpr static_map(static_map && other) noexcept
        : _data(std::move(other._data)), _size(std::exchange(other._size, 0)) {}

    constexpr static_map & operator=(const static_map & other) {
        reset(other.size());
        std::copy(other.data(), other.data() + other.size(), _data.get());
        return *this;
    }
    constexpr static_map & operator=(static_map && other) noexcept {
        _data = std::move(other._data);
        _size = std::exchange(other._size, 0);
        return *this;
    }

    [[nodiscard]] constexpr iterator begin() noexcept { return _data.get(); }
    [[nodiscard]] constexpr iterator end() noexcept {
        return _data.get() + _size;
    }
    [[nodiscard]] constexpr const_iterator begin() const noexcept {
        return _data.get();
    }
    [[nodiscard]] constexpr const_iterator end() const noexcept {
        return _data.get() + _size;
    }

    [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
        return _data.get();
    }
    [[nodiscard]] constexpr const_iterator cend() const noexcept {
        return _data.get() + _size;
    }

    [[nodiscard]] constexpr size_type size() const noexcept { return _size; }
    [[nodiscard]] constexpr bool empty() const noexcept { return _size == 0; }

    constexpr void swap(static_map & other) noexcept {
        std::swap(_data, other._data);
        std::swap(_size, other._size);
    }
    friend constexpr void swap(static_map & a, static_map & b) noexcept {
        a.swap(b);
    }
    // reset() reallocates and keeps nothing; resize() reallocates and keeps
    // the elements that still fit. Neither initialises: after reset() every
    // element is indeterminate, after a growing resize() the new tail is --
    // unlike std::vector::resize, which value-initialises it. fill() or
    // assign the tail yourself if you need a known state.
    //
    // Both are a no-op at the current size, so reset(size()) preserves the
    // contents it would otherwise discard. That is deliberate: operator=
    // calls reset() then copies, and without it every same-size assignment
    // would reallocate.
    constexpr void reset(const size_type n) {
        if(n == size()) return;
        _data = std::make_unique_for_overwrite<mapped_type[]>(n);
        _size = n;
    }
    constexpr void resize(const size_type n) {
        if(n == size()) return;
        // Allocate before touching _data: moving _data out first leaves the
        // map holding a null buffer under its *old* _size if the allocation
        // throws, and every subsequent operator[] dereferences null. This way
        // a throw leaves the map exactly as it was. (If an element's move
        // assignment throws, the buffer and size survive but the elements
        // already moved from are valid-but-unspecified.)
        auto new_data = std::make_unique_for_overwrite<mapped_type[]>(n);
        std::move(_data.get(), _data.get() + std::min(n, _size),
                  new_data.get());
        _data = std::move(new_data);
        _size = n;
    }

    [[nodiscard]] constexpr mapped_type & operator[](
        const key_type i) noexcept {
        assert(static_cast<size_type>(i) < size());
        return _data[static_cast<size_type>(i)];
    }
    [[nodiscard]] constexpr const mapped_type & operator[](
        const key_type i) const noexcept {
        assert(static_cast<size_type>(i) < size());
        return _data[static_cast<size_type>(i)];
    }
    [[nodiscard]] constexpr mapped_type & at(const key_type i) {
        if(static_cast<size_type>(i) >= size())
            throw std::out_of_range("Invalid key.");
        return _data[static_cast<size_type>(i)];
    }
    [[nodiscard]] constexpr const mapped_type & at(const key_type i) const {
        if(static_cast<size_type>(i) >= size())
            throw std::out_of_range("Invalid key.");
        return _data[static_cast<size_type>(i)];
    }

    constexpr void fill(const mapped_type & v) noexcept(
        std::is_nothrow_copy_assignable_v<mapped_type>) {
        std::fill(_data.get(), _data.get() + size(), v);
    }

    [[nodiscard]] constexpr mapped_type * data() noexcept {
        return _data.get();
    }
    [[nodiscard]] constexpr const mapped_type * data() const noexcept {
        return _data.get();
    }
};

}  // namespace melon
