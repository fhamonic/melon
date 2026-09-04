#undef NDEBUG
#include <gtest/gtest.h>

#include <utility>

#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/algorithm/depth_first_search.hpp"
#include "melon/container/mutable_digraph.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// the traversal visits the vertices reachable from the source in
// depth-first order, one advance() at a time
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(depth_first_search, no_arcs_graph) {
    static_digraph_builder<static_digraph> builder(2);

    auto [graph] = builder.build();

    depth_first_search alg(graph, 0u);

    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 0u);
    alg.advance();
    ASSERT_TRUE(alg.finished());
    alg.reset();
}

GTEST_TEST(depth_first_search, test) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc({0, 1})
        .add_arc({0, 2})
        .add_arc({0, 3})
        .add_arc({1, 3})
        .add_arc({2, 4})
        .add_arc({3, 5})
        .add_arc({3, 6})
        .add_arc({4, 7})
        .add_arc({4, 5})
        .add_arc({5, 2});

    auto [graph] = builder.build();

    depth_first_search alg(graph, 0u);

    static_assert(std::movable<decltype(alg)> && !std::copyable<decltype(alg)>);

    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 0u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 1u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 3u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 5u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 2u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 4u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 7u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 6u);
    alg.advance();
    ASSERT_TRUE(alg.finished());
}

////////////////////////////////////////////////////////////////////////////////
// the algorithm is an input range over its own traversal
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(depth_first_search, algorithm_iterator) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc({0, 1})
        .add_arc({0, 2})
        .add_arc({0, 3})
        .add_arc({1, 3})
        .add_arc({2, 4})
        .add_arc({3, 5})
        .add_arc({3, 6})
        .add_arc({4, 7})
        .add_arc({4, 5})
        .add_arc({5, 2});

    auto [graph] = builder.build();

    depth_first_search alg(graph, 0u);

    static_assert(std::ranges::input_range<decltype(alg)>);
    static_assert(std::ranges::viewable_range<decltype(alg)>);

    std::vector<vertex_t<static_digraph>> traversal = {0u, 1u, 3u, 5u,
                                                       2u, 4u, 7u, 6u};

    std::size_t cpt = 0;
    for(const auto v : alg) {
        ASSERT_EQ(v, traversal[cpt]);
        ++cpt;
    }
}

////////////////////////////////////////////////////////////////////////////////
// the store_pred_* traits flags record the DFS tree, each gating exactly its
// own accessor
////////////////////////////////////////////////////////////////////////////////

// depth_first_search_default_traits sets all three flags to false, so
// _pred_vertices_map, _pred_arcs_map and _depth_map -- and the accessors
// gated on them -- are instantiated nowhere but here.
//
// Constructed by spelling the type out rather than with the
// `depth_first_search(traits{}, graph, source)` form the other algorithms
// accept: depth_first_search declares deduction guides taking a Traits object
// but has no matching constructor, so that form does not compile today.
namespace {
struct dfs_traits_with_preds : depth_first_search_default_traits {
    static constexpr bool store_pred_vertices = true;
    static constexpr bool store_pred_arcs = true;
    static constexpr bool store_depth = true;
};
struct dfs_traits_pred_vertices_only : depth_first_search_default_traits {
    static constexpr bool store_pred_vertices = true;
};

template <typename A>
concept has_pred_vertex = requires(
    const A & a, const vertex_t<static_digraph> & u) { a.pred_vertex(u); };
template <typename A>
concept has_pred_arc = requires(
    const A & a, const vertex_t<static_digraph> & u) { a.pred_arc(u); };
template <typename A>
concept has_depth =
    requires(const A & a, const vertex_t<static_digraph> & u) { a.depth(u); };
}  // namespace

