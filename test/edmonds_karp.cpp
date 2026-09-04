#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/edmonds_karp.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/complete_digraph.hpp"

#include "ranges_test_helper.hpp"
#include "unsized_digraph.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// edmonds_karp computes the maximum flow value and a minimum cut
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(edmonds_karp, test) {
    static_digraph_builder<static_digraph, int, char> builder(6);

    // example from https://www.geeksforgeeks.org/max-flow-problem-introduction/
    builder.add_arc({0, 1}, 16, false);
    builder.add_arc({0, 2}, 13, false);
    builder.add_arc({1, 2}, 10, false);
    builder.add_arc({1, 3}, 12, true);  //
    builder.add_arc({2, 1}, 4, false);
    builder.add_arc({2, 4}, 14, false);
    builder.add_arc({3, 2}, 9, false);
    builder.add_arc({3, 5}, 20, false);
    builder.add_arc({4, 3}, 7, true);  //
    builder.add_arc({4, 5}, 4, true);  //

    auto [graph, capacity, part_of_minimum_cut] = builder.build();

    edmonds_karp alg(graph, capacity, 0u, 5u);
    ASSERT_EQ(alg.run().flow_value(), 23);
    ASSERT_TRUE(EQ_MULTISETS(
        alg.minimum_cut(), std::views::filter(arcs(graph), [&](const auto & a) {
            return part_of_minimum_cut[a];
        })));

    // flow()/flows_map() expose the computed flow: capacity-feasible,
    // conserved at the internal vertices, and the view agrees with the
    // per-arc accessor.
    for(auto && a : arcs(graph)) {
        ASSERT_GE(alg.flow(a), 0);
        ASSERT_LE(alg.flow(a), capacity[a]);
        ASSERT_EQ(alg.flows_map()[a], alg.flow(a));
    }
    for(auto && u : vertices(graph)) {
        if(u == 0u || u == 5u) continue;
        int in_flow = 0, out_flow = 0;
        for(auto && a : in_arcs(graph, u)) in_flow += alg.flow(a);
        for(auto && a : out_arcs(graph, u)) out_flow += alg.flow(a);
        ASSERT_EQ(in_flow, out_flow);
    }

    // reset() zeroes the flow the accessors read.
    alg.reset();
    for(auto && a : arcs(graph)) ASSERT_EQ(alg.flow(a), 0);
}

////////////////////////////////////////////////////////////////////////////////
// degenerate networks are handled: a single saturated arc, a zero capacity,
// no arcs at all
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(edmonds_karp, arc_with_fixed_capacity) {
    static_digraph_builder<static_digraph, int> builder(2);

    builder.add_arc({0, 1}, 107);

    auto [graph, capacity] = builder.build();

    edmonds_karp alg(graph, capacity, 0u, 1u);
    ASSERT_EQ(alg.run().flow_value(), 107);
    ASSERT_TRUE(EQ_MULTISETS(alg.minimum_cut(), {0u}));
    alg.reset();
}

GTEST_TEST(edmonds_karp, arc_with_0_capacity) {
    static_digraph_builder<static_digraph, int> builder(2);

    builder.add_arc({0, 1}, 0);

    auto [graph, capacity] = builder.build();

    edmonds_karp alg(graph, capacity, 0u, 1u);
    ASSERT_EQ(alg.run().flow_value(), 0);
    ASSERT_TRUE(EQ_MULTISETS(alg.minimum_cut(), {0u}));
    alg.reset();
}

GTEST_TEST(edmonds_karp, no_arcs) {
    static_digraph_builder<static_digraph, int, char> builder(2);

    auto [graph, capacity, part_of_minimum_cut] = builder.build();

    edmonds_karp alg(graph, capacity, 0u, 1u);
    ASSERT_EQ(alg.run().flow_value(), 0);
    ASSERT_TRUE(EMPTY(alg.minimum_cut()));
    alg.reset();
}

////////////////////////////////////////////////////////////////////////////////
// the algorithm also runs on a lazy graph view with a lambda capacity map
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(edmonds_karp, complete_digraph_view) {
    auto graph = views::complete_digraph<>(5ul);
    edmonds_karp alg(graph, [](const auto &) { return 1; }, 0ul, 1ul);
    ASSERT_EQ(alg.run().flow_value(), 4);
    alg.reset();
}

