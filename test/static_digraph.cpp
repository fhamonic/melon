#undef NDEBUG
#include <gtest/gtest.h>

#include <memory>

#include "melon/container/static_digraph.hpp"
#include "melon/graph.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// static_digraph models the incidence and adjacency graph concepts, in both
// directions, and supports vertex and arc maps
////////////////////////////////////////////////////////////////////////////////

static_assert(melon::graph<static_digraph>);
static_assert(melon::outward_incidence_graph<static_digraph>);
static_assert(melon::outward_adjacency_graph<static_digraph>);
static_assert(melon::inward_incidence_graph<static_digraph>);
static_assert(melon::inward_adjacency_graph<static_digraph>);
static_assert(melon::has_vertex_map<static_digraph>);
static_assert(melon::has_arc_map<static_digraph>);

// A value type the fill-by-copy factories cannot hold makes the creation
// concepts answer false -- probeable, where an unconstrained factory with a
// deduced return type would hard-error during return-type deduction.
namespace {
struct no_default_ctor {
    no_default_ctor(int);
};
}  // namespace
static_assert(!melon::has_vertex_map<static_digraph, std::unique_ptr<int>>);
static_assert(!melon::has_arc_map<static_digraph, std::unique_ptr<int>>);
static_assert(!melon::has_vertex_map<static_digraph, no_default_ctor>);

////////////////////////////////////////////////////////////////////////////////
// construction from source/target vectors defines the vertices, arcs,
// incidence lists and validity bounds -- and out-of-range queries die
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(static_digraph, empty_constructor) {
    static_digraph graph;
    ASSERT_EQ(num_vertices(graph), 0u);
    ASSERT_EQ(num_arcs(graph), 0u);
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_FALSE(is_valid_vertex(graph, 0));
    ASSERT_FALSE(is_valid_arc(graph, 0));

    EXPECT_DEATH((void)out_arcs(graph, 0), "");
    EXPECT_DEATH((void)arc_target(graph, 0), "");
    EXPECT_DEATH((void)out_arcs(graph, 0), "");
    EXPECT_DEATH((void)arc_source(graph, 0), "");
}

GTEST_TEST(static_digraph, empty_vectors_constructor) {
    std::vector<vertex_t<static_digraph>> sources;
    std::vector<vertex_t<static_digraph>> targets;

    static_digraph graph(1, std::move(sources), std::move(targets));
    ASSERT_EQ(num_vertices(graph), 1u);
    ASSERT_EQ(num_arcs(graph), 0u);
    ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {0}));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_TRUE(is_valid_vertex(graph, 0));
    ASSERT_FALSE(is_valid_vertex(graph, 1));
    ASSERT_FALSE(is_valid_arc(graph, 0));

    EXPECT_DEATH((void)out_arcs(graph, 1), "");
    EXPECT_DEATH((void)arc_target(graph, 1), "");
    EXPECT_DEATH((void)out_arcs(graph, 1), "");
    EXPECT_DEATH((void)arc_source(graph, 1), "");
}

GTEST_TEST(static_digraph, vectors_constructor_1) {
    std::vector<
        std::pair<arc_t<static_digraph>, std::pair<vertex_t<static_digraph>,
                                                   vertex_t<static_digraph>>>>
        arc_pairs(
            {{0, {0, 1}}, {1, {0, 2}}, {2, {1, 2}}, {3, {2, 0}}, {4, {2, 1}}});

    static_digraph graph(3, std::views::keys(std::views::values(arc_pairs)),
                         std::views::values(std::views::values(arc_pairs)));
    ASSERT_EQ(num_vertices(graph), 3u);
    ASSERT_EQ(num_arcs(graph), 5u);
    ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {0, 1, 2}));
    ASSERT_TRUE(EQ_MULTISETS(arcs(graph), {0, 1, 2, 3, 4}));

    for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph, u));
    ASSERT_FALSE(
        is_valid_vertex(graph, vertex_t<static_digraph>(num_vertices(graph))));

    for(auto a : arcs(graph)) ASSERT_TRUE(is_valid_arc(graph, a));
    ASSERT_FALSE(is_valid_arc(graph, arc_t<static_digraph>(num_arcs(graph))));

    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 0), {1, 2}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 1), {2}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 2), {0, 1}));

    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, 0), {2}));
    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, 1), {0, 2}));
    ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, 2), {0, 1}));

    ASSERT_TRUE(EQ_MULTISETS(arcs_entries(graph), arc_pairs));

    for(arc_t<static_digraph> a : arcs(graph)) {
        ASSERT_EQ(arc_source(graph, a), arc_pairs[a].second.first);
        ASSERT_EQ(arc_target(graph, a), arc_pairs[a].second.second);
    }
}