GTEST_TEST(depth_first_search, store_pred_traits) {
    // An out-tree: every vertex but the root has exactly one incoming arc, so
    // the predecessors are the same whatever order DFS happens to explore in.
    static_digraph_builder<static_digraph> builder(5);
    builder.add_arc({0, 1}).add_arc({0, 2}).add_arc({1, 3}).add_arc({2, 4});
    auto [graph] = builder.build();

    depth_first_search<graph_ref_view<static_digraph>, dfs_traits_with_preds>
        alg(graph, 0u);

    using with_preds = decltype(alg);
    using without = decltype(depth_first_search(graph, 0u));
    static_assert(has_pred_vertex<with_preds>);
    static_assert(has_pred_arc<with_preds>);
    // control: the default traits withdraw both, so the assertions above are
    // not passing merely because the member names are misspelled
    static_assert(!has_pred_vertex<without>);
    static_assert(!has_pred_arc<without>);

    alg.run();

    ASSERT_EQ(alg.pred_vertex(1u), 0u);
    ASSERT_EQ(alg.pred_vertex(2u), 0u);
    ASSERT_EQ(alg.pred_vertex(3u), 1u);
    ASSERT_EQ(alg.pred_vertex(4u), 2u);

    // compared through the endpoints rather than an arc id, so the test does
    // not depend on how the builder numbers the arcs
    for(const auto & [v, pred] :
        {std::pair{1u, 0u}, {2u, 0u}, {3u, 1u}, {4u, 2u}}) {
        ASSERT_EQ(arc_source(graph, alg.pred_arc(v)), pred);
        ASSERT_EQ(arc_target(graph, alg.pred_arc(v)), v);
    }
}

GTEST_TEST(depth_first_search, store_pred_vertices_alone) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc({0, 1}).add_arc({1, 2});
    auto [graph] = builder.build();

    depth_first_search<graph_ref_view<static_digraph>,
                       dfs_traits_pred_vertices_only>
        alg(graph, 0u);

    // the flags are independent: asking for predecessor vertices must not
    // drag in the predecessor arc map, whose static_assert additionally
    // requires an outward_incidence_graph
    static_assert(has_pred_vertex<decltype(alg)>);
    static_assert(!has_pred_arc<decltype(alg)>);

    alg.run();
    ASSERT_EQ(alg.pred_vertex(1u), 0u);
    ASSERT_EQ(alg.pred_vertex(2u), 1u);
}

////////////////////////////////////////////////////////////////////////////////
// store_depth records the depth along the DFS tree, and rides on no other flag
////////////////////////////////////////////////////////////////////////////////

// depth() is gated on store_depth, and store_depth alone: it must not ride in
// on either of the predecessor flags, nor drag them in.
namespace {
struct dfs_traits_depth_only : depth_first_search_default_traits {
    static constexpr bool store_depth = true;
};
}  // namespace

GTEST_TEST(depth_first_search, store_depth_alone) {
    // A path: the DFS tree is forced, so the depths are unambiguous.
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc({0, 1}).add_arc({1, 2}).add_arc({2, 3});
    auto [graph] = builder.build();

    depth_first_search<graph_ref_view<static_digraph>, dfs_traits_depth_only>
        alg(graph, 0u);

    static_assert(has_depth<decltype(alg)>);
    static_assert(!has_pred_vertex<decltype(alg)>);
    static_assert(!has_pred_arc<decltype(alg)>);
    // control: the default traits withdraw depth()
    static_assert(!has_depth<decltype(depth_first_search(graph, 0u))>);

    alg.run();
    ASSERT_EQ(alg.depth(0u), 0);
    ASSERT_EQ(alg.depth(1u), 1);
    ASSERT_EQ(alg.depth(2u), 2);
    ASSERT_EQ(alg.depth(3u), 3);
}

