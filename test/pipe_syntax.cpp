#undef NDEBUG
#include <gtest/gtest.h>

#include <concepts>
#include <ranges>
#include <vector>

#include "melon/container/static_digraph.hpp"
#include "melon/graph.hpp"
#include "melon/maps/constant.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/reverse.hpp"
#include "melon/views/subgraph.hpp"
#include "melon/views/undirect.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

using G = static_digraph;

////////////////////////////////////////////////////////////////////////////////
// the pipe spelling and the call spelling name the SAME type, for every adaptor
////////////////////////////////////////////////////////////////////////////////

// The zero-cost contract: because both spellings name one type, every
// downstream instantiation is identical and the pipe cannot cost anything by
// construction.
static_assert(std::same_as<decltype(views::reverse(std::declval<G &>())),
                           decltype(std::declval<G &>() | views::reverse)>);
static_assert(std::same_as<decltype(views::undirect(std::declval<G &>())),
                           decltype(std::declval<G &>() | views::undirect)>);
static_assert(std::same_as<decltype(views::graph_all(std::declval<G &>())),
                           decltype(std::declval<G &>() | views::graph_all)>);
static_assert(std::same_as<decltype(views::subgraph(std::declval<G &>())),
                           decltype(std::declval<G &>() | views::subgraph())>);

// The adaptor object and the class it builds are distinct entities, the
// std::ranges shape: views::reverse is not a type.
static_assert(std::same_as<decltype(views::reverse(std::declval<G &>())),
                           reverse_view<graph_ref_view<G>>>);
static_assert(std::same_as<decltype(views::undirect(std::declval<G &>())),
                           undirect_view<graph_ref_view<G>>>);

GTEST_TEST(pipe_syntax, pipe_and_call_agree_at_runtime) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc({0u, 1u}).add_arc({1u, 2u});
    auto [graph] = builder.build();

    auto piped = graph | views::reverse;
    auto called = views::reverse(graph);
    static_assert(std::same_as<decltype(piped), decltype(called)>);

    ASSERT_EQ(piped.arc_source(0u), called.arc_source(0u));
    ASSERT_EQ(piped.arc_source(0u), 1u);
    ASSERT_EQ(piped.arc_target(0u), 0u);
}

GTEST_TEST(pipe_syntax, graph_all_pipes_an_undirected_graph) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc({0u, 1u}).add_arc({1u, 2u});
    auto [graph] = builder.build();

    // undirect yields an undirected graph; the all-CPO must accept it from
    // a pipe exactly as from a call, and both name the same type.
    auto ug = graph | views::undirect;
    auto w1 = views::graph_all(ug);
    auto w2 = ug | views::graph_all;
    static_assert(std::same_as<decltype(w1), decltype(w2)>);
    ASSERT_EQ(w1.num_edges(), 2u);
}

////////////////////////////////////////////////////////////////////////////////
// a bound closure owns its filters and is reusable
////////////////////////////////////////////////////////////////////////////////

// A bound closure is self-contained: it stores its filters decayed and
// hands out copies (or moves, from an rvalue closure), so applying it
// builds a view *owning* its filter -- unlike the direct call on an lvalue
// filter, which references the caller's. Both value categories of the
// closure therefore build the same type.
using vertex_filter = static_map<vertex_t<G>, bool>;
template <typename Closure>
using piped_t = decltype(std::declval<G &>() | std::declval<Closure>());
using bound_subgraph =
    decltype(views::subgraph(std::declval<vertex_filter &>()));
static_assert(
    std::same_as<piped_t<bound_subgraph &>, piped_t<bound_subgraph &&>>);
static_assert(std::same_as<piped_t<bound_subgraph>,
                           subgraph_view<graph_ref_view<G>,
                                         mapping_owning_view<vertex_filter>,
                                         maps::true_map>>);

GTEST_TEST(pipe_syntax, bound_subgraph_filters_through_the_pipe) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc({0u, 1u}).add_arc({1u, 2u});
    auto [graph] = builder.build();

    auto filter = create_vertex_map<bool>(graph, true);
    filter[2u] = false;

    auto sub = graph | views::subgraph(filter);
    ASSERT_TRUE(EQ_MULTISETS(sub.vertices(), {0u, 1u}));

    // Self-contained: mutating the caller's filter after the pipe does not
    // reach into the view -- it took a copy through the closure.
    filter[1u] = false;
    ASSERT_TRUE(EQ_MULTISETS(sub.vertices(), {0u, 1u}));

    auto adaptor = views::subgraph(filter);
    auto sub2 = graph | adaptor;
    auto sub3 = graph | adaptor;
    static_assert(std::same_as<decltype(sub2), decltype(sub3)>);
    ASSERT_TRUE(EQ_MULTISETS(sub2.vertices(), {0u}));
}

GTEST_TEST(pipe_syntax, induced_subgraph_binds_its_vertex_range) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc({0u, 1u}).add_arc({1u, 2u}).add_arc({2u, 3u});
    auto [graph] = builder.build();

    auto induced =
        graph | views::induced_subgraph(std::vector<unsigned int>{0u, 1u});
    static_assert(std::same_as<decltype(induced),
                               decltype(views::induced_subgraph(
                                   graph, std::vector<unsigned int>{0u, 1u}))>);
    ASSERT_TRUE(EQ_MULTISETS(induced.vertices(), {0u, 1u}));
}

////////////////////////////////////////////////////////////////////////////////
// composition is itself a closure and applies left to right
////////////////////////////////////////////////////////////////////////////////

// Composition associates: g | (c1 | c2) names the type (g | c1) | c2 names.
using composed = decltype(views::reverse | views::subgraph());
static_assert(std::same_as<piped_t<composed>,
                           decltype(std::declval<G &>() | views::reverse |
                                    views::subgraph())>);

GTEST_TEST(pipe_syntax, composition_applies_left_to_right) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc({0u, 1u}).add_arc({1u, 2u});
    auto [graph] = builder.build();

    auto adaptor = views::reverse | views::subgraph();
    auto rsub = graph | adaptor;
    ASSERT_EQ(rsub.arc_source(0u), 1u);
    ASSERT_EQ(rsub.arc_target(0u), 0u);
}

////////////////////////////////////////////////////////////////////////////////
// the pipe rejects non-graphs and unbound multi-argument adaptors
////////////////////////////////////////////////////////////////////////////////

// What must NOT compile: piping a non-graph, and piping the raw
// multi-argument adaptor without binding it first (std::views::filter
// behaves the same way).
template <typename T>
concept pipeable_into_reverse = requires(T t) { t | views::reverse; };
static_assert(!pipeable_into_reverse<int>);
static_assert(pipeable_into_reverse<G &>);

template <typename T, typename A>
concept pipeable_into = requires(T t, const A & a) { t | a; };
static_assert(!pipeable_into<G &, views::subgraph_fn>);
static_assert(!pipeable_into<G &, views::induced_subgraph_fn>);

// Piping garbage into a *bound* closure fails at the pipe, not later.
template <typename T>
concept pipeable_into_bound_subgraph =
    requires(T t, const bound_subgraph & c) { t | c; };
static_assert(!pipeable_into_bound_subgraph<int>);
