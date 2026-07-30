#undef NDEBUG
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "melon/algorithm/dijkstra.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/container/static_map.hpp"
#include "melon/detail/concat_view.hpp"
#include "melon/detail/specialization_of.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/graph_view.hpp"
#include "melon/views/subgraph.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// views::subgraph wraps like graph_all -- ref view for lvalues, owning view
// for rvalues -- and defaults both filters to true_map
////////////////////////////////////////////////////////////////////////////////

template <typename G>
using trivial_subgraph_t = decltype(views::subgraph(std::declval<G>(), {}, {}));

GTEST_TEST(subgraph_views, graph_view) {
    using G = static_digraph;

    static_assert(
        std::same_as<
            trivial_subgraph_t<G &>,
            subgraph_view<graph_ref_view<G>, maps::true_map, maps::true_map>>);
    static_assert(std::same_as<trivial_subgraph_t<const G &>,
                               subgraph_view<graph_ref_view<const G>,
                                             maps::true_map, maps::true_map>>);
    static_assert(std::same_as<trivial_subgraph_t<G>,
                               subgraph_view<graph_owning_view<G>,
                                             maps::true_map, maps::true_map>>);
    static_assert(std::same_as<trivial_subgraph_t<G &&>,
                               subgraph_view<graph_owning_view<G>,
                                             maps::true_map, maps::true_map>>);

    static_assert(graph_view<trivial_subgraph_t<G &>>);
    static_assert(graph_view<trivial_subgraph_t<const G &>>);
    static_assert(graph_view<trivial_subgraph_t<G>>);
    static_assert(graph_view<trivial_subgraph_t<G &&>>);
}

////////////////////////////////////////////////////////////////////////////////
// copying a mutable lvalue view uses the copy constructor, and the copy owns
// its own filter
////////////////////////////////////////////////////////////////////////////////

// regression: subgraph's second and third parameters have defaults, so its
// constructor template is callable with one argument and competed with the
// copy constructor. The guard it already carried was written against the
// *deduced* G, which for an lvalue is `subgraph<...> &` -- a reference is not
// a specialization of anything, so the guard never fired where it was needed.
static_assert(
    !detail::specialization_of<subgraph_view<graph_ref_view<static_digraph>,
                                             maps::true_map, maps::true_map> &,
                               subgraph_view>);

GTEST_TEST(subgraph_views, copying_a_mutable_lvalue_uses_the_copy_constructor) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc(0, 1).add_arc(1, 2);
    auto [graph] = builder.build();

    subgraph_view view(graph, create_vertex_map<bool>(graph, true));

    subgraph_view from_mutable_lvalue(view);
    static_assert(std::same_as<decltype(view), decltype(from_mutable_lvalue)>);

    // a copy, not an alias: the filter is owned by value
    from_mutable_lvalue.disable_vertex(1u);
    ASSERT_TRUE(view.is_valid_vertex(1u));
    ASSERT_FALSE(from_mutable_lvalue.is_valid_vertex(1u));

    decltype(view) from_const_lvalue(std::as_const(view));
    ASSERT_TRUE(from_const_lvalue.is_valid_vertex(1u));

    // a subgraph of a subgraph is a different specialization: still reachable
    subgraph_view<views::graph_all_t<decltype(view) &>, maps::true_map,
                  maps::true_map>
        nested(view);
    ASSERT_TRUE(EQ_RANGES(view.vertices(), nested.vertices()));
}

