#ifndef MELON_STATIC_MAP_BOOL_HPP
#define MELON_STATIC_MAP_BOOL_HPP

#include <algorithm>
#include <bit>
#include <cassert>
#include <memory>
#include <ranges>
#include <vector>

#include "melon/static_map.hpp"

namespace fhamonic {
namespace melon {

template <>
class StaticMap<bool> {
public:
    using value_type = bool;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

private:
    using span_type = std::size_t;
    static_assert(std::is_unsigned_v<span_type>);
    static constexpr size_type N = sizeof(span_type) << 3;
    static constexpr size_type span_index_mask = N - 1;
    static constexpr size_type nb_spans(std::size_t n) {
        return (n + N - 1) / N;
    }

public:
    // Branchless version
    // class reference {
    // private:
    //     span_type * _p;
    //     size_type _local_index;

    // public:
    //     reference(span_type * p, size_type index)
    //         : _p(p), _local_index(index) {}
    //     reference(const reference &) = default;

    //     operator bool() const noexcept { return (*_p >> _local_index) & 1; }
    //     reference & operator=(bool b) noexcept {
    //         *_p ^= (((*_p >> _local_index) & 1) ^ b) << _local_index;
    //         return *this;
    //     }
    //     reference & operator=(const reference & other) noexcept {
    //         return *this = bool(other);
    //     }
    //     bool operator==(const reference & x) const noexcept {
    //         return bool(*this) == bool(x);
    //     }
    //     bool operator<(const reference & x) const noexcept {
    //         return !bool(*this) && bool(x);
    //     }
    // };

    struct reference {
        span_type * _p;
        span_type _mask;

        reference(span_type * x, size_type y)
            : _p(x), _mask(span_type(1) << y) {}
        reference() noexcept : _p(0), _mask(0) {}
        reference(const reference &) = default;

        operator bool() const noexcept { return !!(*_p & _mask); }
        reference & operator=(bool x) noexcept {
            if(x)
                *_p |= _mask;
            else
                *_p &= ~_mask;
            return *this;
        }
        reference & operator=(const reference & x) noexcept {
            return *this = bool(x);
        }
        bool operator==(const reference & x) const {
            return bool(*this) == bool(x);
        }
        bool operator<(const reference & x) const {
            return !bool(*this) && bool(x);
        }
    };
    using const_reference = bool;

    class iterator_base {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = StaticMap<bool>::difference_type;
        using value_type = bool;
        using pointer = void;

    protected:
        span_type * _p;
        size_type _local_index;

    public:
        iterator_base(span_type * p, size_type index)
            : _p(p), _local_index(index) {}

        iterator_base() = default;
        iterator_base(const iterator_base &) = default;
        iterator_base(iterator_base &&) = default;

        iterator_base & operator=(const iterator_base &) = default;
        iterator_base & operator=(iterator_base &&) = default;

    protected:
        constexpr void _incr() noexcept {
            if(_local_index++ == N - 1) {
                ++_p;
                _local_index = 0;
            }
        }
        constexpr void _decr() noexcept {
            if(_local_index++ == 0) {
                --_p;
                _local_index = N - 1;
            }
        }
        constexpr void _incr(difference_type i) noexcept {
            difference_type n = static_cast<difference_type>(_local_index) + i;
            _p += n / difference_type(N);
            n = n % difference_type(N);
            if(n < 0) {
                n += difference_type(N);
                --_p;
            }
            _local_index = static_cast<size_type>(n);
        }

    public:
        friend bool operator==(const iterator_base & x,
                               const iterator_base & y) noexcept {
            return x._p == y._p && x._local_index == y._local_index;
        }
        friend constexpr std::strong_ordering operator<=>(
            const iterator_base & x, const iterator_base & y) noexcept {
            if(const auto cmp = x._p <=> y._p; cmp != 0) return cmp;
            return x._local_index <=> y._local_index;
        }
        difference_type operator-(const iterator_base & other) const noexcept {
            return (difference_type(N) * (_p - other._p) +
                    static_cast<difference_type>(_local_index) -
                    static_cast<difference_type>(other._local_index));
        }
    };

    class iterator : public iterator_base {
    public:
        using reference = StaticMap<bool>::reference;

        using iterator_base::iterator_base;

