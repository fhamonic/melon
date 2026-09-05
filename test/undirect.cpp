#undef NDEBUG
#include <gtest/gtest.h>

#include <utility>

#include "melon/algorithm/dijkstra.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/graph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/subgraph.hpp"
#include "melon/views/undirect.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// undirect presents each arc as an edge with the same endpoints, orientation
// forgotten
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(undirect_views, static_graph) {
    std::vector<
        std::pair<arc_t<static_digraph>, std::pair<vertex_t<static_digraph>,
                                                   vertex_t<static_digraph>>>>
        arc_pairs(
            {{0, {0, 1}}, {1, {0, 2}}, {2, {1, 2}}, {3, {2, 0}}, {4, {2, 1}}});

    static_digraph graph(3, std::views::keys(std::views::values(arc_pairs)),
                         std::views::values(std::views::values(arc_pairs)));

    auto ugraph = views::undirect(graph);

    ASSERT_EQ(num_vertices(graph), num_vertices(ugraph));
    ASSERT_EQ(num_arcs(graph), num_edges(ugraph));

    ASSERT_TRUE(EQ_RANGES(vertices(graph), vertices(ugraph)));
    ASSERT_TRUE(EQ_RANGES(arcs(graph), edges(ugraph)));

    for(auto && a : arcs(graph)) {
        auto u1 = arc_source(graph, a);
        auto v1 = arc_target(graph, a);
        auto [u2, v2] = edge_endpoints(ugraph, a);
        ASSERT_TRUE((u1 == u2 && v1 == v2) || (u1 == v2 && u2 == v1));
    }
}

////////////////////////////////////////////////////////////////////////////////
// copying a mutable lvalue view uses the copy constructor
////////////////////////////////////////////////////////////////////////////////

// regression: same trap as views::reverse -- an unconstrained
// `undirect(G &&)` beats the copy constructor for a non-const lvalue and
// hard-errors in graph_ref_view.
GTEST_TEST(undirect_views, copying_a_mutable_lvalue_uses_the_copy_constructor) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc({0, 1}).add_arc({1, 2}).add_arc({2, 0});
    auto [graph] = builder.build();

    undirect_view view(graph);

    undirect_view from_mutable_lvalue(view);
    static_assert(std::same_as<decltype(view), decltype(from_mutable_lvalue)>);
    ASSERT_EQ(from_mutable_lvalue.num_edges(), num_arcs(graph));
    ASSERT_TRUE(EQ_RANGES(view.edges(), from_mutable_lvalue.edges()));

    decltype(view) from_const_lvalue(std::as_const(view));
    ASSERT_EQ(from_const_lvalue.num_edges(), num_arcs(graph));
}

////////////////////////////////////////////////////////////////////////////////
// incidence() copies the wrapped view only where the copy buys borrowedness
////////////////////////////////////////////////////////////////////////////////

// regression: _capture() must branch on the same condition the
// enable_borrowed_graph specialisation requires (borrowed && copyable).
// Branching on copy_constructible alone deep-copies a copyable non-borrowed
// graph -- a filtered subgraph carrying its filter map by value -- into both
// incidence() lambdas on every call, buying a borrowedness the trait
// (correctly) refuses to report anyway.
namespace {
int filter_map_copies = 0;
struct counting_filter {
    bool operator[](const vertex_t<static_digraph> &) const { return true; }
    counting_filter() = default;
    counting_filter(const counting_filter &) { ++filter_map_copies; }
    counting_filter(counting_filter &&) noexcept = default;
    counting_filter & operator=(const counting_filter &) = default;
    counting_filter & operator=(counting_filter &&) noexcept = default;
};
}  // namespace

GTEST_TEST(undirect_views, incidence_copies_the_view_only_when_borrowed) {
    static_digraph_builder<static_digraph> builder(2);
    builder.add_arc({0, 1});
    auto [graph] = builder.build();

    auto sub = views::subgraph(graph, counting_filter{});
    static_assert(std::copy_constructible<decltype(sub)>);
    static_assert(!enable_borrowed_graph<decltype(sub)>);

    undirect_view und(sub);
    static_assert(!enable_borrowed_graph<decltype(und)>);

    filter_map_copies = 0;
    unsigned count = 0;
    for(auto && [e, w] : und.incidence(0u)) (void)e, (void)w, ++count;
    ASSERT_EQ(count, 1u);
    ASSERT_EQ(filter_map_copies, 0);

    // Where the copy does buy the trait -- a borrowed, one-pointer ref view --
    // it is still taken, so the promise keeps its property.
    undirect_view und_ref{graph_ref_view(graph)};
    static_assert(enable_borrowed_graph<decltype(und_ref)>);
    unsigned ref_count = 0;
    for(auto && [e, w] : und_ref.incidence(0u)) (void)e, (void)w, ++ref_count;
    ASSERT_EQ(ref_count, 1u);
}

////////////////////////////////////////////////////////////////////////////////
// no orphan adjacency() member: no CPO ever reaches one
////////////////////////////////////////////////////////////////////////////////

namespace {
template <typename U>
concept has_adjacency_member =
    requires(const U & u, vertex_t<static_digraph> v) { u.adjacency(v); };
}  // namespace
static_assert(
    !has_adjacency_member<undirect_view<graph_ref_view<static_digraph>>>);

////////////////////////////////////////////////////////////////////////////////
// degree() is a member, so it is O(1) and does not depend on the concat shape
////////////////////////////////////////////////////////////////////////////////

// Without the member, the CPO synthesises degree by sizing incidence(u) -- a
// concat of two transform_views, whose sized_range-ness depends on which
// implementation detail/concat_view.hpp selects, hence on
// __cpp_lib_ranges_concat, hence on the toolchain: has_degree answers false
// under GCC 14 / C++23 and true under GCC 15 / C++26. This static_assert is
// what fails if the member is dropped.
static_assert(
    melon::has_degree<undirect_view<views::graph_all_t<static_digraph &>>>);

GTEST_TEST(undirect, degree_counts_both_incidence_lists) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc({0, 1}).add_arc({1, 2}).add_arc({2, 0}).add_arc({1, 1});
    auto [graph] = builder.build();

    auto ugraph = views::undirect(graph);
    for(auto && v : vertices(ugraph)) {
        // the member and the range it describes must agree, self-loops
        // included -- vertex 1 carries one, and it sits in both incidence
        // lists, so it counts twice on each side
        ASSERT_EQ(static_cast<std::ptrdiff_t>(degree(ugraph, v)),
                  std::ranges::distance(incidence(ugraph, v)));
    }
    ASSERT_EQ(degree(ugraph, 1u), 4u);
}
