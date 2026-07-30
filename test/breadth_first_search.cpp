#undef NDEBUG
#include <gtest/gtest.h>

#include <utility>

#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// the traversal visits the vertices reachable from the source in
// breadth-first order, one advance() at a time
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(breadth_first_search, no_arcs_graph) {
    static_digraph_builder<static_digraph> builder(2);

    auto [graph] = builder.build();

    breadth_first_search alg(graph, 0u);

    static_assert(std::movable<decltype(alg)> && !std::copyable<decltype(alg)>);

    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 0u);
    alg.advance();
    ASSERT_TRUE(alg.finished());
    alg.reset();
}

GTEST_TEST(breadth_first_search, test) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(0, 1)
        .add_arc(0, 2)
        .add_arc(0, 5)
        .add_arc(1, 0)
        .add_arc(1, 2)
        .add_arc(1, 3)
        .add_arc(2, 0)
        .add_arc(2, 1)
        .add_arc(2, 3)
        .add_arc(2, 5)
        .add_arc(3, 1)
        .add_arc(3, 2)
        .add_arc(3, 4)
        .add_arc(4, 3)
        .add_arc(4, 5)
        .add_arc(5, 0)
        .add_arc(5, 2)
        .add_arc(5, 4)
        .add_arc(7, 5);

    auto [graph] = builder.build();

    breadth_first_search alg(graph, 0u);

    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 0u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 1u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 2u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 5u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 3u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 4u);
    alg.advance();
    ASSERT_TRUE(alg.finished());
}

////////////////////////////////////////////////////////////////////////////////
// the algorithm is an input range over its own traversal
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(breadth_first_search, algorithm_iterator) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(0, 1)
        .add_arc(0, 2)
        .add_arc(0, 5)
        .add_arc(1, 0)
        .add_arc(1, 2)
        .add_arc(1, 3)
        .add_arc(2, 0)
        .add_arc(2, 1)
        .add_arc(2, 3)
        .add_arc(2, 5)
        .add_arc(3, 1)
        .add_arc(3, 2)
        .add_arc(3, 4)
        .add_arc(4, 3)
        .add_arc(4, 5)
        .add_arc(5, 0)
        .add_arc(5, 2)
        .add_arc(5, 4)
        .add_arc(7, 5);

    auto [graph] = builder.build();

    breadth_first_search alg(graph, 0u);

    static_assert(std::ranges::input_range<decltype(alg)>);
    static_assert(std::ranges::viewable_range<decltype(alg)>);

    std::vector<vertex_t<static_digraph>> traversal = {0u, 1u, 2u, 5u, 3u, 4u};

    std::size_t cpt = 0;
    for(const auto v : alg) {
        ASSERT_EQ(v, traversal[cpt]);
        ++cpt;
    }
}

////////////////////////////////////////////////////////////////////////////////
// traits flags select the stored data, and add_source() resumes a finished
// run from a new root without forgetting what was already reached
////////////////////////////////////////////////////////////////////////////////

struct bfs_traversal_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
    static constexpr bool store_traversal_range = true;
};

