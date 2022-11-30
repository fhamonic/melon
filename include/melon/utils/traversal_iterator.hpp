#ifndef MELON_UTILS_TRAVERSAL_ITERATOR_HPP
#define MELON_UTILS_TRAVERSAL_ITERATOR_HPP

#include <concepts>
#include <iterator>

#include "melon/concepts/traversal.hpp"

namespace fhamonic {
namespace melon {

struct traversal_end_sentinel {};

template <typename A>
requires concepts::traversal_algorithm<A> &&
    std::default_initializable<typename A::traversal_entry>
class traversal_iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = typename A::traversal_entry;
    using reference = value_type const &;
    using pointer = value_type *;
    using difference_type = void;

    explicit traversal_iterator(A & alg) : algorithm(alg) {
        if(!algorithm.empty_queue()) ++(*this);
    }
    traversal_iterator & operator++() noexcept {
        node = algorithm.next_entry();
        return *this;
    }
    friend bool operator==(const traversal_iterator & it,
                           traversal_end_sentinel) noexcept {
        return it.algorithm.empty_queue();
    }
    reference operator*() const noexcept { return node; }

private:
    A & algorithm;

public:
    value_type node;
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILS_TRAVERSAL_ITERATOR_HPP