#undef NDEBUG
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <ranges>

#include "melon/algorithm/depth_first_search.hpp"
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

// The answer is a constant, so the view carries its own degree members:
// without them, out_degree falls back to the size of the out_arcs iota, and
// in_degree does not exist at all -- in_arcs is a concat of two subranges,
// which is not a sized_range, so the size-of-the-range fallback never fires
// for it.
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

// regression: num_arcs() must compute n * (n - 1) in std::size_t. Computed
// after casting both operands to `arc` (unsigned int by default), it wraps
// silently past 65536 vertices: 70000 vertices report 604'962'704 arcs
// instead of 4'899'930'000.
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

// The arc-id products are taken in std::size_t: in vertex-width arithmetic
// the same wide-arc configuration that raises the ceiling enumerates wrong
// incidence lists past the 32-bit line.
GTEST_TEST(complete_digraph, wide_arc_incidence_is_exact_past_the_ceiling) {
    views::complete_digraph<unsigned int, std::uint64_t> graph(70000);
    const auto u = 69999u;
    ASSERT_EQ(*std::ranges::begin(out_arcs(graph, u)), 4899860001ull);
    ASSERT_EQ(arc_source(graph, *std::ranges::begin(out_arcs(graph, u))), u);
    ASSERT_TRUE(SIZE(in_arcs(graph, u), 69999));
    std::uint64_t last = 0;
    for(auto && a : in_arcs(graph, u)) last = a;
    ASSERT_EQ(last, 4899860000ull);
}

GTEST_TEST(complete_digraph, uint16_vertices_do_not_overflow_int_promotion) {
    views::complete_digraph<std::uint16_t, std::uint32_t> graph(65535);
    ASSERT_EQ(num_arcs(graph), 4294770690ull);
    ASSERT_EQ(*std::ranges::begin(out_arcs(graph, std::uint16_t(65534))),
              4294705156u);
}

// Signed handles would turn in_arcs(0)'s wraparound-empty first subrange into
// a genuine walk from -1.
template <typename V, typename A>
concept complete_digraph_instantiable =
    requires { typename views::complete_digraph<V, A>; };
static_assert(complete_digraph_instantiable<unsigned int, unsigned int>);
static_assert(!complete_digraph_instantiable<int, int>);
static_assert(!complete_digraph_instantiable<unsigned int, int>);

// The borrowed promise covers every range a CPO can hand out -- including the
// synthesized neighbors and entries, which must be built from values, never
// from the view object's address.
GTEST_TEST(complete_digraph, cpo_ranges_survive_the_graph_object) {
    auto source = std::make_unique<views::complete_digraph<>>(4);
    auto entries = arcs_entries(*source);
    auto neighbors = out_neighbors(*source, 1u);
    source.reset();
    ASSERT_TRUE(SIZE(entries, 12));
    ASSERT_TRUE(EQ_RANGES(neighbors, {0u, 2u, 3u}));
}

GTEST_TEST(complete_digraph, moved_dfs_survives_its_source_object) {
    auto src = std::make_unique<depth_first_search<views::complete_digraph<>>>(
        views::complete_digraph<>(4), 0u);
    src->advance();
    auto moved = std::move(*src);
    src.reset();
    std::size_t walked = 1;
    for(; !moved.finished(); moved.advance()) ++walked;
    ASSERT_EQ(walked, 4u);
}
