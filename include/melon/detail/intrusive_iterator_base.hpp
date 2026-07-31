#pragma once

#include <cassert>
#include <iterator>

namespace melon {

template <typename S, typename T>
class intrusive_iterator_base {
public:
    // operator*() returns a prvalue, which Cpp17ForwardIterator forbids
    // (*it must be reference); the C++20 concept has no such requirement.
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type = std::decay_t<T>;
    using reference = T;
    using pointer = void;
    using difference_type = std::ptrdiff_t;

protected:
    const S * _structure;
    T _cursor;

public:
    constexpr intrusive_iterator_base(const S * struct_ptr, const T & first)
        : _structure(struct_ptr), _cursor(first) {}

    constexpr intrusive_iterator_base() = default;
    constexpr intrusive_iterator_base(intrusive_iterator_base &&) = default;
    constexpr intrusive_iterator_base(const intrusive_iterator_base &) =
        default;

    constexpr intrusive_iterator_base & operator=(intrusive_iterator_base &&) =
        default;
    constexpr intrusive_iterator_base & operator=(
        const intrusive_iterator_base &) = default;

    constexpr T operator*() const { return _cursor; }

    [[nodiscard]] constexpr friend bool operator==(
        const intrusive_iterator_base & it1,
        const intrusive_iterator_base & it2) noexcept {
        assert(it1._structure == it2._structure);
        return it1._cursor == it2._cursor;
    }
};

}  // namespace melon
