#undef NDEBUG
#include <gtest/gtest.h>

#include <cstdint>
#include <ranges>
#include <vector>

#include "melon/container/static_forward_digraph.hpp"
#include "melon/graph.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// static_forward_digraph models the outward-only incidence and adjacency
// graph concepts and supports vertex and arc maps
////////////////////////////////////////////////////////////////////////////////

static_assert(melon::graph<static_forward_digraph>);
static_assert(melon::outward_incidence_graph<static_forward_digraph>);
static_assert(melon::outward_adjacency_graph<static_forward_digraph>);
static_assert(melon::has_vertex_map<static_forward_digraph>);
static_assert(melon::has_arc_map<static_forward_digraph>);

////////////////////////////////////////////////////////////////////////////////
// construction from source/target vectors defines the vertices, arcs,
// out-incidence lists and validity bounds -- and out-of-range queries die
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(static_forward_digraph, empty_constructor) {
    static_forward_digraph graph;
    ASSERT_EQ(num_vertices(graph), 0u);
    ASSERT_EQ(num_arcs(graph), 0u);
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_FALSE(is_valid_vertex(graph, 0));
    ASSERT_FALSE(is_valid_arc(graph, 0));

    EXPECT_DEATH((void)out_arcs(graph, 0), "");
    EXPECT_DEATH((void)arc_target(graph, 0), "");
}

GTEST_TEST(static_forward_digraph, empty_vectors_constructor) {
    std::vector<vertex_t<static_forward_digraph>> sources;
    std::vector<vertex_t<static_forward_digraph>> targets;

    static_forward_digraph graph(0, std::move(sources), std::move(targets));
    ASSERT_EQ(num_vertices(graph), 0u);
    ASSERT_EQ(num_arcs(graph), 0u);
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_FALSE(is_valid_vertex(graph, 0));
    ASSERT_FALSE(is_valid_arc(graph, 0));

    EXPECT_DEATH((void)out_arcs(graph, 0), "");
    EXPECT_DEATH((void)arc_target(graph, 0), "");
    EXPECT_DEATH((void)out_neighbors(graph, 0), "");
}

GTEST_TEST(static_forward_digraph, vectors_constructor_1) {
    std::vector<std::pair<arc_t<static_forward_digraph>,
                          std::pair<vertex_t<static_forward_digraph>,
                                    vertex_t<static_forward_digraph>>>>
        arc_pairs(
            {{0, {0, 1}}, {1, {0, 2}}, {2, {1, 2}}, {3, {2, 0}}, {4, {2, 1}}});

    static_forward_digraph graph(
        3, std::views::keys(std::views::values(arc_pairs)),
        std::views::values(std::views::values(arc_pairs)));
    ASSERT_EQ(num_vertices(graph), 3u);
    ASSERT_EQ(num_arcs(graph), 5u);

    ASSERT_TRUE(EQ_RANGES(vertices(graph), {0, 1, 2}));
    ASSERT_TRUE(EQ_RANGES(arcs(graph), {0, 1, 2, 3, 4}));

    for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph, u));
    ASSERT_FALSE(is_valid_vertex(
        graph, vertex_t<static_forward_digraph>(num_vertices(graph))));

    for(auto a : arcs(graph)) ASSERT_TRUE(is_valid_arc(graph, a));
    ASSERT_FALSE(
        is_valid_arc(graph, arc_t<static_forward_digraph>(num_arcs(graph))));

    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph, 0), {1, 2}));
    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph, 1), {2}));
    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph, 2), {0, 1}));
    ASSERT_TRUE(EQ_RANGES(arcs_entries(graph), arc_pairs));
}

GTEST_TEST(static_forward_digraph, vectors_constructor_2) {
    std::vector<std::pair<arc_t<static_forward_digraph>,
                          std::pair<vertex_t<static_forward_digraph>,
                                    vertex_t<static_forward_digraph>>>>
        arc_pairs({{0, {1, 2}},
                   {1, {1, 6}},
                   {2, {1, 7}},
                   {3, {2, 3}},
                   {4, {2, 4}},
                   {5, {3, 4}},
                   {6, {5, 2}},
                   {7, {5, 3}},
                   {8, {6, 5}}});

    static_forward_digraph graph(
        8, std::views::keys(std::views::values(arc_pairs)),
        std::views::values(std::views::values(arc_pairs)));
    ASSERT_EQ(num_vertices(graph), 8u);
    ASSERT_EQ(num_arcs(graph), 9u);

    ASSERT_TRUE(EQ_RANGES(vertices(graph), {0, 1, 2, 3, 4, 5, 6, 7}));
    ASSERT_TRUE(EQ_RANGES(arcs(graph), {0, 1, 2, 3, 4, 5, 6, 7, 8}));

    for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph, u));
    ASSERT_FALSE(is_valid_vertex(
        graph, vertex_t<static_forward_digraph>(num_vertices(graph))));

    for(auto a : arcs(graph)) ASSERT_TRUE(is_valid_arc(graph, a));
    ASSERT_FALSE(
        is_valid_arc(graph, arc_t<static_forward_digraph>(num_arcs(graph))));

    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph, 0),
                  std::ranges::empty_view<vertex_t<static_forward_digraph>>()));
    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph, 1), {2, 6, 7}));
    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph, 2), {3, 4}));
    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph, 6), {5}));
    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph, 7),
                  std::ranges::empty_view<vertex_t<static_forward_digraph>>()));

    ASSERT_TRUE(EQ_RANGES(arcs_entries(graph), arc_pairs));
}

////////////////////////////////////////////////////////////////////////////////
// the constructor forwards its range arguments: rvalues may be moved from,
// lvalues are left intact
////////////////////////////////////////////////////////////////////////////////