GTEST_TEST(static_digraph, vectors_constructor_2) {
    std::vector<
        std::pair<arc_t<static_digraph>, std::pair<vertex_t<static_digraph>,
                                                   vertex_t<static_digraph>>>>
        arc_pairs({{0, {1, 2}},
                   {1, {1, 6}},
                   {2, {1, 7}},
                   {3, {2, 3}},
                   {4, {2, 4}},
                   {5, {3, 4}},
                   {6, {5, 2}},
                   {7, {5, 3}},
                   {8, {6, 5}}});

    static_digraph graph(8, std::views::keys(std::views::values(arc_pairs)),
                         std::views::values(std::views::values(arc_pairs)));
    ASSERT_EQ(num_vertices(graph), 8u);
    ASSERT_EQ(num_arcs(graph), 9u);

    ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {0, 1, 2, 3, 4, 5, 6, 7}));
    ASSERT_TRUE(EQ_MULTISETS(arcs(graph), {0, 1, 2, 3, 4, 5, 6, 7, 8}));

    for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph, u));
    ASSERT_FALSE(
        is_valid_vertex(graph, vertex_t<static_digraph>(num_vertices(graph))));

    for(auto a : arcs(graph)) ASSERT_TRUE(is_valid_arc(graph, a));
    ASSERT_FALSE(is_valid_arc(graph, arc_t<static_digraph>(num_arcs(graph))));

    ASSERT_TRUE(EMPTY(out_neighbors(graph, 0)));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 1), {2, 6, 7}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 2), {3, 4}));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, 6), {5}));
    ASSERT_TRUE(EMPTY(out_neighbors(graph, 7)));

    ASSERT_TRUE(EQ_MULTISETS(arcs_entries(graph), arc_pairs));

    for(arc_t<static_digraph> a : arcs(graph)) {
        ASSERT_EQ(arc_source(graph, a), arc_pairs[a].second.first);
        ASSERT_EQ(arc_target(graph, a), arc_pairs[a].second.second);
    }
}

////////////////////////////////////////////////////////////////////////////////
// out_arcs and in_arcs enumerate arc ids in ascending order
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(static_digraph, incidence_ranges_are_ascending) {
    // The constructor fills each in-arc bucket backwards over *descending*
    // arc ids precisely so both incidence ranges come out ascending: forward
    // strides through every arc map indexed inside an incidence loop.
    std::vector<vertex_t<static_digraph>> sources = {0, 0, 1, 2, 2};
    std::vector<vertex_t<static_digraph>> targets = {1, 2, 2, 0, 1};
    static_digraph graph(3, sources, targets);

    for(auto u : vertices(graph)) {
        ASSERT_TRUE(std::ranges::is_sorted(out_arcs(graph, u)));
        ASSERT_TRUE(std::ranges::is_sorted(in_arcs(graph, u)));
    }
    ASSERT_TRUE(EQ_RANGES(in_arcs(graph, 2u), {1u, 2u}));
    ASSERT_TRUE(EQ_RANGES(in_arcs(graph, 1u), {0u, 4u}));
}
