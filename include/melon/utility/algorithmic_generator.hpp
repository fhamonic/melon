#ifndef MELON_UTILITY_ALGORITHM_GENERATOR_HPP
#define MELON_UTILITY_ALGORITHM_GENERATOR_HPP

#include <concepts>
#include <iterator>
#include <type_traits>

namespace fhamonic {
namespace melon {

// clang-format off
template <typename A>
concept algorithmic_generator = requires(A alg) {
    { alg.finished() } -> std::convertible_to<bool>;
    alg.current();
    alg.advance();
};
// clang-format on

template <typename A>
    requires algorithmic_generator<A>
using traversal_entry_t = std::decay_t<decltype(std::declval<A &&>().current())>;


struct algorithm_end_sentinel {};

template <algorithmic_generator A>
class algorithm_iterator {
private:
    std::reference_wrapper<A> algorithm;

public:
    using iterator_category = std::input_iterator_tag;
    using value_type = traversal_entry_t<A>;
    using reference = value_type const &;
    using pointer = value_type *;
    using difference_type = std::ptrdiff_t;

    algorithm_iterator(const algorithm_iterator &) = default;
    algorithm_iterator(algorithm_iterator &&) = default;

    algorithm_iterator & operator=(const algorithm_iterator &) = default;
    algorithm_iterator & operator=(algorithm_iterator &&) = default;

    explicit algorithm_iterator(A & alg) : algorithm(alg) {}
    algorithm_iterator & operator++() noexcept {
        algorithm.get().advance();
        return *this;
    }
    // P0541 : post-increment on input iterators returns void
    // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0541r0.html
    void operator++(int) noexcept { operator++(); }
    friend bool operator==(const algorithm_iterator & it,
                           algorithm_end_sentinel) noexcept {
        return it.algorithm.get().finished();
    }
    value_type operator*() const noexcept { return algorithm.get().current(); }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILITY_ALGORITHM_GENERATOR_HPP