////////////////////////////////////////////////////////////////////////////////
// a graph without num_vertices takes the no-reserve BFS path: the queue grows
// while it is being walked, so it reallocates -- the walk must survive that.
// An iterator-based walk faults under ASan on a 300-vertex path; the queue
// has to outgrow every small capacity step for the test to mean anything
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(edmonds_karp, graph_without_num_vertices) {
    constexpr unsigned n = 300;
    constexpr unsigned bottleneck = n / 2;
    std::vector<std::pair<unsigned, unsigned>> arc_pairs;
    std::vector<int> capacities;
    for(unsigned i = 0; i + 1 < n; ++i) {
        arc_pairs.emplace_back(i, i + 1);
        capacities.push_back(i == bottleneck ? 2 : 9);
    }
    unsized_digraph graph(n, arc_pairs);

    edmonds_karp alg(graph, capacities, 0u, n - 1);
    ASSERT_EQ(alg.run().flow_value(), 2);
    ASSERT_TRUE(EQ_MULTISETS(alg.minimum_cut(), {bottleneck}));
}

////////////////////////////////////////////////////////////////////////////////
// capacity types without a genuine numeric_limits specialization are rejected
// at the concept level: the primary template's max() returns T{} -- a zero
// infinity that makes the augmenting-path loop spin forever at runtime for a
// type that compiles cleanly
////////////////////////////////////////////////////////////////////////////////

namespace zero_infinity_probes {
struct opaque_capacity {
    int v;
};
}  // namespace zero_infinity_probes

template <typename V>
concept edmonds_karp_admits =
    requires(static_digraph & g, std::vector<V> & capacities) {
        edmonds_karp(g, capacities);
    };
static_assert(edmonds_karp_admits<int>);
static_assert(edmonds_karp_admits<double>);
static_assert(!edmonds_karp_admits<zero_infinity_probes::opaque_capacity>);

////////////////////////////////////////////////////////////////////////////////
// the unset-terminal and un-converged states are preconditions, not silent
// wrong answers
////////////////////////////////////////////////////////////////////////////////

// regression: the two-argument constructor leaves _s and _t
// default-initialised, so run()/flow_value() must assert rather than read
// them silently. minimum_cut() has a second precondition: it reads the
// reachability the *final*, failed augmenting search leaves behind, so before
// run() converges it names a cut of no particular graph.
namespace {
auto two_arc_instance() {
    static_digraph_builder<static_digraph, int> builder(3);
    builder.add_arc({0, 1}, 5).add_arc({1, 2}, 3);
    return builder.build();
}
}  // namespace

GTEST_TEST(edmonds_karp, unset_terminals_are_preconditions) {
    auto [graph, capacity_map] = two_arc_instance();

    edmonds_karp alg(graph, capacity_map);
    EXPECT_DEATH((void)alg.run(), "");
    EXPECT_DEATH((void)alg.flow_value(), "");

    // setting only one is not enough either
    edmonds_karp half(graph, capacity_map);
    half.set_source(0u);
    EXPECT_DEATH((void)half.run(), "");
}

GTEST_TEST(edmonds_karp, minimum_cut_requires_a_converged_run) {
    auto [graph, capacity_map] = two_arc_instance();
    edmonds_karp alg(graph, capacity_map, 0u, 2u);

    EXPECT_DEATH((void)alg.minimum_cut(), "");

    alg.run();
    ASSERT_EQ(alg.flow_value(), 3);
    ASSERT_TRUE(EQ_MULTISETS(alg.minimum_cut(), {1}));

    // reset() and re-aiming both invalidate it again
    alg.reset();
    EXPECT_DEATH((void)alg.minimum_cut(), "");
    alg.run();
    alg.set_target(1u);
    EXPECT_DEATH((void)alg.minimum_cut(), "");
}

GTEST_TEST(edmonds_karp, source_equals_target_is_a_precondition) {
    static_digraph_builder<static_digraph, int> builder(3);
    builder.add_arc({0, 1}, 5).add_arc({1, 2}, 5);
    auto [graph, capacity_map] = builder.build();
    edmonds_karp alg(graph, capacity_map, 1u, 1u);
    EXPECT_DEATH((void)alg.run(), "");
}
