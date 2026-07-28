#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <iterator>
#include <memory>
#include <ranges>

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
    [[nodiscard]] constexpr static_map() noexcept : _data(nullptr), _size(0) {};
    [[nodiscard]] constexpr explicit static_map(const size_type size)
        : _data(std::make_unique_for_overwrite<mapped_type[]>(size))
        // : _data(reinterpret_cast<mapped_type *>(
        //       std::malloc(size * sizeof(mapped_type))))
        , _size(size) {};

    [[nodiscard]] constexpr static_map(const size_type size,
                                       const mapped_type & init_value)
        : static_map(size) {
        std::fill(_data.get(), _data.get() + _size, init_value);
    }

    // Taken by value, not by `IT &&`: as forwarding references these deduced
    // IT = T & for named iterators (which no longer models
    // random_access_iterator, silently removing the constructor) and produced
    // "deduced conflicting types for IT" when the two arguments differed in
    // value category, e.g. static_map(it, v.end()).
    template <std::random_access_iterator IT>
    [[nodiscard]] constexpr static_map(IT it_begin, IT it_end)
        : static_map(static_cast<size_type>(std::distance(it_begin, it_end))) {
        std::copy(it_begin, it_end, _data.get());
    }
    template <std::ranges::random_access_range R>
    [[nodiscard]] constexpr explicit static_map(R && r)
        : static_map(std::ranges::begin(r), std::ranges::end(r)) {}
    static_map(const static_map & other)
        : static_map(other.data(), other.data() + other.size()) {};
    [[nodiscard]] constexpr static_map(static_map &&) = default;

    static_map & operator=(const static_map & other) {
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

    [[nodiscard]] constexpr size_type size() const noexcept { return _size; }
    // Named reset(), not resize(): this reallocates and leaves every element
    // value-uninitialised. It does NOT preserve the existing contents the way
    // std::vector::resize does, and callers relying on that name were silently
    // losing their data.
    constexpr void reset(const size_type n) {
        if(n == size()) return;
        _data = std::make_unique_for_overwrite<mapped_type[]>(n);
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
