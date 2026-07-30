#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/utility/erdos_renyi.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// the arc count follows the density parameter: none at 0, all at 1, and a
// plausible fraction in between
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(erdos_renyi, test) {
    auto random_graph1 = erdos_renyi<static_digraph>(100, 0.0);
    ASSERT_EQ(random_graph1.num_vertices(), 100);
    ASSERT_EQ(random_graph1.num_arcs(), 0);

    auto random_graph2 = erdos_renyi<static_digraph>(100, 1.0);
    ASSERT_EQ(random_graph2.num_vertices(), 100);
    ASSERT_EQ(random_graph2.num_arcs(), 9900);

    auto random_graph3 = erdos_renyi<static_digraph>(100, 0.5);
    ASSERT_EQ(random_graph3.num_vertices(), 100);
    // the probability of the two following tests to fail is 2*10^-60,
    // a billion times more than the number of atoms on Earth...
    ASSERT_TRUE(random_graph3.num_arcs() > 1000);
    ASSERT_TRUE(random_graph3.num_arcs() < 8900);
}

////////////////////////////////////////////////////////////////////////////////
// regression (2.8): the generator is seedable, not an unseedable function
// static
////////////////////////////////////////////////////////////////////////////////

// The engine and the distribution were function-local `static`s, so the graph
// could not be reproduced (no way to seed) and two threads calling
// erdos_renyi() raced on both -- a distribution carries its own state, so even
// the distribution was shared mutable state. There is now an overload taking
// the generator by reference; the two-argument one seeds a local engine.
GTEST_TEST(erdos_renyi, same_seed_gives_the_same_graph) {
    std::mt19937 gen1(20260729);
    std::mt19937 gen2(20260729);

    auto a = erdos_renyi<static_digraph>(60, 0.5, gen1);
    auto b = erdos_renyi<static_digraph>(60, 0.5, gen2);

    ASSERT_EQ(num_arcs(a), num_arcs(b));
    ASSERT_TRUE(EQ_RANGES(arcs_entries(a), arcs_entries(b)));

    // a different seed gives a different graph -- otherwise the test above
    // would pass for a generator that ignores its seed
    std::mt19937 gen3(1);
    auto c = erdos_renyi<static_digraph>(60, 0.5, gen3);
    ASSERT_FALSE(EQ_RANGES(arcs_entries(a), arcs_entries(c)));
}

// the two-argument overload still works and still varies between calls
GTEST_TEST(erdos_renyi, unseeded_overload_still_varies) {
    auto a = erdos_renyi<static_digraph>(60, 0.5);
    auto b = erdos_renyi<static_digraph>(60, 0.5);
    ASSERT_FALSE(EQ_RANGES(arcs_entries(a), arcs_entries(b)));
}
