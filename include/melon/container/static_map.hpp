#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>

#include "melon/detail/not_self.hpp"

namespace melon {

template <std::integral K = std::size_t, typename V = std::size_t>
class static_map {
public:
    using key_type = K;
    using mapped_type = V;
    // These describe what the iterators below actually yield. They used to
    // say std::pair<const K, V &> while begin()/end() are plain V pointers,
    // so std::iterator_traits and std::ranges::range_value_t disagreed with
    // the container's own typedefs.
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
        // : _data(reinterpret_cast<mapped_type *>(
        //       std::malloc(size * sizeof(mapped_type))))
        , _size(size) {};

    constexpr static_map(const size_type size, const mapped_type & init_value)
        : static_map(size) {
        std::fill(_data.get(), _data.get() + _size, init_value);
    }

    // Taken by value, not by `IT &&`: as forwarding references these deduced
    // IT = T & for named iterators (which no longer models
    // random_access_iterator, silently removing the constructor) and produced
    // "deduced conflicting types for IT" when the two arguments differed in
    // value category, e.g. static_map(it, v.end()).
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
    // one sizing pass through ranges::distance, then the copy. This is also
    // what makes static_digraph's forward_range constructor constraint true --
    // it forwards its endpoint ranges here, and with only the random-access
    // overload std::constructible_from answered yes for a forward_list while
    // actual construction hard-errored in the mem-initializer.
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
    constexpr static_map(static_map &&) = default;

    constexpr static_map & operator=(const static_map & other) {
        reset(other.size());
        std::copy(other.data(), other.data() + other.size(), _data.get());
        return *this;
    }
    static_map & operator=(static_map &&) = default;

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

    // cbegin/cend, empty and swap: what every standard container provides and
    // this one did not. std::ranges::empty and std::ranges::swap already found
    // an answer through size() and the move operations, but only the members
    // make `m.empty()` and an ADL `swap(a, b)` work.
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
        // Allocate before touching _data. Moving _data out first left the map
        // holding a null buffer and its *old* _size if the allocation threw,
        // so every subsequent operator[] dereferenced null; this way a throw
        // leaves the map exactly as it was. std::move, not std::copy: the old
        // buffer is about to die. (If an element's move assignment throws the
        // map keeps its old buffer and size, but the elements already moved
        // from are valid-but-unspecified.)
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
    // Both overloads, like operator[] right above: the const-only at() made a
    // checked *write* impossible, which is the access that most wants checking.
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

    void fill(const mapped_type & v) noexcept(
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
