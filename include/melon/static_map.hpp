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
        : _data(std::make_unique<value_type[]>(size))
        , _data_end(_data.get() + size){};

    StaticMap(size_type size, value_type init_value) : StaticMap(size) {
        std::ranges::fill(*this, init_value);
    };

    StaticMap(const StaticMap & other) : StaticMap(other.size()) {
        std::ranges::copy(other, _data);
    };
    StaticMap(StaticMap &&) = default;

    StaticMap & operator=(const StaticMap & other) {
        resize(other.size());
        std::ranges::copy(other, _data);
    };
    StaticMap & operator=(StaticMap &&) = default;

    iterator begin() noexcept { return _data.get(); }
    iterator end() noexcept { return _data_end; }
    const_iterator cbegin() const noexcept { return _data.get(); }
    const_iterator cend() const noexcept { return _data_end; }

    size_type size() const noexcept { return std::distance(cbegin(), cend()); }
    void resize(size_type n) {
        if(n == size()) return;
        _data = std::make_unique<value_type[]>(n);
        _data_end = _data.get() + n;
    }

    reference operator[](size_type i) noexcept {
        assert(0 <= i && i < size());
        return _data[i];
    }
    const_reference operator[](size_type i) const noexcept {
        assert(0 <= i && i < size());
        return _data[i];
    }
};

#include <bit>

template <>
class StaticMap<bool> {
public:
    using value_type = bool;
    using size_type = std::size_t;

private:
    using span_type = std::size_t;
    static_assert(std::is_unsigned_v<span_type>);
    static constexpr size_type N = sizeof(span_type) << 3;
    static constexpr size_type span_index_mask = N - 1;
    static constexpr size_type nb_spans(std::size_t n) {
        return (n + N - 1) / N;
    }

public:
    class reference {
    private:
        span_type * _p;
        span_type _index;

    public:
        reference(span_type * p, span_type index) : _p(p), _index(index) {}

        operator bool() const { return (*_p >> _index) & 1; }
        reference & operator=(bool b) {
            // if(b)
            //     *_p |= (1 << _index);
            // else
            //     *_p &= ~(1 << _index);
            *_p ^= (((*_p >> _index) & 1) & b) << _index;
            return *this;
        }
    };
    using const_reference = const reference;
    // using iterator = T *;
    // using const_iterator = const T *;

private:
    std::unique_ptr<span_type[]> _data;
    size_type _size;

public:
    StaticMap() : _data(nullptr), _size(0){};
    StaticMap(size_type size)
        : _data(std::make_unique<span_type[]>(nb_spans(size))), _size(size){};

    StaticMap(size_type size, bool init_value) : StaticMap(size) {
        std::fill(_data.get(), _data.get() + nb_spans(size),
                  init_value ? ~span_type(0) : span_type(0));
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
    };
    StaticMap & operator=(StaticMap &&) = default;

    // iterator begin() noexcept { return _data.get(); }
    // iterator end() noexcept { return _data_end; }
    // const_iterator cbegin() const noexcept { return _data.get(); }
    // const_iterator cend() const noexcept { return _data_end; }

    size_type size() const noexcept { return _size; }
    void resize(size_type n) {
        if(n == _size) return;
        _data = std::make_unique<span_type[]>(nb_spans(n));
        _size = n;
    }

    reference operator[](size_type i) noexcept {
        assert(0 <= i && i < size());
        return reference(_data.get() + i / N,
                         static_cast<span_type>(i & span_index_mask));
    }
    // const_reference operator[](size_type i) const noexcept {
    //     assert(0 <= i && i < size());
    //     return _data[i];
    // }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STATIC_MAP_HPP