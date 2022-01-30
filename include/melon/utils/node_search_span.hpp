#ifndef MELON_NODE_SEARCH_SPAN_HPP
#define MELON_NODE_SEARCH_SPAN_HPP

#include <concepts>
#include <iterator>

namespace fhamonic {
namespace melon {

template <typename Algo>
concept node_search_algorithm = requires(Algo alg) {
    { alg.emptyQueue() } -> std::convertible_to<bool>;
    { alg.processNextNode() } -> std::default_initializable;
};

template <typename Algo>
requires node_search_algorithm<Algo>
struct node_search_span {
    struct end_iterator {};
    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = decltype(std::declval<Algo>().processNextNode());
        using reference = value_type const &;
        using pointer = value_type *;

        iterator(Algo & alg) : algorithm(alg) {}
        iterator & operator++() noexcept {
            node = algorithm.processNextNode();
            return *this;
        }
        friend bool operator==(const iterator & it, end_iterator) noexcept {
            return it.algorithm.emptyQueue();
        }
        reference operator*() const noexcept { return node; }

    private:
        Algo & algorithm;

    public:
        value_type node;
    };

    iterator begin() {
        iterator it{algorithm};
        if(!algorithm.emptyQueue()) ++it;
        return ++it;
    }
    end_iterator end() noexcept { return {}; }

    node_search_span(node_search_span const &) = delete;

public:
    explicit node_search_span(Algo & alg) : algorithm(alg) {}
    Algo & algorithm;
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_NODE_SEARCH_SPAN_HPP