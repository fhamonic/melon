#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/kruskal.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/undirect.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

namespace {
// Drains the generator into the list of edges it selects.
auto mst_edges(auto & alg) {
    std::vector<decltype(alg.current())> edges;
    for(; !alg.finished(); alg.advance()) edges.push_back(alg.current());
    return edges;
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// kruskal enumerates the MST edges in ascending cost order, and reset()
// replays exactly the same run
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(kruskal, test) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder
        .add_arc(0, 1, 7)    // 0
        .add_arc(0, 2, 9)    // 1
        .add_arc(0, 5, 14)   // 2
        .add_arc(1, 2, 10)   // 3
        .add_arc(1, 3, 15)   // 4
        .add_arc(2, 3, 12)   // 5
        .add_arc(2, 5, 2)    // 6
        .add_arc(3, 4, 6)    // 7
        .add_arc(4, 5, 11);  // 8

    auto [graph, cost_map] = builder.build();
    auto ugraph = views::undirect(graph);

    kruskal alg(ugraph, cost_map);

    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 6u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 7u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 0u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 1u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 8u);
    alg.advance();
    ASSERT_TRUE(alg.finished());

    // reset() must give back exactly the run above: appending the edge list
    // to the one already there and re-pushing every vertex grows both with
    // each call. It also returns *this, like every other algorithm.
    ASSERT_EQ(std::addressof(alg.reset()), std::addressof(alg));
    ASSERT_TRUE(EQ_RANGES(mst_edges(alg), {6, 7, 0, 1, 8}));
    alg.reset().reset();
    ASSERT_TRUE(EQ_RANGES(mst_edges(alg), {6, 7, 0, 1, 8}));
}

////////////////////////////////////////////////////////////////////////////////
// degenerate inputs an unconditional first-edge merge mishandles
////////////////////////////////////////////////////////////////////////////////

// A reset() that seeds the cursor by merging *_sorted_edges.begin() outright,
// with no emptiness test, dereferences end() on a graph with vertices but no
// edges. Such a graph has an empty spanning forest.
GTEST_TEST(kruskal, edgeless_graph) {
    static_digraph_builder<static_digraph, int> builder(3);
    auto [graph, cost_map] = builder.build();
    auto ugraph = views::undirect(graph);

    kruskal alg(ugraph, cost_map);
    ASSERT_TRUE(alg.finished());
    ASSERT_TRUE(mst_edges(alg).empty());

    alg.reset();
    ASSERT_TRUE(alg.finished());
}

// The same unconditional merge accepts the first edge without asking whether
// its endpoints are already in one component, so a cheapest self-loop goes
// into the tree -- the one edge exempted from the test every other edge
// passes through.
GTEST_TEST(kruskal, cheapest_edge_is_a_self_loop) {
    static_digraph_builder<static_digraph, int> builder(3);
    builder
        .add_arc(0, 0, 1)   // 0: self-loop, and the cheapest edge
        .add_arc(0, 1, 2)   // 1
        .add_arc(1, 2, 3);  // 2
    auto [graph, cost_map] = builder.build();
    auto ugraph = views::undirect(graph);

    kruskal alg(ugraph, cost_map);
    ASSERT_TRUE(EQ_RANGES(mst_edges(alg), {1, 2}));
}
