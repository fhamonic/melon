#undef NDEBUG
#include <gtest/gtest.h>

#include <cstddef>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "melon/container/static_digraph.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/undirect.hpp"

#include "ranges_test_helper.hpp"
#include "undirected_triangle.hpp"

// Not `using namespace melon;`: with the directive at file scope, MSVC's
// instantiation-time lookup meets the melon::create_*_map variables from
// inside the create-map CPOs' ADL branch and re-enters the operator() it is
// compiling (C3779/C2131) for every graph that provides maps through free
// functions, adl_ugraph::triangle included. Named using-declarations for
// everything but the create-map names keep the file readable without arming
// that trap.
using melon::edge_map_t;
using melon::edge_t;
using melon::edges_range_t;
using melon::has_degree;
using melon::has_edge_map;
using melon::has_incidence;
using melon::has_num_edges;
using melon::incidence_range_t;
using melon::output_mapping_of;
using melon::static_digraph;
using melon::static_digraph_builder;
using melon::undirected_graph;
using melon::undirected_graph_ref_view;
using melon::vertex_t;
namespace views = melon::views;

using adl_triangle = adl_ugraph::triangle;

////////////////////////////////////////////////////////////////////////////////
// a type models undirected_graph whether its CPOs are free functions or
// members
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// a digraph has arcs, not edges: none of the undirected concepts may accept it
////////////////////////////////////////////////////////////////////////////////

static_assert(!undirected_graph<static_digraph>);
static_assert(!has_num_edges<static_digraph>);
static_assert(!has_incidence<static_digraph>);
static_assert(!has_degree<static_digraph>);
static_assert(!has_edge_map<static_digraph, int>);

////////////////////////////////////////////////////////////////////////////////
// the CPOs dispatch to free functions by ADL and to members alike
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// num_edges and degree fall back on std::ranges::size when no member or free
// function answers
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// create_edge_map builds an output mapping over the graph's edges
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(undirected_graph_cpos, create_edge_map) {
    const adl_triangle g;

    auto map = melon::create_edge_map<int>(g);
    static_assert(std::same_as<edge_map_t<adl_triangle, int>, decltype(map)>);
    static_assert(output_mapping_of<decltype(map), edge_t<adl_triangle>, int>);
    ASSERT_EQ(std::ranges::size(map), 3u);

    auto filled = melon::create_edge_map<int>(g, 7);
    for(const auto & e : melon::edges(g)) ASSERT_EQ(filled[e], 7);
}

////////////////////////////////////////////////////////////////////////////////
// the CPOs serve a graph reached through an adaptor just as well as a concrete
// container
////////////////////////////////////////////////////////////////////////////////

