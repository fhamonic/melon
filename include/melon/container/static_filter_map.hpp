#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

#include "melon/detail/specialization_of.hpp"

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
    static constexpr size_type num_spans(std::size_t n) {
        return (n + N - 1) / N;
    }

public:
    // A proxy, not a bool: it stores a pointer into the map's buffer, so
    // `auto b = m[i];` deduces this type rather than bool and every later
    // assignment to `b` writes through into the map. It also dangles the
    // moment the map is destroyed, moved from or reset(), while still
    // comparing and converting happily. Spell `bool b = m[i];` to snapshot.
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
            I tmp = *static_cast<I *>(this);
            _bump_up();
            return tmp;
        }
        constexpr I & operator--() noexcept {
            _bump_down();
            return *static_cast<I *>(this);
        }
        constexpr I operator--(int) noexcept {
            I tmp = *static_cast<I *>(this);
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
            I tmp = x;
            tmp += n;
            return tmp;
        }
        friend constexpr I operator+(difference_type n, const I & x) {
            return x + n;
        }
        friend constexpr I operator-(const I & x, difference_type n) {
            I tmp = x;
            tmp -= n;
            return tmp;
        }
    };

    class iterator : public iterator_base<iterator> {
    public:
        // Both spellings: the C++20 concept is satisfied through the proxy
        // reference, while the Cpp17 category deliberately overstates -- the
        // std::vector<bool> precedent -- so legacy category dispatch keeps
        // picking the random-access algorithms.
        using iterator_concept = std::random_access_iterator_tag;
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
    public:
        // Same overstatement as iterator's: dereferencing yields a prvalue
        // bool, which the C++20 concept accepts and Cpp17 would not.
        using iterator_concept = std::random_access_iterator_tag;
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
    // explicit: `static_filter_map m = 10;` reads as a value, not as a
    // request for ten default-initialised bits.
    explicit static_filter_map(size_type size)
        : _data(std::make_unique_for_overwrite<span_type[]>(num_spans(size)))
        , _size(size) {};

    static_filter_map(size_type size, bool init_value)
        : static_filter_map(size) {
        fill(init_value);
    };

    static_filter_map(const static_filter_map & other)
        : static_filter_map(other._size) {
        std::copy(other._data.get(), other._data.get() + num_spans(other._size),
                  _data.get());
    };
    // Hand-written so the source leaves as a valid *empty* map: a defaulted
    // move nulls _data but keeps _size, and the moved-from map then answers
    // size() == N over a null buffer.
    static_filter_map(static_filter_map && other) noexcept
        : _data(std::move(other._data)), _size(std::exchange(other._size, 0)) {}

    static_filter_map & operator=(const static_filter_map & other) {
        reset(other.size());
        std::copy(other._data.get(), other._data.get() + num_spans(other._size),
                  _data.get());
        return *this;
    };
    static_filter_map & operator=(static_filter_map && other) noexcept {
        _data = std::move(other._data);
        _size = std::exchange(other._size, 0);
        return *this;
    }

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

    const_iterator cbegin() const noexcept {
        return const_iterator(_data.get(), 0);
    }
    const_iterator cend() const noexcept {
        return const_iterator(_data.get() + _size / N, _size & span_index_mask);
    }

    size_type size() const noexcept { return _size; }
    [[nodiscard]] bool empty() const noexcept { return _size == 0; }

    void swap(static_filter_map & other) noexcept {
        std::swap(_data, other._data);
        std::swap(_size, other._size);
    }
    friend void swap(static_filter_map & a, static_filter_map & b) noexcept {
        a.swap(b);
    }
    // Discards the current contents and initialises nothing, except at the
    // current size, where it is a no-op that preserves them -- operator=
    // relies on that to avoid reallocating on every same-size assignment.
    void reset(size_type n) {
        if(n == _size) return;
        _data = std::make_unique_for_overwrite<span_type[]>(num_spans(n));
        _size = n;
    }

    [[nodiscard]] reference at(const size_type i) {
        if(static_cast<size_type>(i) >= size())
            throw std::out_of_range("Invalid key.");
        return reference(_data.get() + i / N, i & span_index_mask);
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
        std::fill(_data.get(), _data.get() + num_spans(_size),
                  b ? ~span_type(0) : span_type(0));
    }

    // Enumerates, in increasing key order, the set keys of a span of [0,
    // size()).
    //
    // Two shapes that look like premature complication, both about the
    // dependency chain the per-set-bit loop is bound by. The hot cursor is
    // (span pointer, bit offset) rather than a key index: an index-based
    // increment rebuilds the span pointer on every set bit, measured 2x on
    // dense scans. And the range ends at std::default_sentinel rather than
    // being common: exact equality with an end iterator needs a clamp in the
    // increment -- countr_zero may land at or beyond the end key inside the
    // last span -- and those two extra compares per set bit measured 1.5x.
    // Pipe through std::views::common if an iterator-pair range is required.
    class filter_iterator {
    public:
        using iterator_concept = std::forward_iterator_tag;
        // input, not forward: operator* returns a prvalue key, which a Cpp17
        // forward iterator may not do. Same split as iota_view's iterator.
        using iterator_category = std::input_iterator_tag;
        using value_type = K;
        using difference_type = std::ptrdiff_t;

    private:
        const span_type * _p;
        size_type _local_index;
        const span_type * _data;
        const span_type * _end_p;
        size_type _end_local;

    public:
        constexpr filter_iterator(const span_type * data, size_type index,
                                  size_type end_index) noexcept
            : _p(data + index / N)
            , _local_index(index & span_index_mask)
            , _data(data)
            , _end_p(data + end_index / N)
            , _end_local(end_index & span_index_mask) {}
        constexpr filter_iterator() = default;
        constexpr filter_iterator(const filter_iterator &) = default;
        constexpr filter_iterator(filter_iterator &&) = default;
        constexpr filter_iterator & operator=(const filter_iterator &) =
            default;
        constexpr filter_iterator & operator=(filter_iterator &&) = default;

        [[nodiscard]] constexpr K operator*() const noexcept {
            return static_cast<K>(static_cast<size_type>(_p - _data) * N +
                                  _local_index);
        }
        constexpr filter_iterator & operator++() noexcept {
            span_type shifted = span_type{0};
            if(++_local_index != N) shifted = (*_p) >> _local_index;
            if(shifted == span_type{0}) {
                _local_index = 0;
                // One past the last span the scan may dereference: _end_p
                // itself when the end key is span-aligned, else _end_p + 1.
                // Reading past it would overrun the allocation.
                const span_type * const end_span_p =
                    _end_p + (_end_local != size_type{0});
                do {
                    // Exhaustion leaves the cursor at end_span_p with a zero
                    // offset, which the sentinel test below accepts.
                    if(++_p >= end_span_p) [[unlikely]]
                        return *this;
                } while(*_p == span_type{0});
                shifted = *_p;
            }
            _local_index += static_cast<size_type>(std::countr_zero(shifted));
            return *this;
        }
        constexpr filter_iterator operator++(int) noexcept {
            filter_iterator tmp = *this;
            ++*this;
            return tmp;
        }
        // Position equality between iterators of the same filter range;
        // comparing across ranges is undefined, as for any two containers'
        // iterators.
        [[nodiscard]] constexpr friend bool operator==(
            const filter_iterator & x, const filter_iterator & y) noexcept {
            assert(x._data == y._data);
            return x._p == y._p && x._local_index == y._local_index;
        }
        // The end test the scan itself uses: at or past the end key. The
        // cursor may legitimately sit beyond it -- on a set bit past the end
        // key inside the last span, or on end_span_p after exhaustion.
        [[nodiscard]] constexpr friend bool operator==(
            const filter_iterator & x, std::default_sentinel_t) noexcept {
            return x._p > x._end_p ||
                   (x._p == x._end_p && x._local_index >= x._end_local);
        }
    };

    template <std::ranges::viewable_range R>
    [[nodiscard]] auto filter(R && r) const {
        // remove_cvref_t, not R: R is a forwarding reference's deduced type,
        // so it is iota_view<K, K> & for an lvalue argument and the
        // bit-scanning fast path below would never match one.
        using range_type = std::remove_cvref_t<R>;
        // Any common iota_view over an integral type, not iota_view<K, K>
        // exactly: the near-miss iota(0, n) -- int literals against a map
        // keyed by an unsigned type -- otherwise falls silently through to
        // the generic branch below, 10-50x slower. (views::take and
        // views::drop of an iota_view collapse back to an iota_view, so
        // clipped ranges stay on this path too.)
        if constexpr(detail::specialization_of<range_type,
                                               std::ranges::iota_view> &&
                     std::ranges::common_range<range_type> &&
                     std::integral<std::ranges::range_value_t<range_type>>) {
            // The bounds come from `*begin(r)` and ranges::distance, never
            // from `*end(r)`: recovering an iota's upper bound by
            // dereferencing its past-the-end iterator is undefined, and so is
            // dereferencing the begin of an empty one -- hence the early
            // return. ranges::distance is O(1) on a common integral iota, so
            // there is no walk to optimise back out.
            if(std::ranges::empty(r))
                return std::ranges::subrange(
                    filter_iterator(_data.get(), size_type{0}, size_type{0}),
                    std::default_sentinel);
            const auto raw_begin = *std::ranges::begin(r);
            using bound_type = std::remove_const_t<decltype(raw_begin)>;
            const auto raw_end =
                static_cast<bound_type>(raw_begin + std::ranges::distance(r));
            // Clamp both bounds into [0, _size] before they become span
            // indices -- in the iota's own type first, so a negative signed
            // bound never reaches the unsigned cast.
            const size_type end_index =
                raw_end <= bound_type{0}
                    ? size_type{0}
                    : std::min(static_cast<size_type>(raw_end), _size);
            const size_type begin_index =
                raw_begin <= bound_type{0}
                    ? size_type{0}
                    : std::min(static_cast<size_type>(raw_begin), end_index);

            filter_iterator first(_data.get(), begin_index, end_index);
            // The iterator's increment scans from the key *after* its
            // current one, so the first set key must be found here: advance
            // iff the begin bit itself is off. An empty range (begin ==
            // end) dereferences nothing.
            if(begin_index < end_index && !operator[](begin_index)) ++first;
            return std::ranges::subrange(first, std::default_sentinel);
        } else {
            // forward, not r: passed on as an lvalue, an rvalue container
            // becomes a ref_view of a temporary dead at the semicolon;
            // forwarding hands ownership to an owning_view instead.
            return std::views::filter(
                std::views::transform(
                    std::forward<R>(r),
                    [](auto && i) { return static_cast<K>(i); }),
                [this](const auto & k) {
                    return operator[](static_cast<size_type>(k));
                });
        }
    }
};

}  // namespace melon