// regression: the constructor must forward, not `std::move`, its forwarding
// references -- an unconditional `_arc_target(std::move(targets))` steals
// from an lvalue the caller still owns, harmless only because static_map's
// range constructor copies. The asserts read the member, not the argument.
GTEST_TEST(static_forward_digraph, built_from_rvalue_ranges) {
    std::vector<unsigned int> sources{0u, 0u, 1u, 2u};
    std::vector<unsigned int> targets{1u, 2u, 2u, 0u};

    const static_forward_digraph graph(3, std::move(sources),
                                       std::move(targets));

    ASSERT_EQ(num_vertices(graph), 3u);
    ASSERT_EQ(num_arcs(graph), 4u);
    ASSERT_TRUE(EQ_RANGES(out_arcs(graph, 0u), {0u, 1u}));
    ASSERT_TRUE(EQ_RANGES(out_arcs(graph, 1u), {2u}));
    ASSERT_TRUE(EQ_RANGES(out_arcs(graph, 2u), {3u}));
    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph, 0u), {1u, 2u}));
    for(const auto a : arcs(graph))
        ASSERT_EQ(arc_target(graph, a),
                  std::vector<unsigned int>({1u, 2u, 2u, 0u})[a]);
}

// and from lvalues, which an unconditional std::move would quietly steal from
GTEST_TEST(static_forward_digraph,
           built_from_lvalue_ranges_leaves_them_intact) {
    std::vector<unsigned int> sources{0u, 0u, 1u, 2u};
    std::vector<unsigned int> targets{1u, 2u, 2u, 0u};

    const static_forward_digraph graph(3, sources, targets);

    ASSERT_EQ(num_arcs(graph), 4u);
    ASSERT_TRUE(EQ_RANGES(sources, {0u, 0u, 1u, 2u}));
    ASSERT_TRUE(EQ_RANGES(targets, {1u, 2u, 2u, 0u}));
}

////////////////////////////////////////////////////////////////////////////////
// static_forward_digraph is basic_static_forward_digraph<>: the handle types
// are template parameters, unsigned only, and a count past a handle's max is
// caught
////////////////////////////////////////////////////////////////////////////////

static_assert(
    std::same_as<static_forward_digraph,
                 basic_static_forward_digraph<unsigned int, unsigned int>>);

template <typename V, typename A>
concept static_forward_digraph_instantiable =
    requires { typename basic_static_forward_digraph<V, A>; };
static_assert(
    static_forward_digraph_instantiable<std::uint16_t, std::uint64_t>);
static_assert(!static_forward_digraph_instantiable<int, unsigned int>);
static_assert(!static_forward_digraph_instantiable<unsigned int, int>);

using narrow_forward_digraph =
    basic_static_forward_digraph<std::uint16_t, std::uint16_t>;
using wide_forward_digraph =
    basic_static_forward_digraph<std::uint64_t, std::uint64_t>;
static_assert(melon::outward_incidence_graph<narrow_forward_digraph>);
static_assert(melon::outward_adjacency_graph<narrow_forward_digraph>);
static_assert(!melon::has_arc_source<narrow_forward_digraph>);
static_assert(melon::outward_incidence_graph<wide_forward_digraph>);
static_assert(melon::outward_adjacency_graph<wide_forward_digraph>);
static_assert(std::same_as<vertex_t<narrow_forward_digraph>, std::uint16_t>);
static_assert(std::same_as<arc_t<wide_forward_digraph>, std::uint64_t>);

// regression: on a 16-bit handle the last vertex's `u + 1` promotes to int;
// its bound comparison and its offset lookup must both reach the last slot
GTEST_TEST(static_forward_digraph, uint16_handles_reach_the_last_vertex) {
    const std::size_t n = 65535;  // the handle's max is the largest count
    const std::uint16_t last = 65534;
    std::vector<std::uint16_t> sources = {0, last, last};
    std::vector<std::uint16_t> targets = {last, 0, last};
    narrow_forward_digraph graph(n, sources, targets);

    ASSERT_EQ(num_vertices(graph), n);
    ASSERT_EQ(std::ranges::distance(vertices(graph)), 65535);
    ASSERT_TRUE(EQ_RANGES(out_arcs(graph, last), {1u, 2u}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, last), {0u, 65534u}));
    ASSERT_EQ(out_degree(graph, last), 2u);
    ASSERT_TRUE(EQ_RANGES(out_arcs(graph, std::uint16_t(0)), {0u}));
}

GTEST_TEST(static_forward_digraph, counts_past_a_handles_max_die) {
    std::vector<std::uint16_t> no_arcs;
    EXPECT_DEATH((narrow_forward_digraph(65536, no_arcs, no_arcs)), "");
    std::vector<std::uint16_t> too_many(65536, std::uint16_t(0));
    EXPECT_DEATH((narrow_forward_digraph(1, too_many, too_many)), "");
}

GTEST_TEST(static_forward_digraph, uint64_handles_answer_the_same_queries) {
    std::vector<std::uint64_t> sources = {0, 0, 1, 2};
    std::vector<std::uint64_t> targets = {1, 2, 2, 0};
    wide_forward_digraph graph(3, sources, targets);

    ASSERT_EQ(num_arcs(graph), 4u);
    // materialized: an iota over 64-bit handles has a __int128 distance,
    // which the helper cannot print
    ASSERT_TRUE(EQ_RANGES(
        std::ranges::to<std::vector>(out_arcs(graph, std::uint64_t(0))),
        {0u, 1u}));
    ASSERT_TRUE(EQ_RANGES(out_neighbors(graph, std::uint64_t(2)), {0u}));
    ASSERT_EQ(arc_target(graph, std::uint64_t(3)), 0u);
}
