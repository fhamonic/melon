#ifndef MELON_STATIC_MAP_HPP
#define MELON_STATIC_MAP_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <iterator>
#include <memory>
#include <ranges>

namespace fhamonic {
namespace melon {

template <typename K, typename V>
requires std::integral<K>
class static_map {
public:
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<const K, V &>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using reference = value_type;
    using const_reference = const value_type;

    template <typename R>
    class iterator_base {
    public:
        using I = iterator_base<R>;
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = static_map<K, V>::difference_type;
        using value_type = std::pair<const K, V &>;
        using pointer = void;
        using reference = R;

    protected:
        mapped_type * _p;
        size_type _index;

    public:
        iterator_base(mapped_type * p, const size_type & index)
            : _p(p), _index(index) {}

        iterator_base() = default;
        iterator_base(const iterator_base &) = default;
        iterator_base(iterator_base &&) = default;

        iterator_base & operator=(const iterator_base &) = default;
        iterator_base & operator=(iterator_base &&) = default;

        friend bool operator==(const iterator_base & x,
                               const iterator_base & y) noexcept {
            return x._p == y._p && x._index == y._index;
        }
        friend constexpr std::strong_ordering operator<=>(
            const iterator_base & x, const iterator_base & y) noexcept {
            assert(x._p == y._p);
            return x._index <=> y._index;
        }
        difference_type operator-(const iterator_base & other) const noexcept {
            assert(_p == other._p);
            return static_cast<difference_type>(_index) -
                   static_cast<difference_type>(other._index);
        }
        I & operator++() noexcept {
            ++_index;
            return *static_cast<I *>(this);
        }
        I operator++(int) noexcept {
            I tmp = *static_cast<I *>(this);
            ++_index;
            return tmp;
        }
        I & operator--() noexcept {
            --_index;
            return *static_cast<I *>(this);
        }
        I operator--(int) noexcept {
            I tmp = *static_cast<I *>(this);
            --_index;
            return tmp;
        }
        I & operator+=(difference_type i) noexcept {
            _index += i;
            return *static_cast<I *>(this);
        }

        I & operator-=(difference_type i) noexcept {
            _index -= i;
            return *static_cast<I *>(this);
        }

        friend I operator+(const I & x, difference_type n) {
            I tmp = x;
            return tmp += n;
        }
        friend I operator+(difference_type n, const I & x) { return x + n; }
        friend I operator-(const I & x, difference_type n) {
            I tmp = x;
            return tmp -= n;
        }

        reference operator*() const noexcept {
            return reference(static_cast<key_type>(_index), _p[_index]);
        }
        reference operator[](difference_type i) const { return *(*this + i); }
    };

    // class iterator : public iterator_base<iterator> {
    //     using iterator_base<iterator>::iterator_base;
    //     using reference = std::pair<const K, V &>;

    //     reference operator*() const noexcept {
    //         return reference(static_cast<key_type>(_index), _p[_index]);
    //     }
    //     reference operator[](difference_type i) const { return *(*this + i);
    //     }
    // };

    // class const_iterator
    //     : public iterator_base<const_iterator> {
    //     using iterator_base<const_iterator>::iterator_base;
    //     using reference = std::pair<const K, V>;

    //     reference operator*() const noexcept {
    //         return reference(static_cast<key_type>(_index), _p[_index]);
    //     }
    //     reference operator[](difference_type i) const { return *(*this + i);
    //     }
    // };

    using iterator = iterator_base<std::pair<const K, V &>>;
    using const_iterator = iterator_base<std::pair<const K, V>>;

private:
    std::unique_ptr<mapped_type[]> _data;
    size_type _size;

public:
    static_map() : _data(nullptr), _size(0){};
    explicit static_map(const size_type size)
        : _data(std::make_unique_for_overwrite<mapped_type[]>(size))
        , _size(size){};

    static_map(const size_type size, const mapped_type & init_value)
        : static_map(size) {
        std::fill(_data.get(), _data.get() + _size, init_value);
    }

    template <std::random_access_iterator IT>
    static_map(IT && it_begin, IT && it_end)
        : static_map(static_cast<size_type>(std::distance(it_begin, it_end))) {
        std::copy(it_begin, it_end, _data.get());
    }
    template <std::ranges::random_access_range R>
    explicit static_map(R && r) : static_map(r.begin(), r.end()) {}
    static_map(const static_map & other)
        : static_map(other.data(), other.data() + other.size()){};
    static_map(static_map &&) = default;

    static_map & operator=(const static_map & other) {
        resize(other.size());
        std::copy(other.data(), other.data() + other.size(), _data.get());
    }
    static_map & operator=(static_map &&) = default;

    iterator begin() noexcept { return iterator(_data.get(), 0); }
    iterator end() noexcept { return iterator(_data.get(), _size); }
    const_iterator begin() const noexcept {
        return const_iterator(_data.get(), 0);
    }
    const_iterator end() const noexcept {
        return const_iterator(_data.get(), _size);
    }

    constexpr size_type size() const noexcept { return _size; }
    constexpr void resize(const size_type n) {
        if(n == size()) return;
        _data = std::make_unique_for_overwrite<mapped_type[]>(n);
        _size = n;
    }

    constexpr mapped_type & operator[](const key_type i) noexcept {
        assert(static_cast<size_type>(i) < size());
        return _data[static_cast<size_type>(i)];
    }
    constexpr mapped_type operator[](const key_type i) const noexcept {
        assert(static_cast<size_type>(i) < size());
        return _data[static_cast<size_type>(i)];
    }

    constexpr mapped_type * data() noexcept { return _data.get(); }
    constexpr mapped_type * data() const noexcept { return _data.get(); }
};

}  // namespace melon
}  // namespace fhamonic

// #include "melon/data_structures/static_map_atomic_bool.hpp"
// #include "melon/data_structures/static_map_bool.hpp"

#endif  // MELON_STATIC_MAP_HPP