GTEST_TEST(breadth_first_search, traversal_traits) {
    static_digraph_builder<static_digraph> builder(9);

    builder.add_arc(0, 1)
        .add_arc(0, 2)
        .add_arc(0, 5)
        .add_arc(1, 0)
        .add_arc(1, 2)
        .add_arc(1, 3)
        .add_arc(2, 0)
        .add_arc(2, 1)
        .add_arc(2, 3)
        .add_arc(2, 5)
        .add_arc(3, 1)
        .add_arc(3, 2)
        .add_arc(3, 4)
        .add_arc(4, 3)
        .add_arc(4, 5)
        .add_arc(5, 0)
        .add_arc(5, 2)
        .add_arc(5, 4)
        .add_arc(7, 3);

    auto [graph] = builder.build();

    breadth_first_search alg(bfs_traversal_traits{}, graph, 0u);
    alg.run();
    ASSERT_TRUE(EQ_MULTISETS(alg.traversal(), {0u, 1u, 2u, 5u, 3u, 4u}));
    for(auto && u : {0u, 1u, 2u, 5u, 3u, 4u}) {
        ASSERT_TRUE(alg.reached(u));
        ASSERT_TRUE(alg.reached_map()[u]);
    }
    for(auto && u : {6u, 7u, 8u}) {
        ASSERT_FALSE(alg.reached(u));
        ASSERT_FALSE(alg.reached_map()[u]);
    }
    alg.add_source(7u).run();
    ASSERT_TRUE(EQ_MULTISETS(alg.traversal(), {7u}));
    for(auto && u : {0u, 1u, 2u, 5u, 3u, 4u, 7u}) {
        ASSERT_TRUE(alg.reached(u));
        ASSERT_TRUE(alg.reached_map()[u]);
    }
    for(auto && u : {6u, 8u}) {
        ASSERT_FALSE(alg.reached(u));
        ASSERT_FALSE(alg.reached_map()[u]);
    }
    alg.add_source(8u).run();
    ASSERT_TRUE(EQ_MULTISETS(alg.traversal(), {8u}));
    for(auto && u : {0u, 1u, 2u, 5u, 3u, 4u, 7u, 8u}) {
        ASSERT_TRUE(alg.reached(u));
        ASSERT_TRUE(alg.reached_map()[u]);
    }
    for(auto && u : {6u}) {
        ASSERT_FALSE(alg.reached(u));
        ASSERT_FALSE(alg.reached_map()[u]);
    }
}

struct bfs_all_traits {
    static constexpr bool store_pred_vertices = true;
    static constexpr bool store_pred_arcs = true;
    static constexpr bool store_distances = true;
    static constexpr bool store_traversal_range = true;
};

GTEST_TEST(breadth_first_search, all_traits) {
    static_digraph_builder<static_digraph> builder(9);

    builder.add_arc(0, 1)
        .add_arc(0, 2)
        .add_arc(0, 5)
        .add_arc(1, 0)
        .add_arc(1, 2)
        .add_arc(1, 3)
        .add_arc(2, 0)
        .add_arc(2, 1)
        .add_arc(2, 3)
        .add_arc(2, 5)
        .add_arc(3, 1)
        .add_arc(3, 2)
        .add_arc(3, 4)
        .add_arc(4, 3)
        .add_arc(4, 5)
        .add_arc(5, 0)
        .add_arc(5, 2)
        .add_arc(5, 4)
        .add_arc(7, 3);

    auto [graph] = builder.build();

    breadth_first_search alg(bfs_all_traits{}, graph, 0u);
    alg.run();
    ASSERT_TRUE(EQ_MULTISETS(alg.traversal(), {0u, 1u, 2u, 5u, 3u, 4u}));
    for(auto && u : {0u, 1u, 2u, 5u, 3u, 4u}) {
        ASSERT_TRUE(alg.reached(u));
        ASSERT_TRUE(alg.reached_map()[u]);
    }
    for(auto && u : {6u, 7u, 8u}) {
        ASSERT_FALSE(alg.reached(u));
        ASSERT_FALSE(alg.reached_map()[u]);
    }
    ASSERT_EQ(alg.dist(0u), 0);
    ASSERT_EQ(alg.dist(1u), 1u);
    ASSERT_EQ(alg.dist(2u), 1u);
    ASSERT_EQ(alg.dist(5u), 1u);
    ASSERT_EQ(alg.dist(3u), 2u);
    ASSERT_EQ(alg.dist(4u), 2u);
    ASSERT_EQ(alg.pred_vertex(1u), 0u);
    ASSERT_EQ(alg.pred_vertex(2u), 0u);
    ASSERT_EQ(alg.pred_vertex(5u), 0u);
    ASSERT_EQ(alg.pred_vertex(3u), 1u);
    ASSERT_EQ(alg.pred_vertex(4u), 5u);
    ASSERT_EQ(alg.pred_arc(1u), 0u);
    ASSERT_EQ(alg.pred_arc(2u), 1u);
    ASSERT_EQ(alg.pred_arc(5u), 2u);
    ASSERT_EQ(alg.pred_arc(3u), 5u);
    ASSERT_EQ(alg.pred_arc(4u), 17u);

    alg.add_source(7u).run();
    ASSERT_TRUE(EQ_MULTISETS(alg.traversal(), {7u}));
    for(auto && u : {0u, 1u, 2u, 5u, 3u, 4u, 7u}) {
        ASSERT_TRUE(alg.reached(u));
        ASSERT_TRUE(alg.reached_map()[u]);
    }
    for(auto && u : {6u, 8u}) {
        ASSERT_FALSE(alg.reached(u));
        ASSERT_FALSE(alg.reached_map()[u]);
    }
    ASSERT_EQ(alg.dist(7u), 0);
    ASSERT_EQ(alg.dist(3u), 2);

    alg.add_source(8u).run();
    ASSERT_TRUE(EQ_MULTISETS(alg.traversal(), {8u}));
    for(auto && u : {0u, 1u, 2u, 5u, 3u, 4u, 7u, 8u}) {
        ASSERT_TRUE(alg.reached(u));
        ASSERT_TRUE(alg.reached_map()[u]);
    }
    for(auto && u : {6u}) {
        ASSERT_FALSE(alg.reached(u));
        ASSERT_FALSE(alg.reached_map()[u]);
    }
    ASSERT_EQ(alg.dist(8u), 0);
}

