#ifndef MELON_TRAVERSAL_ITERATOR_HPP
#define MELON_TRAVERSAL_ITERATOR_HPP

#include <concepts>
#include <iterator>

namespace fhamonic {
namespace melon {

template <typename ALG>
concept traversal_algorithm = requires(ALG alg) {
    { alg.empty_queue() }
    ->std::convertible_to<bool>;
    { alg.next_vertex() }
    ->std::default_initializable;
};

struct traversal_end_sentinel {};

template <typename ALG>
requires traversal_algorithm<ALG> class traversal_iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = decltype(std::declval<ALG>().next_vertex());
    using reference = value_type const &;
    using pointer = value_type *;
    using difference_type = void;

    traversal_iterator(ALG & alg) : algorithm(alg) {
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
    ALG & algorithm;

public:
    value_type node;
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_TRAVERSAL_ITERATOR_HPP