////////////////////////////////////////////////////////////////////////////////
// an unfiltered subgraph forwards every accessor of the wrapped graph
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(subgraph_views, static_graph) {
    std::vector<
        std::pair<arc_t<static_digraph>, std::pair<vertex_t<static_digraph>,
                                                   vertex_t<static_digraph>>>>
        arc_pairs(
            {{0, {0, 1}}, {1, {0, 2}}, {2, {1, 2}}, {3, {2, 0}}, {4, {2, 1}}});

    static_digraph graph(3, std::views::keys(std::views::values(arc_pairs)),
                         std::views::values(std::views::values(arc_pairs)));

    auto subgraph_graph = views::subgraph(graph);

    ASSERT_TRUE(EQ_RANGES(vertices(graph), subgraph_graph.vertices()));
    ASSERT_TRUE(EQ_RANGES(arcs(graph), subgraph_graph.arcs()));

    for(auto u : vertices(graph)) ASSERT_TRUE(is_valid_vertex(graph, u));
    ASSERT_FALSE(
        is_valid_vertex(graph, vertex_t<static_digraph>(num_vertices(graph))));

    for(auto a : arcs(graph)) ASSERT_TRUE(is_valid_arc(graph, a));
    ASSERT_FALSE(is_valid_arc(graph, arc_t<static_digraph>(num_arcs(graph))));

    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph, 0), subgraph_graph.out_neighbors(0)));
    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph, 1), subgraph_graph.out_neighbors(1)));
    ASSERT_TRUE(
        EQ_RANGES(out_neighbors(graph, 2), subgraph_graph.out_neighbors(2)));

    ASSERT_TRUE(
        EQ_RANGES(in_neighbors(graph, 0), subgraph_graph.in_neighbors(0)));
    ASSERT_TRUE(
        EQ_RANGES(in_neighbors(graph, 1), subgraph_graph.in_neighbors(1)));
    ASSERT_TRUE(
        EQ_RANGES(in_neighbors(graph, 2), subgraph_graph.in_neighbors(2)));

    ASSERT_TRUE(EQ_RANGES(arcs_entries(graph), arc_pairs));

    for(arc_t<static_digraph> a : arcs(graph)) {
        ASSERT_EQ(arc_source(graph, a), arc_pairs[a].second.first);
    }
}

////////////////////////////////////////////////////////////////////////////////
// a false arc-filter entry removes the arc from every adjacency range
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(subgraph_views, static_graph_filter) {
    static_digraph_builder<static_digraph, char> builder(6);

    builder.add_arc(0, 1, 1);
    builder.add_arc(0, 2, 0);
    builder.add_arc(1, 0, 0);
    builder.add_arc(1, 2, 1);
    builder.add_arc(1, 3, 0);
    builder.add_arc(2, 0, 0);
    builder.add_arc(2, 1, 1);
    builder.add_arc(2, 3, 1);
    builder.add_arc(3, 1, 0);
    builder.add_arc(3, 2, 1);

    auto [fgraph, filter_map] = builder.build();
    auto graph =
        views::subgraph(fgraph, maps::true_map{}, std::move(filter_map));

    ASSERT_TRUE(EQ_RANGES(graph.out_neighbors(0), {1}));
    ASSERT_TRUE(EQ_RANGES(graph.out_neighbors(1), {2}));
    ASSERT_TRUE(EQ_RANGES(graph.out_neighbors(2), {1, 3}));
    ASSERT_TRUE(EQ_RANGES(graph.out_neighbors(3), {2}));
}

////////////////////////////////////////////////////////////////////////////////
// the filters are mutable through a non-const view only

// regression 2.8: disable_arc / enable_arc were `const` while disable_vertex /
// enable_vertex were not. Two things followed. The pair could be called on a
// `const subgraph &`, writing the filter -- part of the view's value --
// through a const reference; and with the mapping_owning_view filter that
// views::subgraph(g, maps::true_map{}, map) produces, they did not compile at
// all, because a const member cannot write through an owning view. All four
// are non-const now.
namespace arc_filter_constness {
using G = static_digraph;
using owning_sub =
    decltype(views::subgraph(std::declval<G &>(), maps::true_map{},
                             std::declval<static_map<arc_t<G>, bool> &&>()));

template <typename S>
concept mutable_through_const = requires(const S & s, arc_t<G> a) {
    s.disable_arc(a);
} || requires(const S & s, vertex_t<G> v) { s.disable_vertex(v); };
}  // namespace arc_filter_constness

// the owning-filter subgraph can be written at all -- this is what the const
// used to make impossible
static_assert(requires(arc_filter_constness::owning_sub & s,
                       arc_t<static_digraph> a) {
    s.disable_arc(a);
    s.enable_arc(a);
});
// ... and not through a const reference, for either half of the pair
static_assert(!arc_filter_constness::mutable_through_const<
              arc_filter_constness::owning_sub>);

GTEST_TEST(subgraph_views,
           disabling_and_reenabling_an_arc_of_an_owning_filter) {
    const std::vector<std::pair<unsigned int, unsigned int>> arc_pairs(
        {{0u, 1u}, {0u, 2u}, {1u, 2u}, {2u, 0u}, {2u, 1u}});
    static_digraph graph(3, std::views::keys(arc_pairs),
                         std::views::values(arc_pairs));

    auto sub = views::subgraph(graph, maps::true_map{},
                               create_arc_map<bool>(graph, true));
    ASSERT_TRUE(EQ_RANGES(arcs(sub), {0u, 1u, 2u, 3u, 4u}));

    sub.disable_arc(2);
    ASSERT_TRUE(EQ_RANGES(arcs(sub), {0u, 1u, 3u, 4u}));
    ASSERT_FALSE(is_valid_arc(sub, 2u));

    sub.enable_arc(2);
    ASSERT_TRUE(EQ_RANGES(arcs(sub), {0u, 1u, 2u, 3u, 4u}));
    ASSERT_TRUE(is_valid_arc(sub, 2u));
}

