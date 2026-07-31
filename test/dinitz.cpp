#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/dinitz.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/subgraph.hpp"

#include "ranges_test_helper.hpp"
#include "unsized_digraph.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// dinitz computes the maximum flow value and a minimum cut
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(dinitz, test) {
    static_digraph_builder<static_digraph, int, char> builder(6);

    // example from https://www.geeksforgeeks.org/max-flow-problem-introduction/
    builder.add_arc(0, 1, 16, false);
    builder.add_arc(0, 2, 13, false);
    builder.add_arc(1, 2, 10, false);
    builder.add_arc(1, 3, 12, true);  //
    builder.add_arc(2, 1, 4, false);
    builder.add_arc(2, 4, 14, false);
    builder.add_arc(3, 2, 9, false);
    builder.add_arc(3, 5, 20, false);
    builder.add_arc(4, 3, 7, true);  //
    builder.add_arc(4, 5, 4, true);  //

    auto [graph, capacity, part_of_minimum_cut] = builder.build();

    dinitz alg(graph, capacity, 0u, 5u);
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

GTEST_TEST(dinitz, arc_with_fixed_capacity) {
    static_digraph_builder<static_digraph, int> builder(2);

    builder.add_arc(0, 1, 107);

    auto [graph, capacity] = builder.build();

    dinitz alg(graph, capacity, 0u, 1u);
    ASSERT_EQ(alg.run().flow_value(), 107);
    ASSERT_TRUE(EQ_MULTISETS(alg.minimum_cut(), {0u}));
    alg.reset();
}

GTEST_TEST(dinitz, arc_with_0_capacity) {
    static_digraph_builder<static_digraph, int> builder(2);

    builder.add_arc(0, 1, 0);

    auto [graph, capacity] = builder.build();

    dinitz alg(graph, capacity, 0u, 1u);
    ASSERT_EQ(alg.run().flow_value(), 0);
    ASSERT_TRUE(EQ_MULTISETS(alg.minimum_cut(), {0u}));
    alg.reset();
}

GTEST_TEST(dinitz, no_arcs) {
    static_digraph_builder<static_digraph, int, char> builder(2);

    auto [graph, capacity, part_of_minimum_cut] = builder.build();

    dinitz alg(graph, capacity, 0u, 1u);
    ASSERT_EQ(alg.run().flow_value(), 0);
    ASSERT_TRUE(EMPTY(alg.minimum_cut()));
    alg.reset();
}

////////////////////////////////////////////////////////////////////////////////
// a graph without num_vertices takes the no-reserve BFS path: the queue grows
// while it is being walked, so it reallocates -- the walk must survive that.
// The iterator version faulted under ASan on a 300-vertex path; the queue has
// to outgrow every small capacity step for the test to mean anything
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(dinitz, graph_without_num_vertices) {
    constexpr unsigned n = 300;
    constexpr unsigned bottleneck = n / 2;
    std::vector<std::pair<unsigned, unsigned>> arc_pairs;
    std::vector<int> capacities;
    for(unsigned i = 0; i + 1 < n; ++i) {
        arc_pairs.emplace_back(i, i + 1);
        capacities.push_back(i == bottleneck ? 2 : 9);
    }
    unsized_digraph graph(n, arc_pairs);

    dinitz alg(graph, capacities, 0u, n - 1);
    ASSERT_EQ(alg.run().flow_value(), 2);
    ASSERT_TRUE(EQ_MULTISETS(alg.minimum_cut(), {bottleneck}));
}

////////////////////////////////////////////////////////////////////////////////
// a filtered subgraph's incidence ranges are filter_views over a capturing
// lambda, which is not default-constructible -- yet the per-vertex cursor maps
// go through create_vertex_map<cursor>, and static_map default-constructs its
// slots. The cursors' disengaged default state is what makes dinitz over any
// filtered subgraph constructible at all.
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(dinitz, filtered_subgraph) {
    static_digraph_builder<static_digraph, int> builder(6);
    builder.add_arc(0, 1, 16);
    builder.add_arc(0, 2, 13);
    builder.add_arc(1, 2, 10);
    builder.add_arc(1, 3, 12);
    builder.add_arc(2, 1, 4);
    builder.add_arc(2, 4, 14);
    builder.add_arc(3, 2, 9);
    builder.add_arc(3, 5, 20);
    builder.add_arc(4, 3, 7);
    builder.add_arc(4, 5, 4);
    auto [graph, capacity] = std::move(builder).build();

    auto vertex_keep = create_vertex_map<bool>(graph, true);
    auto arc_keep = create_arc_map<bool>(graph, true);
    auto sub = views::subgraph(graph, vertex_keep, arc_keep);

    dinitz alg(sub, capacity, 0u, 5u);
    ASSERT_EQ(alg.run().flow_value(), 23);

    // disabling the source's 16-capacity arc reroutes everything through the
    // 13-capacity one
    sub.disable_arc(0u);
    dinitz alg2(sub, capacity, 0u, 5u);
    ASSERT_EQ(alg2.run().flow_value(), 13);
}

// The relocation path: over an *owning* filtered subgraph the graph member
// moves with the algorithm, so the hand-written move must rebase every
// non-borrowed filter_view cursor onto the new object's graph.
GTEST_TEST(dinitz, filtered_subgraph_move) {
    static_digraph_builder<static_digraph, int> builder(4);
    builder.add_arc(0, 1, 5);
    builder.add_arc(0, 2, 3);
    builder.add_arc(1, 3, 4);
    builder.add_arc(2, 3, 6);
    auto [graph, capacity] = std::move(builder).build();

    dinitz alg(
        views::subgraph(auto(graph), create_vertex_map<bool>(graph, true),
                        create_arc_map<bool>(graph, true)),
        capacity, 0u, 3u);
    dinitz moved = std::move(alg);
    ASSERT_EQ(moved.run().flow_value(), 7);

    dinitz other(
        views::subgraph(auto(graph), create_vertex_map<bool>(graph, true),
                        create_arc_map<bool>(graph, true)),
        capacity, 0u, 3u);
    other = std::move(moved);
    ASSERT_EQ(other.reset().run().flow_value(), 7);
}

// #include "melon/views/complete_digraph.hpp"

// GTEST_TEST(dinitz, complete_digraph_view) {
//     ASSERT_EQ(dinitz(
//                   views::complete_digraph(5ul), [](const auto &) { return 1;
//                   }, 0ul, 1ul) .run() .flow_value(),
//               4);
// }

////////////////////////////////////////////////////////////////////////////////
// capacity types without a genuine numeric_limits specialization are rejected
// at the concept level: the primary template's max() returns T{} -- a zero
// infinity that made the augmenting-path loop spin forever at runtime for a
// type that compiled cleanly
////////////////////////////////////////////////////////////////////////////////

namespace zero_infinity_probes {
struct opaque_capacity {
    int v;
};
}  // namespace zero_infinity_probes

template <typename V>
concept dinitz_admits = requires(
    static_digraph & g, std::vector<V> & capacities) { dinitz(g, capacities); };
static_assert(dinitz_admits<int>);
static_assert(dinitz_admits<double>);
static_assert(!dinitz_admits<zero_infinity_probes::opaque_capacity>);