GTEST_TEST(depth_first_search, depth_equals_pred_chain_length) {
    // The defining invariant: depth(u) is the number of arcs from the source
    // to u *along the DFS tree*, so it must agree with walking pred_vertex up
    // to the root -- whatever shape the tree happens to take. Checked on a
    // graph with several routes to the same vertex, where the DFS tree is not
    // forced by the structure.
    static_digraph_builder<static_digraph> builder(7);
    builder.add_arc({0, 1})
        .add_arc({0, 2})
        .add_arc({1, 3})
        .add_arc({2, 3})
        .add_arc({3, 4})
        .add_arc({4, 5})
        .add_arc({2, 5})
        .add_arc({5, 6});
    auto [graph] = builder.build();

    depth_first_search<graph_ref_view<static_digraph>, dfs_traits_with_preds>
        alg(graph, 0u);
    alg.run();

    for(const vertex_t<static_digraph> & v : vertices(graph)) {
        ASSERT_TRUE(alg.reached(v));
        int chain = 0;
        for(auto u = v; u != alg.pred_vertex(u); u = alg.pred_vertex(u))
            ++chain;
        ASSERT_EQ(alg.depth(v), chain) << "vertex " << v;
    }
}

////////////////////////////////////////////////////////////////////////////////
// regression: depth is a tree depth, not a shortest-hop distance
////////////////////////////////////////////////////////////////////////////////

// The flag is called store_depth, not store_distances: that name invites
// reading it as the shortest-hop distance breadth_first_search::dist()
// returns. It is not: it counts arcs along the route DFS took. This pins that
// difference down, so that "fixing" depth() into a distance fails here rather
// than silently changing what the flag means.
namespace {
struct bfs_dist_traits : breadth_first_search_default_traits {
    static constexpr bool store_distances = true;
};
}  // namespace

GTEST_TEST(depth_first_search, depth_is_not_the_shortest_hop_distance) {
    // 0->1, 0->2, 2->1, 1->3. Vertex 1 is one hop from the source, but if DFS
    // descends through 2 first it reaches 1 at depth 2.
    mutable_digraph graph;
    const auto v0 = graph.create_vertex();
    const auto v1 = graph.create_vertex();
    const auto v2 = graph.create_vertex();
    const auto v3 = graph.create_vertex();
    (void)graph.create_arc(v0, v1);
    (void)graph.create_arc(v0, v2);
    (void)graph.create_arc(v2, v1);
    (void)graph.create_arc(v1, v3);

    depth_first_search<graph_ref_view<mutable_digraph>, dfs_traits_depth_only>
        dfs(graph, v0);
    dfs.run();

    auto bfs = breadth_first_search(bfs_dist_traits{}, graph, v0);
    bfs.run();

    // the true shortest-hop distances
    ASSERT_EQ(bfs.dist(v1), 1);
    ASSERT_EQ(bfs.dist(v3), 2);

    // DFS descended 0 -> 2 -> 1 -> 3, so its depths are strictly larger
    ASSERT_EQ(dfs.depth(v2), 1);
    ASSERT_EQ(dfs.depth(v1), 2);
    ASSERT_EQ(dfs.depth(v3), 3);
    ASSERT_GT(dfs.depth(v1), bfs.dist(v1));
    ASSERT_GT(dfs.depth(v3), bfs.dist(v3));
}

////////////////////////////////////////////////////////////////////////////////
// regression: an algorithm lvalue must not be swallowed by the greedy
// single-argument constructor
////////////////////////////////////////////////////////////////////////////////

// An unconstrained `depth_first_search(G &&)` beats the copy constructor for
// a non-const lvalue and tries to build the algorithm out of itself;
// detail::not_self is what excludes it. With copy deleted, the pin is that an
// algorithm is not constructible from an algorithm lvalue at all -- without
// not_self the greedy constructor would quietly become viable again where the
// deleted copy constructor should be chosen.
GTEST_TEST(depth_first_search, is_not_constructible_from_an_algorithm) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc({0, 1}).add_arc({1, 2}).add_arc({2, 3});
    auto [graph] = builder.build();

    depth_first_search alg(graph, 0u);
    using alg_t = decltype(alg);
    static_assert(!std::is_constructible_v<alg_t, alg_t &>);
    static_assert(!std::is_constructible_v<alg_t, const alg_t &>);
    static_assert(std::is_constructible_v<alg_t, alg_t &&>);

    alg.advance();
    auto relocated = std::move(alg);
    ASSERT_EQ(relocated.current(), 1u);
    relocated.run();
    ASSERT_TRUE(relocated.reached(3u));
}
