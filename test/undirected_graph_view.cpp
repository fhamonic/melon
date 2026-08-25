#undef NDEBUG
#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <utility>

#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/undirect.hpp"
#include "melon/views/undirected_graph_view.hpp"

#include "ranges_test_helper.hpp"
#include "undirected_triangle.hpp"

using namespace melon;

using triangle = adl_ugraph::triangle;
using ref_view = undirected_graph_ref_view<triangle>;
using owning_view = undirected_graph_owning_view<triangle>;

////////////////////////////////////////////////////////////////////////////////
// the two adaptors model undirected_graph_view; a bare undirected graph does
// not
////////////////////////////////////////////////////////////////////////////////

static_assert(!undirected_graph_view<triangle>);
static_assert(undirected_graph_view<ref_view>);
static_assert(undirected_graph_view<owning_view>);
static_assert(undirected_graph<ref_view>);
static_assert(undirected_graph<owning_view>);

// A view is cheap to copy/move around; the owning one is move-only.
static_assert(std::is_copy_constructible_v<ref_view>);
static_assert(std::is_copy_assignable_v<ref_view>);
static_assert(!std::is_copy_constructible_v<owning_view>);
static_assert(std::is_move_constructible_v<owning_view>);

static_assert(
    std::same_as<views::undirected_graph_all_t<triangle &>, ref_view>);
static_assert(
    std::same_as<views::undirected_graph_all_t<triangle>, owning_view>);
static_assert(std::same_as<views::undirected_graph_all_t<ref_view>, ref_view>);

////////////////////////////////////////////////////////////////////////////////
// the ref view is a shallow-const, rebindable handle that forwards every CPO
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(undirected_graph_ref_view, forwards_every_cpo) {
    triangle g;
    ref_view view(g);

    ASSERT_EQ(std::addressof(view.base()), std::addressof(g));

    ASSERT_EQ(melon::num_vertices(view), melon::num_vertices(g));
    ASSERT_EQ(melon::num_edges(view), melon::num_edges(g));
    ASSERT_TRUE(EQ_RANGES(melon::vertices(view), melon::vertices(g)));
    ASSERT_TRUE(EQ_RANGES(melon::edges(view), melon::edges(g)));

    for(const auto & e : melon::edges(view))
        ASSERT_EQ(melon::edge_endpoints(view, e), melon::edge_endpoints(g, e));
    for(const auto & v : melon::vertices(view))
        ASSERT_TRUE(
            EQ_RANGES(melon::incidence(view, v), melon::incidence(g, v)));

    auto vmap = melon::create_vertex_map<int>(view, 3);
    auto emap = melon::create_edge_map<int>(view, 4);
    ASSERT_EQ(vmap[0u], 3);
    ASSERT_EQ(emap[0u], 4);
}

// regression: the forwarding interface forwards degree like every other
// accessor of the protocol. The fixture's incidence range is deliberately
// unsized, so the CPO's sized-incidence fallback cannot answer either and,
// without the forwarding member, the views have no degree at all.
GTEST_TEST(undirected_graph_ref_view, forwards_degree) {
    static_assert(has_degree<triangle>);
    static_assert(has_degree<ref_view>);
    static_assert(has_degree<owning_view>);

    triangle g;
    ref_view view(g);
    for(const auto & v : melon::vertices(view))
        ASSERT_EQ(melon::degree(view, v), melon::degree(g, v));
}

// Shallow constness: the view is a handle, so a const view still hands out the
// underlying graph as a mutable reference.
GTEST_TEST(undirected_graph_ref_view, is_shallow_const) {
    triangle g;
    const ref_view view(g);

    static_assert(std::same_as<decltype(view.base()), triangle &>);
    view.base().edge_list.emplace_back(0, 2);
    ASSERT_EQ(melon::num_edges(view), 4u);
}

GTEST_TEST(undirected_graph_ref_view, is_rebindable) {
    triangle g1;
    triangle g2;
    g2.edge_list.pop_back();

    ref_view view(g1);
    ASSERT_EQ(melon::num_edges(view), 3u);
    view = ref_view(g2);
    ASSERT_EQ(melon::num_edges(view), 2u);
}

