#ifndef MELON_TRAVERSAL_ITERATOR_HPP
#define MELON_TRAVERSAL_ITERATOR_HPP

#include <concepts>
#include <iterator>

namespace fhamonic {
namespace melon {

template <typename A>
concept traversal_algorithm = requires(A alg) {
    { alg.empty_queue() }
    ->std::convertible_to<bool>;
    { alg.next_vertex() }
    -> std::default_initializable;
};

struct traversal_end_sentinel {};

template <typename A>
requires traversal_algorithm<A> class traversal_iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = decltype(std::declval<A>().next_vertex());
    using reference = value_type const &;
    using pointer = value_type *;
    using difference_type = void;

    traversal_iterator(A & alg) : algorithm(alg) {
        if(!algorithm.empty_queue()) ++(*this);
    }
    traversal_iterator & operator++() noexcept {
        node = algorithm.next_vertex();
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

#endif  // MELON_TRAVERSAL_ITERATOR_HPP