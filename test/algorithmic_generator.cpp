#undef NDEBUG
#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <vector>

#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// a resumable computation exposing finished()/current()/advance() models
// algorithmic_generator, and each of the three members is load-bearing
////////////////////////////////////////////////////////////////////////////////

// The minimal thing the concept describes: a resumable computation exposing
// its state through finished()/current()/advance(). Deriving from
// algorithm_view_interface is what turns it into a range.
class squares : public algorithm_view_interface<squares> {
private:
    int _bound;
    int _i;

public:
    explicit squares(int bound) : _bound(bound), _i(0) {}

    [[nodiscard]] bool finished() const noexcept { return _i >= _bound; }
    [[nodiscard]] int current() const noexcept { return _i * _i; }
    void advance() noexcept { ++_i; }
};

static_assert(algorithmic_generator<squares>);
static_assert(std::same_as<traversal_entry_t<squares>, int>);

// The three members are all load-bearing.
struct no_advance {
    bool finished() const;
    int current() const;
};
static_assert(!algorithmic_generator<no_advance>);
static_assert(!algorithmic_generator<int>);

// Every melon traversal is meant to model it; breadth_first_search is the
// simplest of them.
using bfs = decltype(breadth_first_search(std::declval<static_digraph &>()));
static_assert(algorithmic_generator<bfs>);

////////////////////////////////////////////////////////////////////////////////
// algorithm_iterator walks the generator as a C++20 input iterator
////////////////////////////////////////////////////////////////////////////////

using iterator = algorithm_iterator<squares>;

static_assert(std::input_iterator<iterator>);
static_assert(std::sentinel_for<std::default_sentinel_t, iterator>);
static_assert(std::same_as<std::iter_value_t<iterator>, int>);
// iterator_concept, not iterator_category: post-increment returns void, so
// the Cpp17InputIterator operations the *category* promises (`*it++`) are
// not available -- the iterator is C++20-only and says so.
static_assert(
    std::same_as<typename iterator::iterator_concept, std::input_iterator_tag>);
// P0541: post-increment on an input iterator returns void.
static_assert(std::same_as<decltype(std::declval<iterator &>()++), void>);

GTEST_TEST(algorithm_iterator, walks_the_generator) {
    squares alg(4);
    iterator it(alg);

    ASSERT_FALSE(it == std::default_sentinel);
    ASSERT_EQ(*it, 0);
    ++it;
    ASSERT_EQ(*it, 1);
    it++;
    ASSERT_EQ(*it, 4);
    ++it;
    ASSERT_EQ(*it, 9);
    ++it;
    ASSERT_TRUE(it == std::default_sentinel);
}

// The iterator is a handle on the algorithm, not a copy of its state: two
// iterators on the same generator advance together.
GTEST_TEST(algorithm_iterator, refers_to_the_algorithm) {
    squares alg(3);
    iterator it1(alg);
    iterator it2 = it1;

    ++it1;
    ASSERT_EQ(*it2, 1);
    ASSERT_EQ(*it1, *it2);
}

// An already finished generator yields an empty range.
GTEST_TEST(algorithm_iterator, handles_an_empty_run) {
    squares alg(0);
    ASSERT_TRUE(iterator(alg) == std::default_sentinel);
}

////////////////////////////////////////////////////////////////////////////////
// regression: the iterator typedefs advertise only what operator* produces
////////////////////////////////////////////////////////////////////////////////

// operator* returns a prvalue and there is no operator->, but the typedefs
// advertised `value_type const &` and `value_type *`, so std::iterator_traits
// reported a reference and a pointer this iterator never produces.
namespace {
struct counting_generator {
    int i = 0;
    bool finished() const { return i >= 3; }
    int current() const { return i; }
    void advance() { ++i; }
};
using counting_iterator = algorithm_iterator<counting_generator>;
}  // namespace

static_assert(algorithmic_generator<counting_generator>);
static_assert(std::same_as<counting_iterator::value_type, int>);
static_assert(std::same_as<counting_iterator::reference, int>);
static_assert(std::same_as<counting_iterator::pointer, void>);
static_assert(
    std::same_as<counting_iterator::reference,
                 decltype(*std::declval<const counting_iterator &>())>);
static_assert(std::same_as<std::iter_reference_t<counting_iterator>,
                           counting_iterator::reference>);
// No iterator_category means std::iterator_traits stays empty: a pre-ranges
// algorithm rejects this iterator at its constraint instead of instantiating
// against a Cpp17 category the iterator cannot honor (`*it++` is void).
template <typename T>
constexpr bool advertises_cpp17_category =
    requires { typename std::iterator_traits<T>::iterator_category; };
static_assert(!advertises_cpp17_category<counting_iterator>);
static_assert(std::input_iterator<counting_iterator>);

////////////////////////////////////////////////////////////////////////////////
// algorithm_view_interface makes the generator a consumable input range --
// deliberately a range, never a std::ranges::view
////////////////////////////////////////////////////////////////////////////////

static_assert(std::ranges::input_range<squares>);
// A range but deliberately NOT a std::ranges::view: an algorithm carries
// O(n) state, so modelling view (which algorithm_view_interface's old
// view_interface base opted it into) made `alg | std::views::take(3)` on an
// lvalue deep-copy the whole algorithm and run on the copy. As a plain
// range, adaptors wrap a ref_view around an lvalue instead.
static_assert(!std::ranges::view<squares>);
static_assert(!std::ranges::enable_view<squares>);

GTEST_TEST(algorithm_view_interface, makes_the_generator_a_range) {
    squares alg(5);

    std::vector<int> collected;
    for(const int s : alg) collected.push_back(s);
    ASSERT_EQ(collected, (std::vector<int>{0, 1, 4, 9, 16}));

    // The run is consumed: a second pass over the same object yields nothing.
    ASSERT_TRUE(std::ranges::begin(alg) == std::ranges::end(alg));
}

GTEST_TEST(algorithm_view_interface, composes_with_std_ranges) {
    squares alg(6);

    auto odd_squares = std::ranges::to<std::vector<int>>(std::views::filter(
        std::views::take(alg, 4), [](int s) { return s % 2 == 1; }));
    ASSERT_EQ(odd_squares, (std::vector<int>{1, 9}));
}

// And the same works on a real traversal, which is the point of the header.
GTEST_TEST(algorithm_view_interface, drives_a_real_traversal) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0u, 1u).add_arc(1u, 2u).add_arc(2u, 3u);
    auto [graph] = builder.build();

    breadth_first_search alg(graph, 0u);
    static_assert(algorithmic_generator<decltype(alg)>);

    std::vector<vertex_t<static_digraph>> visited;
    for(const auto & v : alg) visited.push_back(v);
    ASSERT_TRUE(EQ_RANGES(visited, {0, 1, 2, 3}));
}
