#undef NDEBUG
#include <gtest/gtest.h>

#include <ranges>
#include <vector>

#include "melon/container/static_digraph.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/undirect.hpp"

#include "ranges_test_helper.hpp"
#include "undirected_triangle.hpp"

using namespace melon;

using adl_triangle = adl_ugraph::triangle;

// The same triangle, but every CPO is a member; num_edges and degree are left
// out, and both edges() and incidence() are sized, so that those two CPOs must
// fall back on std::ranges::size.
struct member_ugraph {
    adl_triangle _triangle;

    auto vertices() const { return adl_ugraph::vertices(_triangle); }
    auto edges() const { return adl_ugraph::edges(_triangle); }
    auto edge_endpoints(const unsigned int & e) const {
        return adl_ugraph::edge_endpoints(_triangle, e);
    }
    std::vector<adl_ugraph::endpoints> incidence(const unsigned int & v) const {
        return std::ranges::to<std::vector<adl_ugraph::endpoints>>(
            adl_ugraph::incidence(_triangle, v));
    }
};

static_assert(undirected_graph<adl_triangle>);
static_assert(undirected_graph<member_ugraph>);
static_assert(std::same_as<vertex_t<adl_triangle>, unsigned int>);
static_assert(std::same_as<edge_t<adl_triangle>, unsigned int>);

static_assert(has_num_edges<adl_triangle>);
static_assert(has_num_edges<member_ugraph>);
static_assert(has_incidence<adl_triangle>);
static_assert(has_incidence<member_ugraph>);
static_assert(has_degree<adl_triangle>);
static_assert(has_degree<member_ugraph>);
static_assert(has_edge_map<adl_triangle, int>);
static_assert(!has_edge_map<member_ugraph, int>);

// A digraph has arcs, not edges: none of the undirected concepts may accept it.
static_assert(!undirected_graph<static_digraph>);
static_assert(!has_num_edges<static_digraph>);
static_assert(!has_incidence<static_digraph>);
static_assert(!has_degree<static_digraph>);
static_assert(!has_edge_map<static_digraph, int>);

GTEST_TEST(undirected_graph_cpos, adl_dispatch) {
    const adl_triangle g;

    ASSERT_TRUE(EQ_RANGES(melon::vertices(g), {0, 1, 2}));
    ASSERT_TRUE(EQ_RANGES(melon::edges(g), {0, 1, 2}));
    ASSERT_EQ(melon::num_edges(g), 3u);

    ASSERT_EQ(melon::edge_endpoints(g, 0u), adl_ugraph::endpoints(0, 1));
    ASSERT_EQ(melon::edge_endpoints(g, 1u), adl_ugraph::endpoints(1, 2));
    ASSERT_EQ(melon::edge_endpoints(g, 2u), adl_ugraph::endpoints(2, 0));

    // Vertex 0 is incident to edge 0 (towards 1) and edge 2 (towards 2).
    ASSERT_TRUE(
        EQ_MULTISETS(melon::incidence(g, 0u),
                     std::vector<adl_ugraph::endpoints>{{0, 1}, {2, 2}}));
    for(const auto & v : melon::vertices(g)) ASSERT_EQ(melon::degree(g, v), 2u);
}

GTEST_TEST(undirected_graph_cpos, member_dispatch) {
    const member_ugraph g;

    ASSERT_TRUE(EQ_RANGES(melon::vertices(g), {0, 1, 2}));
    ASSERT_TRUE(EQ_RANGES(melon::edges(g), {0, 1, 2}));
    ASSERT_EQ(melon::edge_endpoints(g, 1u), adl_ugraph::endpoints(1, 2));
    ASSERT_TRUE(
        EQ_MULTISETS(melon::incidence(g, 1u),
                     std::vector<adl_ugraph::endpoints>{{0, 0}, {1, 2}}));
}

// member_ugraph provides neither num_edges nor degree, so those two calls are
// served by the std::ranges::size fallbacks over edges() and incidence().
GTEST_TEST(undirected_graph_cpos, size_fallbacks) {
    const member_ugraph g;

    ASSERT_EQ(melon::num_edges(g), std::ranges::size(melon::edges(g)));
    for(const auto & v : melon::vertices(g)) ASSERT_EQ(melon::degree(g, v), 2u);
}

// The degree of the ADL triangle cannot come from the fallback: its incidence
// range is not sized, so only the free degree() can answer.
static_assert(!std::ranges::sized_range<incidence_range_t<adl_triangle>>);
static_assert(std::ranges::sized_range<incidence_range_t<member_ugraph>>);

GTEST_TEST(undirected_graph_cpos, create_edge_map) {
    const adl_triangle g;

    auto map = melon::create_edge_map<int>(g);
    static_assert(std::same_as<edge_map_t<adl_triangle, int>, decltype(map)>);
    static_assert(output_mapping_of<decltype(map), edge_t<adl_triangle>, int>);
    ASSERT_EQ(std::ranges::size(map), 3u);

    auto filled = melon::create_edge_map<int>(g, 7);
    for(const auto & e : melon::edges(g)) ASSERT_EQ(filled[e], 7);
}

// The CPOs must serve a graph reached through an adaptor just as well as a
// concrete container; views::undirect is the one shipped undirected adaptor.
GTEST_TEST(undirected_graph_cpos, over_a_view) {
    static_digraph_builder<static_digraph, int> builder(3);
    builder.add_arc(0u, 1u, 1).add_arc(1u, 2u, 2).add_arc(2u, 0u, 3);
    auto [digraph, weights] = builder.build();

    auto g = views::undirect(digraph);
    static_assert(undirected_graph<decltype(g)>);
    static_assert(has_num_edges<decltype(g)>);
    static_assert(has_incidence<decltype(g)>);
    static_assert(has_edge_map<decltype(g), double>);

    ASSERT_EQ(melon::num_edges(g), 3u);
    for(const auto & v : melon::vertices(g))
        ASSERT_EQ(std::ranges::distance(melon::incidence(g, v)), 2);
}