// views::undirect is the one shipped undirected adaptor.
GTEST_TEST(undirected_graph_cpos, over_a_view) {
    static_digraph_builder<static_digraph, int> builder(3);
    builder.add_arc({0u, 1u}, 1).add_arc({1u, 2u}, 2).add_arc({2u, 0u}, 3);
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

////////////////////////////////////////////////////////////////////////////////
// asking has_edge_map about a melon-associated type answers false instead of
// failing to compile
////////////////////////////////////////////////////////////////////////////////

// melon is an associated namespace of this type (it derives from a melon base),
// and it has no member create_edge_map -- so the ADL branch of the CPO is the
// one that gets probed. With create_edge_map a *function* template in
// namespace melon, ADL finds it from inside has_adl_create_edge_map and the
// constraint ends up depending on itself ("satisfaction of atomic constraint
// depends on itself"): merely asking has_edge_map<> about this type is a hard
// error. A variable template is invisible to ADL. Same reasoning as
// create_vertex_map / create_arc_map in graph.hpp.
struct melon_associated_ugraph : melon::undirected_graph_view_base {
    adl_triangle _triangle;

    auto vertices() const { return adl_ugraph::vertices(_triangle); }
    auto edges() const { return adl_ugraph::edges(_triangle); }
    auto edge_endpoints(const unsigned int & e) const {
        return adl_ugraph::edge_endpoints(_triangle, e);
    }
};

static_assert(undirected_graph<melon_associated_ugraph>);
static_assert(!has_edge_map<melon_associated_ugraph, int>);
static_assert(!melon_create_map_cpo::has_adl_create_edge_map<
              melon_associated_ugraph, int>);

// The same trap one level up: create_edge_map must not be reachable by ADL for
// the shipped views either, which all live in melon.
static_assert(!melon_create_map_cpo::has_adl_create_edge_map<
              undirected_graph_ref_view<melon_associated_ugraph>, int>);

////////////////////////////////////////////////////////////////////////////////
// the CPOs' specifications come from the const overloads they actually call
////////////////////////////////////////////////////////////////////////////////

// regression: an is_noexcept helper built on std::declval<T &>() and called
// as `is_noexcept<T &>()` -- where `const T &` with T = U& collapses to U& --
// makes the *branch selection* inside the helper probe a non-const object.
// The operators all take `const T &`, so for a type with distinct const and
// non-const overloads the specification is read off the overload that is
// never called.
namespace const_overloaded {

// Each member below has a throwing non-const overload and a noexcept const
// one, returning different types. The const one is what the CPOs call.
struct ugraph {
    unsigned int _edges[3] = {0u, 1u, 2u};
    std::pair<unsigned int, unsigned int> _endpoints[3] = {
        {0u, 1u}, {1u, 2u}, {2u, 0u}};

    auto vertices() const noexcept { return std::views::iota(0u, 3u); }

    std::vector<unsigned int> edges() { return {0u, 1u, 2u}; }
    std::span<const unsigned int> edges() const noexcept { return _edges; }

    std::size_t num_edges() { return 3u; }
    unsigned int num_edges() const noexcept { return 3u; }

    std::pair<unsigned int, unsigned int> edge_endpoints(const unsigned int &) {
        return {0u, 0u};
    }
    std::pair<unsigned int, unsigned int> edge_endpoints(
        const unsigned int & e) const noexcept {
        return _endpoints[e];
    }

    std::vector<std::pair<unsigned int, unsigned int>> incidence(
        const unsigned int &) {
        return {};
    }
    std::span<const std::pair<unsigned int, unsigned int>> incidence(
        const unsigned int &) const noexcept {
        return _endpoints;
    }

    std::size_t degree(const unsigned int &) { return 0u; }
    unsigned int degree(const unsigned int &) const noexcept { return 2u; }
};

}  // namespace const_overloaded

static_assert(undirected_graph<const_overloaded::ugraph>);
static_assert(has_incidence<const_overloaded::ugraph>);
static_assert(has_degree<const_overloaded::ugraph>);
static_assert(has_num_edges<const_overloaded::ugraph>);

// The specification comes from the overload the CPO actually calls -- read
// off the non-const overload, every one of these answers false.
static_assert(
    noexcept(melon::edges(std::declval<const const_overloaded::ugraph &>())));
static_assert(noexcept(
    melon::num_edges(std::declval<const const_overloaded::ugraph &>())));
static_assert(noexcept(
    melon::edge_endpoints(std::declval<const const_overloaded::ugraph &>(),
                          std::declval<const unsigned int &>())));
static_assert(
    noexcept(melon::incidence(std::declval<const const_overloaded::ugraph &>(),
                              std::declval<const unsigned int &>())));
static_assert(
    noexcept(melon::degree(std::declval<const const_overloaded::ugraph &>(),
                           std::declval<const unsigned int &>())));

// The aliases are right on their own, and that is worth recording: they go
// through the CPO, whose body reads through std::as_const (the operator()
// takes `T &&` only to reject dangling rvalues), so a non-const argument
// still selects the const overload. Only noexcept helpers that call the
// member directly rather than through the CPO can read the wrong one.
// Spelling them `std::declval<const T &>()` makes them agree with the
// operators, but it changes no type.
static_assert(std::same_as<edges_range_t<const_overloaded::ugraph>,
                           std::span<const unsigned int>>);
static_assert(std::same_as<edge_t<const_overloaded::ugraph>, unsigned int>);
static_assert(
    std::same_as<incidence_range_t<const_overloaded::ugraph>,
                 std::span<const std::pair<unsigned int, unsigned int>>>);

GTEST_TEST(undirected_graph, const_overloads_are_the_ones_called) {
    const_overloaded::ugraph graph;

    // a *non-const* object, to prove the CPO takes a const reference of it
    ASSERT_EQ(melon::num_edges(graph), 3u);
    ASSERT_EQ(melon::degree(graph, 0u), 2u);
    ASSERT_TRUE(EQ_RANGES(melon::edges(graph), {0u, 1u, 2u}));
    ASSERT_EQ(melon::edge_endpoints(graph, 1u),
              (std::pair<unsigned int, unsigned int>{1u, 2u}));
    ASSERT_EQ(std::ranges::distance(melon::incidence(graph, 0u)), 3);
}

////////////////////////////////////////////////////////////////////////////////
// the range-returning undirected CPOs reject rvalue graphs whose result would
// dangle -- the mirror of the directed ruling pinned in api_review.cpp -- and
// undirected_graph_all rejects const rvalues instead of deep-copying
////////////////////////////////////////////////////////////////////////////////

namespace rvalue_undirected_cpos {
template <typename G>
concept rvalue_edges = requires { melon::edges(G{}); };
template <typename G>
concept rvalue_incidence =
    requires(melon::vertex_t<G> v) { melon::incidence(G{}, v); };
template <typename T>
concept all_accepted = requires(T && t) {
    melon::views::undirected_graph_all(std::forward<T>(t));
};
}  // namespace rvalue_undirected_cpos

static_assert(!rvalue_undirected_cpos::rvalue_edges<const_overloaded::ugraph>);
static_assert(
    !rvalue_undirected_cpos::rvalue_incidence<const_overloaded::ugraph>);
static_assert(rvalue_undirected_cpos::all_accepted<const_overloaded::ugraph &>);
static_assert(
    rvalue_undirected_cpos::all_accepted<const const_overloaded::ugraph &>);
static_assert(rvalue_undirected_cpos::all_accepted<const_overloaded::ugraph>);
static_assert(
    !rvalue_undirected_cpos::all_accepted<const const_overloaded::ugraph>);