////////////////////////////////////////////////////////////////////////////////
// the owning view owns its graph, forwards to it, and hands ownership back out
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(undirected_graph_owning_view, owns_and_forwards) {
    owning_view view{triangle{}};

    ASSERT_EQ(melon::num_vertices(view), 3u);
    ASSERT_EQ(melon::num_edges(view), 3u);
    ASSERT_TRUE(EQ_RANGES(melon::edges(view), {0, 1, 2}));
    ASSERT_EQ(melon::edge_endpoints(view, 2u), adl_ugraph::endpoints(2, 0));
    for(const auto & v : melon::vertices(view))
        ASSERT_EQ(std::ranges::distance(melon::incidence(view, v)), 2);

    auto emap = melon::create_edge_map<double>(view, 1.5);
    ASSERT_EQ(emap.size(), 3u);
    ASSERT_DOUBLE_EQ(emap[1u], 1.5);
}

// The owned graph survives the move, unlike the ref view's dangling-prone
// pointer, and base() && hands the ownership back out.
GTEST_TEST(undirected_graph_owning_view, survives_a_move) {
    owning_view view{triangle{}};
    owning_view moved{std::move(view)};
    ASSERT_EQ(melon::num_edges(moved), 3u);

    triangle recovered = std::move(moved).base();
    ASSERT_EQ(recovered.edge_list.size(), 3u);
}

////////////////////////////////////////////////////////////////////////////////
// undirected_graph_all selects the right adaptor and never wraps a view twice
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(undirected_graph_all, selects_the_right_adaptor) {
    triangle g;

    auto from_lvalue = views::undirected_graph_all(g);
    static_assert(std::same_as<decltype(from_lvalue), ref_view>);
    ASSERT_EQ(std::addressof(from_lvalue.base()), std::addressof(g));

    auto from_rvalue = views::undirected_graph_all(triangle{});
    static_assert(std::same_as<decltype(from_rvalue), owning_view>);
    ASSERT_EQ(melon::num_edges(from_rvalue), 3u);

    auto passed_through = views::undirected_graph_all(ref_view(g));
    static_assert(std::same_as<decltype(passed_through), ref_view>);
    ASSERT_EQ(std::addressof(passed_through.base()), std::addressof(g));
}

// views::undirect already models undirected_graph_view, so wrapping it must be
// a no-op rather than another layer of indirection.
GTEST_TEST(undirected_graph_all, passes_undirect_through) {
    static_digraph_builder<static_digraph, int> builder(3);
    builder.add_arc(0u, 1u, 1).add_arc(1u, 2u, 2).add_arc(2u, 0u, 3);
    auto [digraph, weights] = builder.build();

    auto undirected = views::undirect(digraph);
    static_assert(undirected_graph_view<decltype(undirected)>);

    auto all = views::undirected_graph_all(undirected);
    static_assert(std::same_as<decltype(all), decltype(undirected)>);
    ASSERT_EQ(melon::num_edges(all), 3u);
}

////////////////////////////////////////////////////////////////////////////////
// copying a mutable lvalue view uses the copy constructor
////////////////////////////////////////////////////////////////////////////////

// regression: same trap as graph_ref_view -- a greedy single-argument
// constructor's guard tested against the deduced T never fires for a mutable
// lvalue, whose deduced T is a reference type.
static_assert(std::copy_constructible<ref_view>);
static_assert(std::constructible_from<ref_view, triangle &>);
static_assert(std::constructible_from<ref_view, ref_view &>);
static_assert(std::constructible_from<undirected_graph_ref_view<const triangle>,
                                      triangle &>);

// what the paired convertible_to constraint rejects cleanly
static_assert(!std::constructible_from<
              ref_view, undirected_graph_ref_view<const triangle> &>);
static_assert(!std::constructible_from<ref_view, int &>);

GTEST_TEST(undirected_graph_view,
           copying_a_mutable_lvalue_uses_the_copy_constructor) {
    triangle graph;
    ref_view view(graph);

    ref_view from_mutable_lvalue(view);
    ASSERT_EQ(std::addressof(from_mutable_lvalue.base()),
              std::addressof(graph));
    ASSERT_EQ(num_edges(from_mutable_lvalue), num_edges(graph));

    ref_view from_const_lvalue(std::as_const(view));
    ASSERT_EQ(std::addressof(from_const_lvalue.base()), std::addressof(graph));
}
