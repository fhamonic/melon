#pragma once

#include <concepts>
#include <iterator>
#include <ranges>
#include <type_traits>

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
using traversal_entry_t =
    std::decay_t<decltype(std::declval<A &&>().current())>;

template <algorithmic_generator A>
class algorithm_iterator {
private:
    // std::reference_wrapper<A> algorithm;
    A * algorithm;

public:
    using iterator_category = std::input_iterator_tag;
    using value_type = traversal_entry_t<A>;
    // operator* below returns a prvalue and there is no operator->, so
    // std::iterator_traits used to report a reference and a pointer that this
    // iterator never produces.
    using reference = value_type;
    using pointer = void;
    using difference_type = std::ptrdiff_t;

    algorithm_iterator(const algorithm_iterator &) = default;
    algorithm_iterator(algorithm_iterator &&) = default;

    algorithm_iterator & operator=(const algorithm_iterator &) = default;
    algorithm_iterator & operator=(algorithm_iterator &&) = default;

    explicit algorithm_iterator(A & alg) : algorithm(&alg) {}
    // Forwards straight into the algorithm's advance(), which may allocate
    // and run user code, so it cannot promise noexcept.
    algorithm_iterator & operator++() {
        algorithm->advance();
        return *this;
    }
    // P0541 : post-increment on input iterators returns void
    // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0541r0.html
    void operator++(int) { operator++(); }
    friend bool operator==(const algorithm_iterator & it,
                           std::default_sentinel_t) noexcept {
        // return it.algorithm.get().finished();
        return it.algorithm->finished();
    }
    value_type operator*() const { return algorithm->current(); }
};

template <typename T>
class algorithm_view_interface : public std::ranges::view_interface<T> {
public:
    [[nodiscard]] constexpr auto begin() noexcept {
        return algorithm_iterator(*static_cast<T *>(this));
    }
    [[nodiscard]] constexpr auto end() noexcept {
        return std::default_sentinel;
    }
};

}  // namespace melon