////////////////////////////////////////////////////////////////////////////////
// the endpoint maps come from the wrapped graph, unfiltered
////////////////////////////////////////////////////////////////////////////////

// 2.2: subgraph was the one view in the family that already spelled the
// endpoint maps the way the CPOs probe for them, so it kept the wrapped
// container's maps while graph_ref_view / graph_owning_view / reverse silently
// dropped to the synthesised lambda. Pinned here so the three stay in step.
// The maps are deliberately *unfiltered*: they answer for every arc of the
// wrapped graph, and it is arcs() that decides which of those are in the
// subgraph, exactly as arc_source / arc_target do.
namespace subgraph_endpoint_maps {
using G = static_digraph;
using S = decltype(views::subgraph(std::declval<G &>()));
}  // namespace subgraph_endpoint_maps

static_assert(
    melon::cpo::has_member_arc_sources_map<subgraph_endpoint_maps::S>);
static_assert(
    melon::cpo::has_member_arc_targets_map<subgraph_endpoint_maps::S>);
static_assert(
    std::same_as<decltype(arc_sources_map(
                     std::declval<const subgraph_endpoint_maps::S &>())),
                 decltype(arc_sources_map(
                     std::declval<const subgraph_endpoint_maps::G &>()))>);
static_assert(
    std::same_as<decltype(arc_targets_map(
                     std::declval<const subgraph_endpoint_maps::S &>())),
                 decltype(arc_targets_map(
                     std::declval<const subgraph_endpoint_maps::G &>()))>);

GTEST_TEST(subgraph_views, endpoint_maps_come_from_the_wrapped_graph) {
    const std::vector<std::pair<unsigned int, unsigned int>> arc_pairs(
        {{0u, 1u}, {0u, 2u}, {1u, 2u}, {2u, 0u}, {2u, 1u}});
    static_digraph graph(3, std::views::keys(arc_pairs),
                         std::views::values(arc_pairs));

    // arc 2 filtered out. Seeded rather than disable_arc()'d: that member is
    // `const` (2.8) and so cannot write an owning filter map.
    auto arc_filter = create_arc_map<bool>(graph, true);
    arc_filter[2] = false;
    auto sub = views::subgraph(graph, maps::true_map{}, std::move(arc_filter));

    const auto sources = arc_sources_map(sub);
    const auto targets = arc_targets_map(sub);
    for(const arc_t<static_digraph> a : arcs(graph)) {
        ASSERT_EQ(sources[a], arc_source(graph, a));
        ASSERT_EQ(targets[a], arc_target(graph, a));
    }
    ASSERT_TRUE(EQ_RANGES(arcs(sub), {0u, 1u, 3u, 4u}));
}

////////////////////////////////////////////////////////////////////////////////
// algorithms consume a subgraph like any graph
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(subgraph_views, dijkstra) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 7);
    builder.add_arc(0, 2, 9);
    builder.add_arc(0, 5, 14);
    builder.add_arc(1, 0, 7);
    builder.add_arc(1, 2, 10);
    builder.add_arc(1, 3, 15);
    builder.add_arc(2, 0, 9);
    builder.add_arc(2, 1, 10);
    builder.add_arc(2, 3, 12);
    builder.add_arc(2, 5, 2);
    builder.add_arc(3, 1, 15);
    builder.add_arc(3, 2, 12);
    builder.add_arc(3, 4, 6);
    builder.add_arc(4, 3, 6);
    builder.add_arc(4, 5, 9);
    builder.add_arc(5, 0, 14);
    builder.add_arc(5, 2, 2);
    builder.add_arc(5, 4, 9);

    auto [fgraph, length_map] = builder.build();
    auto graph = views::subgraph(fgraph);

    auto alg = dijkstra(graph, length_map);

    alg.add_source(0);
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(0u, 0));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(1u, 7));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(2u, 9));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(5u, 11));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(4u, 20));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(3u, 21));
    alg.advance();
    ASSERT_TRUE(alg.finished());
}

