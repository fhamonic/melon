#ifndef MELON_UTILS_TRAVERSAL_ITERATOR_HPP
#define MELON_UTILS_TRAVERSAL_ITERATOR_HPP

#include <concepts>
#include <iterator>

#include "melon/concepts/traversal.hpp"

namespace fhamonic {
namespace melon {

struct traversal_end_sentinel {};

template <concepts::traversal_algorithm A>
class traversal_iterator {
private:
    std::reference_wrapper<A> algorithm;

public:
    using iterator_category = std::input_iterator_tag;
    using value_type = traversal_entry_t<A>;
    using reference = value_type const &;
    using pointer = value_type *;
    using difference_type = std::ptrdiff_t;

    traversal_iterator(const traversal_iterator &) = default;
    traversal_iterator(traversal_iterator &&) = default;

    traversal_iterator & operator=(const traversal_iterator &) = default;
    traversal_iterator & operator=(traversal_iterator &&) = default;

    explicit traversal_iterator(A & alg) : algorithm(alg) {}
    traversal_iterator & operator++() noexcept {
        algorithm.get().advance();
        return *this;
    }
    // P0541 : post-increment on input iterators
    // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0541r0.html
    void operator++(int) noexcept { operator++(); }
    friend bool operator==(const traversal_iterator & it,
                           traversal_end_sentinel) noexcept {
        return it.algorithm.get().finished();
    }
    value_type operator*() const noexcept { return algorithm.get().current(); }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILS_TRAVERSAL_ITERATOR_HPP