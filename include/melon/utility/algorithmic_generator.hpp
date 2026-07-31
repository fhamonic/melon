#pragma once

#include <concepts>
#include <iterator>
#include <ranges>
#include <type_traits>

namespace melon {

// clang-format off
template <typename A>
concept algorithmic_generator = requires(A & alg) {
    { alg.finished() } -> std::convertible_to<bool>;
    alg.current();
    alg.advance();
};
// clang-format on

// declval<A &>, not declval<A &&>: algorithm_iterator below calls current()
// through an A *, i.e. on an lvalue, and the concept above probes one too.
// Measuring an rvalue call would name a different overload the moment some
// current() stops being const-qualified.
template <typename A>
    requires algorithmic_generator<A>
using traversal_entry_t = std::decay_t<decltype(std::declval<A &>().current())>;

template <algorithmic_generator A>
class algorithm_iterator {
private:
    A * _algorithm;

public:
    // iterator_concept, not iterator_category: the category promises the
    // Cpp17InputIterator operations, and `*it++` is ill-formed here (P0541
    // post-increment returns void). Advertising the concept keeps
    // std::input_iterator satisfied while std::iterator_traits stays empty,
    // so a pre-ranges algorithm rejects this iterator at its constraint
    // instead of failing mid-instantiation.
    using iterator_concept = std::input_iterator_tag;
    using value_type = traversal_entry_t<A>;
    // operator* below returns a prvalue and there is no operator->, so
    // anything else here would have std::iterator_traits report a reference
    // and a pointer this iterator never produces.
    using reference = value_type;
    using pointer = void;
    using difference_type = std::ptrdiff_t;

    algorithm_iterator(const algorithm_iterator &) = default;
    algorithm_iterator(algorithm_iterator &&) = default;

    algorithm_iterator & operator=(const algorithm_iterator &) = default;
    algorithm_iterator & operator=(algorithm_iterator &&) = default;

    explicit algorithm_iterator(A & alg) : _algorithm(&alg) {}
    algorithm_iterator & operator++() {
        _algorithm->advance();
        return *this;
    }
    // P0541 : post-increment on input iterators returns void
    // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0541r0.html
    void operator++(int) { operator++(); }
    friend bool operator==(const algorithm_iterator & it,
                           std::default_sentinel_t) noexcept {
        return it._algorithm->finished();
    }
    value_type operator*() const { return _algorithm->current(); }
};

// Not std::ranges::view_interface: deriving from it sets enable_view, and an
// algorithm carries O(n) state, so `alg | std::views::take(3)` on an lvalue
// would copy the heap and every vertex map, run on the copy, and leave the
// original unconsumed. As a plain range, adaptors wrap a ref_view around an
// lvalue and move an rvalue, which is what a caller means. view_interface
// offers nothing in exchange: empty(), front() and operator bool all require
// forward_range, and these ranges are input-only.
template <typename T>
class algorithm_view_interface {
public:
    [[nodiscard]] constexpr auto begin() noexcept {
        return algorithm_iterator(*static_cast<T *>(this));
    }
    [[nodiscard]] constexpr auto end() noexcept {
        return std::default_sentinel;
    }

    constexpr T & run() {
        T & self = static_cast<T &>(*this);
        while(!self.finished()) self.advance();
        return self;
    }
};

// The family contract every melon algorithm object models. The semantics the
// names carry, none of which the signatures state:
//   - reset() restores exactly the state the constructor leaves behind: blank
//     for an algorithm whose sources are added afterwards, re-seeded and
//     immediately runnable for one whose constructor seeds (topological_sort,
//     traversal_forest). `alg.reset()` is always equivalent to constructing a
//     fresh object from the same arguments.
//   - run() drains: finished() holds afterwards, and calling it again is a
//     no-op. Results stay readable through the class's accessors.
//   - current()/advance() require !finished(), asserted in debug builds.
//   - finished() is answerable from a const object.
// There is no post-construction, pre-iteration step: a constructed (and, for
// rooted algorithms, sourced) object is ready to iterate.
//
// Algorithms are move-only, and std::movable here is the whole relocation
// contract: `auto alg2 = alg;` is a compile error in every class rather than a
// silent duplication of the entire search state -- every vertex map, the heap,
// the cached cursors. Do not restore copying per class: the copy members it
// takes are a standing source of dangling cursors, and of a trait that answers
// copyable for some graph types and not others. Moving a partially consumed
// algorithm stays supported, and rebases the cursors its iterators hold.
template <typename A>
concept traversal_algorithm =
    std::ranges::input_range<A> && algorithmic_generator<A> &&
    std::movable<A> && requires(A & alg, const A & calg) {
        { calg.finished() } -> std::convertible_to<bool>;
        { alg.reset() } -> std::same_as<A &>;
        { alg.run() } -> std::same_as<A &>;
    };

// The add_source() half of the family. Precondition, asserted: the vertex is
// not already reached -- re-adding a settled vertex would re-process it and
// silently corrupt stored paths and distances.
template <typename A, typename S>
concept rooted_traversal_algorithm =
    traversal_algorithm<A> && requires(A & alg, const S & s) {
        { alg.add_source(s) } -> std::same_as<A &>;
    };

}  // namespace melon
