#undef NDEBUG
#include <gtest/gtest.h>

#include <limits>

#include "melon/numeric/semiring.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// every shipped semiring models the semiring concept
////////////////////////////////////////////////////////////////////////////////

static_assert(semiring<shortest_path_semiring<int>>);
static_assert(semiring<shortest_path_semiring<double>>);
static_assert(semiring<most_reliable_path_semiring<double>>);
static_assert(semiring<max_capacity_path_semiring<int>>);
static_assert(semiring<minimum_spanning_tree_semiring<int>>);

// plus and less are probed on const values: every relaxation passes const
// lvalues and prvalues, so a plus_t invocable only on mutable lvalues would
// otherwise model the concept and fail at every call site.
namespace {
struct mutable_only_plus {
    using value_type = int;
    struct plus_t {
        int operator()(int &, int &) const;
    };
    struct less_t {
        bool operator()(const int &, const int &) const;
    };
    static constexpr value_type zero = 0;
    static constexpr value_type infty = 1;
    static constexpr plus_t plus{};
    static constexpr less_t less{};
};
}  // namespace
static_assert(!semiring<mutable_only_plus>);

////////////////////////////////////////////////////////////////////////////////
// infty_is_absorbing is a promise plus must keep, and absence means false
////////////////////////////////////////////////////////////////////////////////

// bellman_ford drops its unreached-vertex guard wherever this is true, so a
// semiring may only declare it when plus(infty, x) == infty for every
// producible x -- which no integer plus satisfies.
static_assert(has_absorbing_infty<shortest_path_semiring<double>>);
static_assert(!has_absorbing_infty<shortest_path_semiring<int>>);
static_assert(has_absorbing_infty<most_reliable_path_semiring<double>>);
static_assert(has_absorbing_infty<max_capacity_path_semiring<int>>);
// No flag declared at all: plus keeps the last arc's length, absorbing
// nothing.
static_assert(!has_absorbing_infty<minimum_spanning_tree_semiring<int>>);

using S_dbl = shortest_path_semiring<double>;
static_assert(S_dbl::infty == std::numeric_limits<double>::infinity());
// The negative operand is the case the guard exists for: with a max()
// sentinel, max() + negative compares less than max() and an unreached
// vertex would relax as reached.
static_assert(S_dbl::plus(S_dbl::infty, -1e308) == S_dbl::infty);
static_assert(!S_dbl::less(S_dbl::plus(S_dbl::infty, -1e308), S_dbl::infty));

////////////////////////////////////////////////////////////////////////////////
// the concept rejects a type missing any of the four members
////////////////////////////////////////////////////////////////////////////////

struct no_less {
    using value_type = int;
    using plus_t = std::plus<int>;
    static constexpr int zero = 0;
    static constexpr int infty = 1;
    static constexpr plus_t plus{};
};
static_assert(!semiring<no_less>);
static_assert(!semiring<int>);

////////////////////////////////////////////////////////////////////////////////
// zero is neutral for plus, and infty absorbing under less
////////////////////////////////////////////////////////////////////////////////

// zero is the neutral element of plus, and infty the absorbing one under less:
// no reachable path length may ever compare less than zero, nor infty less than
// anything. These are the two properties every melon shortest-path-like
// algorithm relies on when it initialises its distance map.
template <typename S>
constexpr bool is_neutral_and_absorbing(typename S::value_type v) {
    return S::plus(S::zero, v) == v && S::plus(v, S::zero) == v &&
           !S::less(v, S::zero) && !S::less(S::infty, v);
}

////////////////////////////////////////////////////////////////////////////////
// each semiring encodes its own optimisation criterion
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(shortest_path_semiring, sums_lengths_and_takes_the_smallest) {
    using S = shortest_path_semiring<int>;

    ASSERT_EQ(S::zero, 0);
    ASSERT_EQ(S::infty, std::numeric_limits<int>::max());
    ASSERT_EQ(S::plus(3, 4), 7);
    ASSERT_TRUE(S::less(3, 4));
    ASSERT_FALSE(S::less(4, 3));
    ASSERT_FALSE(S::less(3, 3));

    static_assert(is_neutral_and_absorbing<S>(5));
}

GTEST_TEST(most_reliable_path_semiring, multiplies_probabilities) {
    using S = most_reliable_path_semiring<double>;

    ASSERT_EQ(S::zero, 1.0);
    ASSERT_EQ(S::infty, 0.0);
    ASSERT_DOUBLE_EQ(S::plus(0.5, 0.4), 0.2);
    // A more reliable path is a "shorter" one.
    ASSERT_TRUE(S::less(0.9, 0.5));
    ASSERT_FALSE(S::less(0.5, 0.9));

    static_assert(is_neutral_and_absorbing<S>(0.25));
}

GTEST_TEST(max_capacity_path_semiring, keeps_the_bottleneck) {
    using S = max_capacity_path_semiring<int>;

    ASSERT_EQ(S::zero, std::numeric_limits<int>::max());
    ASSERT_EQ(S::infty, 0);
    ASSERT_EQ(S::plus(7, 3), 3);
    ASSERT_EQ(S::plus(3, 7), 3);
    // A larger capacity is a "shorter" path.
    ASSERT_TRUE(S::less(7, 3));
    ASSERT_FALSE(S::less(3, 7));

    static_assert(is_neutral_and_absorbing<S>(42));
}

// Not an actual semiring — plus discards the accumulated length — but it is
// what turns the dijkstra loop into Prim's algorithm.
GTEST_TEST(minimum_spanning_tree_semiring, keeps_only_the_last_length) {
    using S = minimum_spanning_tree_semiring<int>;

    ASSERT_EQ(S::zero, 0);
    ASSERT_EQ(S::infty, std::numeric_limits<int>::max());
    ASSERT_EQ(S::plus(7, 3), 3);
    ASSERT_EQ(S::plus(3, 7), 7);
    ASSERT_TRUE(S::less(3, 7));
}
