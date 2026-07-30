#undef NDEBUG
#include <gtest/gtest.h>

#include <utility>

#include "melon/algorithm/dijkstra.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
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

// regression: same defect as views::reverse -- the unconstrained
// `undirect(G &&)` beat the copy constructor for a non-const lvalue and
// hard-errored in graph_ref_view.
GTEST_TEST(undirect_views, copying_a_mutable_lvalue_uses_the_copy_constructor) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc(0, 1).add_arc(1, 2).add_arc(2, 0);
    auto [graph] = builder.build();

    undirect_view view(graph);

    undirect_view from_mutable_lvalue(view);
    static_assert(std::same_as<decltype(view), decltype(from_mutable_lvalue)>);
    ASSERT_EQ(from_mutable_lvalue.num_edges(), num_arcs(graph));
    ASSERT_TRUE(EQ_RANGES(view.edges(), from_mutable_lvalue.edges()));

    decltype(view) from_const_lvalue(std::as_const(view));
    ASSERT_EQ(from_const_lvalue.num_edges(), num_arcs(graph));
}
