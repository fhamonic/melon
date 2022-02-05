#ifndef MELON_TRAVERSAL_ALGORITHM_ITERATOR_HPP
#define MELON_TRAVERSAL_ALGORITHM_ITERATOR_HPP

#include <concepts>
#include <iterator>

namespace fhamonic {
namespace melon {

template <typename Algo>
concept traversal_algorithm = requires(Algo alg) {
    { alg.empty_queue() } -> std::convertible_to<bool>;
    { alg.next_node() } -> std::default_initializable;
};

struct traversal_algorithm_end_iterator {};

template <typename Algo>
requires traversal_algorithm<Algo>
class traversal_algorithm_iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = decltype(std::declval<Algo>().next_node());
    using reference = value_type const &;
    using pointer = value_type *;
    using difference_type = void;

    traversal_algorithm_iterator(Algo & alg) : algorithm(alg) {
        if(!algorithm.empty_queue()) ++(*this);
    }
    traversal_algorithm_iterator & operator++() noexcept {
        node = algorithm.next_node();
        return *this;
    }
    friend bool operator==(const traversal_algorithm_iterator & it,
                           traversal_algorithm_end_iterator) noexcept {
        return it.algorithm.empty_queue();
    }
    reference operator*() const noexcept { return node; }

private:
    Algo & algorithm;

public:
    value_type node;
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_TRAVERSAL_ALGORITHM_ITERATOR_HPP