#ifndef MELON_STATIC_FILTER_MAP_HPP
#define MELON_STATIC_FILTER_MAP_HPP

#include <algorithm>
#include <bit>
#include <cassert>
#include <memory>
#include <ranges>
#include <vector>

#include "melon/detail/intrusive_view.hpp"

namespace fhamonic {
namespace melon {

template <std::integral K>
class static_filter_map {
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

    class reference {
    private:
        span_type * _p;
        span_type _mask;

    public:
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

    template <typename I>
    class iterator_base {
    public:
        using difference_type = static_filter_map<K>::difference_type;

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
        constexpr void _bump_up() noexcept {
            if(++_local_index == N) {
                ++_p;
                _local_index = 0;
            }
        }
        constexpr void _bump_down() noexcept {
            if(_local_index-- == 0) {
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
        friend constexpr bool operator==(const I & x, const I & y) noexcept {
            return x._p == y._p && x._local_index == y._local_index;
        }
        friend constexpr std::strong_ordering operator<=>(
            const I & x, const I & y) noexcept {
            if(const auto cmp = x._p <=> y._p; cmp != 0) return cmp;
            return x._local_index <=> y._local_index;
        }
        constexpr difference_type operator-(const I & other) const noexcept {
            return (difference_type(N) * (_p - other._p) +
                    static_cast<difference_type>(_local_index) -
                    static_cast<difference_type>(other._local_index));
        }

    public:
        constexpr I & operator++() noexcept {
            _bump_up();
            return *static_cast<I *>(this);
        }
        constexpr I operator++(int) noexcept {
            iterator tmp = *static_cast<I *>(this);
            _bump_up();
            return tmp;
        }
        constexpr I & operator--() noexcept {
            _bump_down();
            return *static_cast<I *>(this);
        }
        constexpr I operator--(int) noexcept {
            iterator tmp = *static_cast<I *>(this);
            _bump_down();
            return tmp;
        }
        constexpr I & operator+=(difference_type i) noexcept {
            _incr(i);
            return *static_cast<I *>(this);
        }

        constexpr I & operator-=(difference_type i) noexcept {
            _incr(-i);
            return *static_cast<I *>(this);
        }

        friend constexpr I operator+(const I & x, difference_type n) {
            iterator tmp = x;
            tmp += n;
            return tmp;
        }
        friend constexpr I operator+(difference_type n, const I & x) {
            return x + n;
        }
        friend constexpr I operator-(const I & x, difference_type n) {
            iterator tmp = x;
            tmp -= n;
            return tmp;
        }
    };

    class iterator : public iterator_base<iterator> {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = iterator_base<iterator>::difference_type;
        using value_type = bool;
        using pointer = void;
        using reference = static_filter_map<K>::reference;

    public:
        using iterator_base<iterator>::iterator_base;

        constexpr reference operator*() const noexcept {
            return reference(iterator_base<iterator>::_p,
                             iterator_base<iterator>::_local_index);
        }
        constexpr reference operator[](difference_type i) const {
            return *(*this + i);
        }
    };

    class const_iterator : public iterator_base<const_iterator> {
        friend static_filter_map;

    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = iterator_base<const_iterator>::difference_type;
        using value_type = bool;
        using pointer = void;
        using reference = const_reference;

    public:
        using iterator_base<const_iterator>::iterator_base;

        constexpr const_reference operator*() const noexcept {
            return (*iterator_base<const_iterator>::_p >>
                    iterator_base<const_iterator>::_local_index) &
                   static_cast<span_type>(1);
        }
        constexpr const_reference operator[](difference_type i) const {
            return *(*this + i);
        }
    };

private:
    std::unique_ptr<span_type[]> _data;
    size_type _size;

public:
    static_filter_map() : _data(nullptr), _size(0) {};
    static_filter_map(size_type size)
        : _data(std::make_unique_for_overwrite<span_type[]>(nb_spans(size)))
        , _size(size) {};

    static_filter_map(size_type size, bool init_value)
        : static_filter_map(size) {
        fill(init_value);
    };

    static_filter_map(const static_filter_map & other)
        : static_filter_map(other._size) {
        std::copy(other._data.get(), other._data.get() + nb_spans(other._size),
                  _data.get());
    };
    static_filter_map(static_filter_map &&) = default;

    static_filter_map & operator=(const static_filter_map & other) {
        resize(other.size());
        std::copy(other._data.get(), other._data.get() + nb_spans(other._size),
                  _data.get());
        return *this;
    };
    static_filter_map & operator=(static_filter_map &&) = default;

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

    [[nodiscard]] const_reference at(const size_type i) const {
        if(static_cast<size_type>(i) >= size())
            throw std::out_of_range("Invalid key.");
        return reference(_data.get() + i / N, i & span_index_mask);
    }

    [[nodiscard]] reference operator[](const size_type i) noexcept {
        assert(i < size());
        return reference(_data.get() + i / N, i & span_index_mask);
    }
    [[nodiscard]] const_reference operator[](const size_type i) const noexcept {
        assert(i < size());
        return reference(_data.get() + i / N, i & span_index_mask);
    }

    void fill(bool b) noexcept {
        std::fill(_data.get(), _data.get() + nb_spans(_size),
                  b ? ~span_type(0) : span_type(0));
    }

    template <std::ranges::viewable_range R>
    auto filter(R && r) const noexcept {
        if constexpr(std::same_as<R, std::ranges::iota_view<K, K>>) {
            K begin_index = std::max(static_cast<K>(0), *std::ranges::begin(r));
            K end_index = std::min(static_cast<K>(_size), *std::ranges::end(r));

            //*
            const_iterator begin_it(_data.get() + begin_index / N,
                                    begin_index & span_index_mask);
            const const_iterator end_it(_data.get() + end_index / N,
                                        end_index & span_index_mask);

            auto next_it = [end_it](const_iterator cursor) {
                span_type shifted;
                if(++cursor._local_index == N) goto find_next_span;
                shifted = (*cursor._p) >> cursor._local_index;
                if(shifted == span_type{0}) {
                find_next_span:
                    cursor._local_index = 0;
                    do {
                        if(++cursor._p > end_it._p) [[unlikely]]
                            return cursor;
                    } while(*cursor._p == span_type{0});
                    shifted = *cursor._p;
                }
                cursor._local_index +=
                    static_cast<size_type>(std::countr_zero(shifted));
                return cursor;
            };

            if(!*begin_it) begin_it = next_it(begin_it);

            return intrusive_view(
                begin_it,
                [data = _data.get()](const const_iterator & cursor) -> K {
                    return static_cast<K>(
                        static_cast<size_type>(cursor._p - data) *
                            size_type(N) +
                        cursor._local_index);
                },
                std::move(next_it),
                [end_it](const const_iterator & cursor) -> bool {
                    return cursor < end_it;
                });
            /*/
            auto next_index = [this, end_index](K i) {
                for(;;) {
                    const size_type offset = i & span_index_mask;
                    i += static_cast<size_type>(std::countr_zero(
                             _data[i / N] &
                             ((~static_cast<span_type>(1)) << offset))) -
                         offset;
                    if((i >= end_index || at(i))) [[likely]]
                        return i;
                }
            };

            if(!at(begin_index)) begin_index = next_index(begin_index);

            return intrusive_view(
                begin_index, std::identity{}, std::move(next_index),
                [end_index](const K & i) -> bool { return i < end_index; });
            //*/
        } else {
            return std::views::filter(
                std::views::transform(
                    r, [](auto && i) { return static_cast<K>(i); }),
                [this](const auto & k) {
                    return operator[](static_cast<K>(k));
                });
        }
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STATIC_FILTER_MAP_HPP