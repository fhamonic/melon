#undef NDEBUG
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "melon/graph.hpp"
#include "melon/views/complete_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

using G = views::complete_digraph<>;

////////////////////////////////////////////////////////////////////////////////
// complete_digraph is a graph_view modelling every directed-graph concept
////////////////////////////////////////////////////////////////////////////////

static_assert(melon::graph<G>);
static_assert(melon::outward_incidence_graph<G>);
static_assert(melon::outward_adjacency_graph<G>);
static_assert(melon::inward_incidence_graph<G>);
static_assert(melon::inward_adjacency_graph<G>);
static_assert(melon::has_vertex_map<G>);
static_assert(melon::has_arc_map<G>);
static_assert(melon::graph_view<G>);

////////////////////////////////////////////////////////////////////////////////
// an empty complete digraph has no vertices or arcs, and asserts on any access
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(complete_digraph, empty_constructor) {
    G graph;
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    EXPECT_DEATH((void)arc_source(graph, 0), "");
    EXPECT_DEATH((void)arc_target(graph, 0), "");

    EXPECT_DEATH((void)out_arcs(graph, 0), "");
    EXPECT_DEATH((void)out_neighbors(graph, 0), "");
    EXPECT_DEATH((void)in_arcs(graph, 0), "");
    EXPECT_DEATH((void)in_neighbors(graph, 0), "");
}

////////////////////////////////////////////////////////////////////////////////
// on n vertices, every ordered pair of distinct vertices is exactly one arc
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(complete_digraph, k4) {
    G graph(4);

    ASSERT_EQ(num_vertices(graph), 4);
    ASSERT_EQ(num_arcs(graph), 12);

    ASSERT_TRUE(EQ_MULTISETS(out_arcs(graph, 0), {0, 1, 2}));
    ASSERT_TRUE(EQ_MULTISETS(out_arcs(graph, 1), {3, 4, 5}));
    ASSERT_TRUE(EQ_MULTISETS(out_arcs(graph, 2), {6, 7, 8}));
    ASSERT_TRUE(EQ_MULTISETS(out_arcs(graph, 3), {9, 10, 11}));

    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 0), {1, 2, 3}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 1), {0, 2, 3}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 2), {0, 1, 3}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 3), {0, 1, 2}));

    ASSERT_TRUE(EQ_MULTISETS(in_arcs(graph, 0), {3, 6, 9}));
    ASSERT_TRUE(EQ_MULTISETS(in_arcs(graph, 1), {0, 7, 10}));
    ASSERT_TRUE(EQ_MULTISETS(in_arcs(graph, 2), {1, 4, 11}));
    ASSERT_TRUE(EQ_MULTISETS(in_arcs(graph, 3), {2, 5, 8}));

    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, 0), {1, 2, 3}));
    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, 1), {0, 2, 3}));
    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, 2), {0, 1, 3}));
    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, 3), {0, 1, 2}));
}

////////////////////////////////////////////////////////////////////////////////
// both degrees are the constant n - 1, O(1) and noexcept
////////////////////////////////////////////////////////////////////////////////

// The answer is a constant, yet the view had no degree members: out_degree
// fell back to the size of the out_arcs iota, and in_degree did not exist at
// all -- in_arcs is a concat of two subranges, which is not a sized_range, so
// the size-of-the-range fallback never fires for it.
GTEST_TEST(complete_digraph, degrees_are_the_constant_n_minus_one) {
    static_assert(melon::has_out_degree<G> && melon::has_in_degree<G>);

    G graph(4);
    static_assert(noexcept(graph.out_degree(0u)));
    static_assert(noexcept(graph.in_degree(0u)));
    for(auto && u : vertices(graph)) {
        EXPECT_EQ(out_degree(graph, u), 3u);
        EXPECT_EQ(in_degree(graph, u), 3u);
    }
    EXPECT_DEATH((void)out_degree(graph, 4u), "");
    EXPECT_DEATH((void)in_degree(graph, 4u), "");
}

////////////////////////////////////////////////////////////////////////////////
// num_arcs is exact up to the arc type's ceiling and asserts past it instead
// of wrapping
////////////////////////////////////////////////////////////////////////////////

// regression 2.8: num_arcs() returns std::size_t but computed n * (n - 1)
// after casting both operands to `arc` (unsigned int by default), so it
// wrapped silently past 65536 vertices: 70000 vertices reported 604'962'704
// arcs instead of 4'899'930'000. It is computed in std::size_t now.
GTEST_TEST(complete_digraph, num_arcs_is_exact_at_the_boundary) {
    // the largest size whose arc count still fits in a 32-bit arc handle
    const std::size_t n = 65535;
    G graph(n);
    ASSERT_EQ(num_arcs(graph), n * (n - 1));
    ASSERT_EQ(num_arcs(graph), 4294770690ull);
    ASSERT_LE(num_arcs(graph), std::numeric_limits<unsigned int>::max());
}

// Past that the product no longer fits in `arc`, which is also the type of the
// handles that would have to address those arcs -- so any number returned
// would be a lie. The assert names the ceiling instead of handing back a
// wrapped one.
GTEST_TEST(complete_digraph, too_many_arcs_for_the_handle_type_is_caught) {
    G graph(70000);
    EXPECT_DEATH((void)num_arcs(graph), "");
}

// with a wider arc type the same size is representable and exact
GTEST_TEST(complete_digraph, a_wider_arc_type_raises_the_ceiling) {
    views::complete_digraph<unsigned int, std::uint64_t> graph(70000);
    ASSERT_EQ(num_arcs(graph), 4899930000ull);
}