////////////////////////////////////////////////////////////////////////////////
// induced_subgraph restricts to a vertex set and hides the arcs leaving it
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(induces_subgraph_views, test) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 7);
    builder.add_arc(0, 2, 9);  //
    builder.add_arc(0, 5, 14);
    builder.add_arc(1, 0, 7);
    builder.add_arc(1, 2, 10);  //
    builder.add_arc(1, 3, 15);  //
    builder.add_arc(2, 0, 9);   //
    builder.add_arc(2, 1, 10);  //
    builder.add_arc(2, 3, 12);  //
    builder.add_arc(2, 5, 2);   //
    builder.add_arc(3, 1, 15);  //
    builder.add_arc(3, 2, 12);  //
    builder.add_arc(3, 4, 6);   //
    builder.add_arc(4, 3, 6);   //
    builder.add_arc(4, 5, 9);
    builder.add_arc(5, 0, 14);
    builder.add_arc(5, 2, 2);  //
    builder.add_arc(5, 4, 9);

    auto [fgraph, length_map] = builder.build();
    auto graph = views::induced_subgraph(
        fgraph, detail::views::concat(std::views::iota(0u, 2u),
                                      std::views::iota(4u, 6u)));

    auto alg = dijkstra(graph, length_map);

    alg.add_source(0u);
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(0u, 0));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(1u, 7));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(5u, 14));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(4u, 23));
    alg.advance();
    ASSERT_TRUE(alg.finished());
}

////////////////////////////////////////////////////////////////////////////////
// induced_subgraph is a movable, assignable graph_view that keeps its vertex
// set immutable
////////////////////////////////////////////////////////////////////////////////

namespace {
using induced_t =
    induced_subgraph_view<graph_ref_view<static_digraph>,
                          std::ranges::ref_view<std::vector<unsigned int>>>;

template <typename T>
concept can_disable_vertex =
    requires(T t, vertex_t<T> v) { t.disable_vertex(v); };
}  // namespace

// regression: the filter member used to be spelled `const
// mapping_owning_view<...>`. A const member deletes the defaulted assignments,
// so induced_subgraph failed std::movable and therefore graph_view, and
// views::graph_all stopped passing an rvalue through: it wrapped the whole
// view in a graph_owning_view instead.
static_assert(graph<induced_t>);
static_assert(enable_graph_view<induced_t>);
static_assert(std::movable<induced_t>);
static_assert(graph_view<induced_t>);
static_assert(std::is_copy_assignable_v<induced_t>);
static_assert(std::is_move_assignable_v<induced_t>);

// which is what lets graph_all pass it through instead of wrapping it
static_assert(std::same_as<views::graph_all_t<induced_t>, induced_t>);
static_assert(std::same_as<views::graph_all_t<induced_t &>, induced_t>);

// Dropping the const would have handed callers the base's enable/disable, and
// the filter and vertices() are two spellings of one set. They stay hidden.
static_assert(!can_disable_vertex<induced_t>);
static_assert(
    can_disable_vertex<subgraph_view<
        graph_ref_view<static_digraph>,
        maps::mapping_all_t<static_map<unsigned, bool>>, maps::true_map>>);

GTEST_TEST(subgraph_views, induced_subgraph_is_assignable) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(1, 2).add_arc(2, 3);
    auto [graph] = builder.build();

    std::vector<unsigned int> first = {0u, 1u, 2u};
    std::vector<unsigned int> second = {2u, 3u};

    auto view = views::induced_subgraph(graph, first);
    auto other = views::induced_subgraph(graph, second);

    ASSERT_TRUE(view.is_valid_vertex(0u));
    ASSERT_FALSE(view.is_valid_vertex(3u));

    other = view;  // used not to compile at all
    ASSERT_TRUE(other.is_valid_vertex(0u));
    ASSERT_FALSE(other.is_valid_vertex(3u));
    ASSERT_TRUE(EQ_RANGES(other.vertices(), first));
}

// The filter is built from the graph's vertex count while the graph is still
// intact: subgraph takes it by forwarding reference, so nothing is moved out
// of it until the base's mem-initializer runs. An rvalue graph is the case
// that would expose it if that ever stopped being true.
GTEST_TEST(subgraph_views, induced_subgraph_over_an_rvalue_graph) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(1, 2).add_arc(2, 3);
    auto [graph] = builder.build();

    std::vector<unsigned int> keep = {0u, 1u, 2u};
    auto view = views::induced_subgraph(static_digraph(graph), keep);

    ASSERT_TRUE(view.is_valid_vertex(0u));
    ASSERT_TRUE(view.is_valid_vertex(1u));
    ASSERT_TRUE(view.is_valid_vertex(2u));
    ASSERT_FALSE(view.is_valid_vertex(3u));
    ASSERT_TRUE(EQ_RANGES(view.vertices(), keep));
}