////////////////////////////////////////////////////////////////////////////////
// each traits flag gates exactly its own members -- no more, no less
////////////////////////////////////////////////////////////////////////////////

// The two extremes above -- all four flags off (the default) and all four on
// -- let a mistake in the per-flag gating hide: any member wrongly gated on a
// *different* flag still appears in the all-on run and still disappears in
// the all-off one. This asks for exactly one flag.
namespace {
struct bfs_distances_only_traits : breadth_first_search_default_traits {
    static constexpr bool store_distances = true;
};

template <typename A>
concept has_dist =
    requires(const A & a, const vertex_t<static_digraph> & u) { a.dist(u); };
template <typename A>
concept has_pred_vertex = requires(
    const A & a, const vertex_t<static_digraph> & u) { a.pred_vertex(u); };
template <typename A>
concept has_traversal = requires(const A & a) { a.traversal_range(); };
}  // namespace

GTEST_TEST(breadth_first_search, store_distances_alone) {
    static_digraph_builder<static_digraph> builder(6);
    builder.add_arc(0, 1)
        .add_arc(0, 2)
        .add_arc(1, 3)
        .add_arc(2, 3)
        .add_arc(3, 4)
        .add_arc(4, 5);
    auto [graph] = builder.build();

    auto alg = breadth_first_search(bfs_distances_only_traits{}, graph, 0u);

    // exactly the one flag asked for, and nothing riding along with it
    static_assert(has_dist<decltype(alg)>);
    static_assert(!has_pred_vertex<decltype(alg)>);
    static_assert(!has_traversal<decltype(alg)>);

    alg.run();

    // BFS distances are hop counts, not arc lengths
    ASSERT_EQ(alg.dist(0u), 0);
    ASSERT_EQ(alg.dist(1u), 1);
    ASSERT_EQ(alg.dist(2u), 1);
    ASSERT_EQ(alg.dist(3u), 2);
    ASSERT_EQ(alg.dist(4u), 3);
    ASSERT_EQ(alg.dist(5u), 4);
}

////////////////////////////////////////////////////////////////////////////////
// move assignment produces an object that owns its buffers
////////////////////////////////////////////////////////////////////////////////

// std::movable only checks that the assignment *declaration* is valid, so the
// static_assert in no_arcs_graph above is satisfied without the operator=
// body ever being instantiated -- which is how the branchless specialisation's
// hand-written assignment once shipped with no return statement at all.
// Assigning one is what forces the body.
namespace {
struct bfs_pred_arcs_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = true;
    static constexpr bool store_distances = false;
    static constexpr bool store_traversal_range = false;
};
}  // namespace

