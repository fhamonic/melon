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
// Measuring an rvalue call here is the same drift mapped_reference_t had --
// harmless while every current() is const-qualified, wrong the moment one is
// not.
template <typename A>
    requires algorithmic_generator<A>
using traversal_entry_t = std::decay_t<decltype(std::declval<A &>().current())>;

template <algorithmic_generator A>
class algorithm_iterator {
private:
    // std::reference_wrapper<A> algorithm;
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
    // std::iterator_traits used to report a reference and a pointer that this
    // iterator never produces.
    using reference = value_type;
    using pointer = void;
    using difference_type = std::ptrdiff_t;

    algorithm_iterator(const algorithm_iterator &) = default;
    algorithm_iterator(algorithm_iterator &&) = default;

    algorithm_iterator & operator=(const algorithm_iterator &) = default;
    algorithm_iterator & operator=(algorithm_iterator &&) = default;

    explicit algorithm_iterator(A & alg) : _algorithm(&alg) {}
    // Forwards straight into the algorithm's advance(), which may allocate
    // and run user code, so it cannot promise noexcept.
    algorithm_iterator & operator++() {
        _algorithm->advance();
        return *this;
    }
    // P0541 : post-increment on input iterators returns void
    // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0541r0.html
    void operator++(int) { operator++(); }
    friend bool operator==(const algorithm_iterator & it,
                           std::default_sentinel_t) noexcept {
        // return it.algorithm.get().finished();
        return it._algorithm->finished();
    }
    value_type operator*() const { return _algorithm->current(); }
};

// Not std::ranges::view_interface: deriving from it made enable_view true,
// and a copyable algorithm then modelled std::ranges::view while carrying
// O(n) state -- so `alg | std::views::take(3)` on an lvalue deep-copied the
// heap and every vertex map, ran on the copy, and left the original
// unconsumed. As a plain range, adaptors wrap a ref_view around an lvalue
// and move an rvalue, which is what a caller means. view_interface bought
// nothing in exchange: empty(), front() and operator bool all require
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

    // `while(!finished()) advance();` was written out identically in eleven
    // algorithms -- and left out of kruskal, which inherits this interface but
    // had no run() at all. It is the definition of what an algorithmic
    // generator's run *is*, so it belongs with finished() and advance() rather
    // than being retyped beside each pair. Not noexcept: advance() allocates
    // and runs the caller's maps.
    //
    // The algorithms whose iteration is internal keep their own run() with
    // the same shape -- return the algorithm, results through accessors:
    // dinitz and edmonds_karp are not generators at all, and
    // bidirectional_dijkstra is a point query whose answer dist() reads
    // after run().
    constexpr T & run() {
        T & self = static_cast<T &>(*this);
        while(!self.finished()) self.advance();
        return self;
    }
};

// The family contract every melon algorithm object models, stated once so it
// cannot drift per class. The semantics the names carry:
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
// contract. Copying was supported until it was measured: nothing in the
// library, the tests, the docs or the examples ever copied an algorithm for a
// purpose -- the only internal copy was traversal_forest copying its own
// breadth_first_search inside traversal_forest's own copy constructor -- while
// the per-class copy members it required were a standing source of
// dangling-cursor and lying-trait bugs. Worse, it could not be *stated*: over
// one identical graph type (a subgraph of a ref to static_digraph) dijkstra,
// breadth_first_search and topological_sort were copyable and
// depth_first_search and strongly_connected_components were not, and
// depth_first_search flipped its answer depending on whether the container
// underneath handed out std-borrowed incidence ranges. A capability whose
// availability rule a user cannot hold in their head is worse than no
// capability. Copying also carries the whole search state -- every vertex map,
// the heap, the cached cursors -- so making it a compile error is the honest
// cost signal. Relocation of a *moved* algorithm remains fully supported and is
// what the cursor rebasing in detail/consumable_view.hpp exists for.
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