        iterator & operator++() noexcept {
            _incr();
            return *this;
        }
        iterator operator++(int) noexcept {
            iterator tmp = *this;
            _incr();
            return tmp;
        }
        iterator & operator--() noexcept {
            _decr();
            return *this;
        }
        iterator operator--(int) noexcept {
            iterator tmp = *this;
            _decr();
            return tmp;
        }
        iterator & operator+=(difference_type i) noexcept {
            _incr(i);
            return *this;
        }

        iterator & operator-=(difference_type i) noexcept {
            _incr(-i);
            return *this;
        }

        friend iterator operator+(const iterator & x, difference_type n) {
            iterator tmp = x;
            tmp += n;
            return tmp;
        }
        friend iterator operator+(difference_type n, const iterator & x) {
            return x + n;
        }
        friend iterator operator-(const iterator & x, difference_type n) {
            iterator tmp = x;
            tmp -= n;
            return tmp;
        }

        reference operator*() const noexcept {
            return reference(_p, _local_index);
        }
        reference operator[](difference_type i) const { return *(*this + i); }
    };

    class const_iterator : public iterator_base {
    public:
        using reference = const_reference;

        using iterator_base::iterator_base;

        const_iterator & operator++() noexcept {
            _incr();
            return *this;
        }
        const_iterator operator++(int) noexcept {
            const_iterator tmp = *this;
            _incr();
            return tmp;
        }
        const_iterator & operator--() noexcept {
            _decr();
            return *this;
        }
        const_iterator operator--(int) noexcept {
            const_iterator tmp = *this;
            _decr();
            return tmp;
        }
        const_iterator & operator+=(difference_type i) noexcept {
            _incr(i);
            return *this;
        }

        const_iterator & operator-=(difference_type i) noexcept {
            _incr(-i);
            return *this;
        }

        friend const_iterator operator+(const const_iterator & x,
                                        difference_type n) {
            const_iterator tmp = x;
            tmp += n;
            return tmp;
        }
        friend const_iterator operator+(difference_type n,
                                        const const_iterator & x) {
            return x + n;
        }
        friend const_iterator operator-(const const_iterator & x,
                                        difference_type n) {
            const_iterator tmp = x;
            tmp -= n;
            return tmp;
        }

        const_reference operator*() const noexcept {
            return (*_p >> _local_index) & 1;
        }
        const_reference operator[](difference_type i) const {
            return *(*this + i);
        }
    };

private:
    std::unique_ptr<span_type[]> _data;
    size_type _size;

public:
    StaticMap() : _data(nullptr), _size(0){};
    StaticMap(size_type size)
        : _data(std::make_unique_for_overwrite<span_type[]>(nb_spans(size))), _size(size){};

    StaticMap(size_type size, bool init_value) : StaticMap(size) {
        fill(init_value);
    };

    StaticMap(const StaticMap & other) : StaticMap(other._size) {
        std::copy(other._data.get(), other._data.get() + nb_spans(other._size),
                  _data.get());
    };
    StaticMap(StaticMap &&) = default;

    StaticMap & operator=(const StaticMap & other) {
        resize(other.size());
        std::copy(other._data.get(), other._data.get() + nb_spans(other._size),
                  _data.get());
        return *this;
    };
    StaticMap & operator=(StaticMap &&) = default;

    iterator begin() noexcept { return iterator(_data.get(), 0); }
    iterator end() noexcept {
        return iterator(_data.get() + _size / N, _size & span_index_mask);
    }
    const_iterator begin() const noexcept {
        return const_iterator(_data.get(), 0);
    }
    const_iterator end() const noexcept {
        return const_iterator(_data.get() + _size / N, _size & span_index_mask);
    }

    size_type size() const noexcept { return _size; }
    void resize(size_type n) {
        if(n == _size) return;
        _data = std::make_unique_for_overwrite<span_type[]>(nb_spans(n));
        _size = n;
    }

    reference operator[](size_type i) noexcept {
        assert(i < size());
        return reference(_data.get() + i / N, i & span_index_mask);
    }
    const_reference operator[](size_type i) const noexcept {
        assert(i < size());
        return reference(_data.get() + i / N, i & span_index_mask);
    }

    void fill(bool b) noexcept {
        std::fill(_data.get(), _data.get() + nb_spans(_size),
                  b ? ~span_type(0) : span_type(0));
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STATIC_MAP_BOOL_HPP