GTEST_TEST(breadth_first_search, is_move_assignable) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(1, 2).add_arc(2, 3);
    auto [graph] = builder.build();

    // default traits over a static_digraph select the branchless specialisation
    static_assert(
        detail::enable_branchless_bfs<views::graph_all_t<static_digraph &>,
                                      breadth_first_search_default_traits>);
    // storing predecessor arcs opts out of it
    static_assert(
        !detail::enable_branchless_bfs<views::graph_all_t<static_digraph &>,
                                       bfs_pred_arcs_traits>);

    breadth_first_search alg(graph, 0u);
    alg.advance();
    ASSERT_EQ(alg.current(), 1u);

    breadth_first_search target(graph, 0u);
    target = std::move(alg);
    ASSERT_EQ(target.current(), 1u);
    ASSERT_TRUE(target.reached(1u));
    ASSERT_FALSE(target.reached(3u));

    // the assigned-to object owns its buffer: it can finish on its own, and
    // the cursor it took over is aimed at that buffer, not the source's
    target.run();
    ASSERT_TRUE(target.reached(3u));
}

GTEST_TEST(breadth_first_search, is_move_assignable_with_pred_arcs) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(1, 2).add_arc(2, 3);
    auto [graph] = builder.build();

    auto reference = breadth_first_search(bfs_pred_arcs_traits{}, graph, 0u);
    reference.run();
    const auto expected_pred_arc = reference.pred_arc(3u);

    auto alg = breadth_first_search(bfs_pred_arcs_traits{}, graph, 0u);
    alg.run();

    auto target = breadth_first_search(bfs_pred_arcs_traits{}, graph, 0u);
    target = std::move(alg);
    ASSERT_TRUE(target.reached(3u));
    ASSERT_EQ(target.pred_arc(3u), expected_pred_arc);
}

////////////////////////////////////////////////////////////////////////////////
// regression: an algorithm lvalue must not be swallowed by the greedy
// single-argument constructor
////////////////////////////////////////////////////////////////////////////////

// `template <typename G> breadth_first_search(G &&)` was unconstrained, so for
// a non-const lvalue of the algorithm type it beat the copy constructor and
// tried to build the algorithm out of itself -- `graph_all` has no overload for
// a breadth_first_search. detail::not_self is what excludes it. Now that copy
// is deleted, the pin is that an algorithm is not constructible from an
// algorithm lvalue *at all*: with not_self gone the greedy constructor would
// quietly become viable again where the deleted copy should be chosen. Both
// specializations carried the original bug, so both are pinned here: default
// traits select the branchless one, bfs_pred_arcs_traits the general one.
GTEST_TEST(breadth_first_search, is_not_constructible_from_an_algorithm) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(1, 2).add_arc(2, 3);
    auto [graph] = builder.build();

    breadth_first_search branchless(graph, 0u);
    auto general = breadth_first_search(bfs_pred_arcs_traits{}, graph, 0u);
    // the two specializations really are different types
    static_assert(!std::same_as<decltype(branchless), decltype(general)>);

    using branchless_t = decltype(branchless);
    using general_t = decltype(general);
    static_assert(!std::is_constructible_v<branchless_t, branchless_t &>);
    static_assert(!std::is_constructible_v<branchless_t, const branchless_t &>);
    static_assert(!std::is_constructible_v<general_t, general_t &>);
    static_assert(!std::is_constructible_v<general_t, const general_t &>);
    // relocating one, on the other hand, is the supported operation
    static_assert(std::is_constructible_v<branchless_t, branchless_t &&>);
    static_assert(std::is_constructible_v<general_t, general_t &&>);

    branchless.advance();
    auto relocated = std::move(branchless);
    ASSERT_EQ(relocated.current(), 1u);
    relocated.run();
    ASSERT_TRUE(relocated.reached(3u));

    general.run();
    auto general_relocated = std::move(general);
    ASSERT_TRUE(general_relocated.reached(3u));
}
