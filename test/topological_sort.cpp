#undef NDEBUG
#include <gtest/gtest.h>

#include <utility>

#include "melon/algorithm/topological_sort.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/container/static_forward_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// the sort yields every vertex of a DAG in a topological order, one
// advance() at a time
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(topological_sort, no_arcs_graph) {
    static_digraph_builder<static_digraph> builder(2);

    auto [graph] = builder.build();

    topological_sort alg(graph);

    static_assert(std::movable<decltype(alg)> && !std::copyable<decltype(alg)>);

    std::vector<vertex_t<static_digraph>> traversal = {0u, 1u};

    std::size_t cpt = 0;
    for(const auto v : alg) {
        ASSERT_EQ(v, traversal[cpt]);
        cpt++;
    }

    for(const auto v : alg) {
        ASSERT_EQ(v, traversal[cpt]);
        cpt++;
    }
}

GTEST_TEST(topological_sort, test) {
    static_digraph_builder<static_digraph> builder(6);

    builder.add_arc(5, 2)
        .add_arc(5, 0)
        .add_arc(4, 0)
        .add_arc(4, 1)
        .add_arc(2, 3)
        .add_arc(3, 1);

    auto [graph] = builder.build();

    topological_sort alg(graph);

    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 4u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 5u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 0u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 2u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 3u);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 1u);
    alg.advance();
    ASSERT_TRUE(alg.finished());
}

////////////////////////////////////////////////////////////////////////////////
// the algorithm is an input range over the sorted order
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(topological_sort, algorithm_iterator) {
    static_digraph_builder<static_digraph> builder(6);

    builder.add_arc(5, 2)
        .add_arc(5, 0)
        .add_arc(4, 0)
        .add_arc(4, 1)
        .add_arc(2, 3)
        .add_arc(3, 1);

    auto [graph] = builder.build();

    topological_sort alg(graph);

    static_assert(std::ranges::input_range<decltype(alg)>);
    static_assert(std::ranges::viewable_range<decltype(alg)>);

    std::vector<vertex_t<static_digraph>> traversal = {4u, 5u, 0u, 2u, 3u, 1u};

    std::size_t cpt = 0;
    for(const auto v : alg) {
        ASSERT_EQ(v, traversal[cpt]);
        ++cpt;
    }
}

////////////////////////////////////////////////////////////////////////////////
// store_ranks: rank() is the longest-path level of a vertex, gated on its flag
////////////////////////////////////////////////////////////////////////////////

// The flag is store_ranks, not store_distances: a "distance" assigned from
// whichever predecessor happens to bring w's in-degree to zero is an
// arbitrary choice among them. A rank has to clear *every* predecessor, so it
// accumulates with max -- these tests pin that down.
//
// Constructed by spelling the type out: topological_sort declares a deduction
// guide taking a Traits object but has no matching constructor, so the
// `topological_sort(traits{}, graph)` form does not compile today (same gap as
// depth_first_search).
namespace {
struct topological_sort_rank_traits : topological_sort_default_traits {
    static constexpr bool store_ranks = true;
};

template <typename A>
concept has_rank =
    requires(const A & a, const vertex_t<static_digraph> & u) { a.rank(u); };
}  // namespace

GTEST_TEST(topological_sort, store_rank_accessor_is_gated) {
    static_digraph_builder<static_digraph> builder(2);
    builder.add_arc(0, 1);
    auto [graph] = builder.build();

    topological_sort<graph_ref_view<static_digraph>,
                     topological_sort_rank_traits>
        alg(graph);
    static_assert(has_rank<decltype(alg)>);
    // control: the default traits withdraw it
    static_assert(!has_rank<decltype(topological_sort(graph))>);

    alg.run();
    ASSERT_EQ(alg.rank(0u), 0);
    ASSERT_EQ(alg.rank(1u), 1);
}

GTEST_TEST(topological_sort, rank_takes_the_longest_path) {
    // The diamond that separates the three candidate rules. Vertex 2 has
    // predecessors 0 (rank 0) and 1 (rank 1):
    //   max  -> 2, the only value with rank(1) < rank(2) across arc 1->2;
    //   min  -> 1, which ties with rank(1) and breaks the ordering;
    //   last predecessor removed -> whichever of the two Kahn happens to
    //                               process last, i.e. unspecified.
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc(0, 1).add_arc(1, 2).add_arc(0, 2);
    auto [graph] = builder.build();

    topological_sort<graph_ref_view<static_digraph>,
                     topological_sort_rank_traits>
        alg(graph);
    alg.run();

    ASSERT_EQ(alg.rank(0u), 0);
    ASSERT_EQ(alg.rank(1u), 1);
    ASSERT_EQ(alg.rank(2u), 2);
}

GTEST_TEST(topological_sort, rank_strictly_increases_along_every_arc) {
    // The defining property, checked exhaustively on a DAG with several
    // merges, a shortcut arc that skips a level, and two independent sources.
    static_digraph_builder<static_digraph> builder(9);
    builder.add_arc(0, 2)
        .add_arc(1, 2)
        .add_arc(0, 3)
        .add_arc(2, 4)
        .add_arc(3, 4)
        .add_arc(0, 4)  // shortcut: source straight to a deep vertex
        .add_arc(4, 5)
        .add_arc(2, 5)
        .add_arc(5, 6)
        .add_arc(1, 7)
        .add_arc(7, 6)
        .add_arc(6, 8);
    auto [graph] = builder.build();

    topological_sort<graph_ref_view<static_digraph>,
                     topological_sort_rank_traits>
        alg(graph);
    alg.run();

    for(const auto & a : arcs(graph)) {
        const auto u = arc_source(graph, a);
        const auto w = arc_target(graph, a);
        ASSERT_LT(alg.rank(u), alg.rank(w))
            << "arc " << u << " -> " << w << " does not increase the rank";
    }

    // sources sit at 0, and the rank is the longest path, not the shortest:
    // 4 is one arc from source 0 but three from 1 via 2
    ASSERT_EQ(alg.rank(0u), 0);
    ASSERT_EQ(alg.rank(1u), 0);
    ASSERT_EQ(alg.rank(4u), 2);
    ASSERT_EQ(alg.rank(8u), 5);
}

////////////////////////////////////////////////////////////////////////////////
// reached() tells the sorted vertices apart from those stuck on a cycle
////////////////////////////////////////////////////////////////////////////////

// Regression: _reached_map is filled with false and read by reached(); if
// nothing writes true to it, every accessor guarded by assert(reached(u)) --
// rank(), pred_vertex(), pred_arc() -- fires in a debug build, and only a
// test that switches one of the three flags on can notice.
GTEST_TEST(topological_sort, reached_marks_the_sorted_vertices) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(1, 2).add_arc(0, 3);
    auto [graph] = builder.build();

    topological_sort alg(graph);
    ASSERT_FALSE(alg.reached(2u));  // nothing sorted yet
    alg.run();
    for(const auto & v : vertices(graph)) ASSERT_TRUE(alg.reached(v));
}

GTEST_TEST(topological_sort, reached_is_false_on_a_cycle) {
    // 0 -> 1, then the cycle 2 -> 3 -> 2, which no in-degree ever drops to
    // zero: those vertices are never yielded, and reached() is how a caller
    // tells that apart from a complete sort.
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(2, 3).add_arc(3, 2);
    auto [graph] = builder.build();

    topological_sort alg(graph);
    alg.run();

    ASSERT_TRUE(alg.reached(0u));
    ASSERT_TRUE(alg.reached(1u));
    ASSERT_FALSE(alg.reached(2u));
    ASSERT_FALSE(alg.reached(3u));
}

////////////////////////////////////////////////////////////////////////////////
// the predecessor chain walks a critical (longest) path of length rank(w)
////////////////////////////////////////////////////////////////////////////////

// Not a traversal artefact, unlike DFS: Kahn's queue is FIFO, so it dequeues
// in non-decreasing rank order, and the predecessor that finally brings w's
// in-degree to zero is therefore one of maximum rank. Following pred_vertex
// back from w consequently walks a *longest* path from a source to w -- the
// critical path -- and its length is exactly rank(w).
namespace {
struct topological_sort_pred_traits : topological_sort_default_traits {
    static constexpr bool store_ranks = true;
    static constexpr bool store_critical_paths = true;
};
}  // namespace

GTEST_TEST(topological_sort, pred_chain_walks_a_longest_path) {
    static_digraph_builder<static_digraph> builder(9);
    builder.add_arc(0, 2)
        .add_arc(0, 3)
        .add_arc(0, 4)
        .add_arc(1, 2)
        .add_arc(1, 7)
        .add_arc(2, 4)
        .add_arc(2, 5)
        .add_arc(3, 4)
        .add_arc(4, 5)
        .add_arc(5, 6)
        .add_arc(6, 8)
        .add_arc(7, 6);
    auto [graph] = builder.build();

    topological_sort<graph_ref_view<static_digraph>,
                     topological_sort_pred_traits>
        alg(graph);
    alg.run();

    for(const auto & v : vertices(graph)) {
        int steps = 0;
        for(auto u = v; alg.rank(u) != 0; u = alg.pred_vertex(u)) {
            ASSERT_EQ(arc_target(graph, alg.pred_arc(u)), u);
            ASSERT_EQ(arc_source(graph, alg.pred_arc(u)), alg.pred_vertex(u));
            ASSERT_EQ(alg.rank(alg.pred_vertex(u)), alg.rank(u) - 1);
            ++steps;
        }
        ASSERT_EQ(steps, alg.rank(v)) << "vertex " << v;
    }

    std::vector<int> expected_ranks = {0, 0, 1, 1, 2, 3, 4, 1, 5};
    for(const auto & v : vertices(graph)) {
        ASSERT_EQ(alg.rank(v), expected_ranks[v]);
    }

    auto path = alg.critical_path_to(8u);
    static_assert(std::ranges::forward_range<decltype(path)>);
    static_assert(std::ranges::borrowed_range<decltype(path)>);
    static_assert(std::ranges::viewable_range<decltype(path)>);

    ASSERT_TRUE(EQ_MULTISETS(
        std::views::transform(
            path,
            [&](const auto & a) { return alg.rank(arc_target(graph, a)); }),
        {1, 2, 3, 4, 5}));
}

////////////////////////////////////////////////////////////////////////////////
// the critical-path accessors are gated on store_critical_paths
////////////////////////////////////////////////////////////////////////////////

namespace {
template <typename A>
concept has_pred_arc = requires(
    const A & a, const vertex_t<static_digraph> & u) { a.pred_arc(u); };
template <typename A>
concept has_pred_vertex = requires(
    const A & a, const vertex_t<static_digraph> & u) { a.pred_vertex(u); };
template <typename A>
concept has_critical_path_to = requires(
    const A & a, const vertex_t<static_digraph> & u) { a.critical_path_to(u); };
}  // namespace

GTEST_TEST(topological_sort, critical_path_accessors_are_gated) {
    static_digraph_builder<static_digraph> builder(2);
    builder.add_arc(0, 1);
    auto [graph] = builder.build();

    using with_paths = topological_sort<graph_ref_view<static_digraph>,
                                        topological_sort_pred_traits>;
    using without = decltype(topological_sort(graph));

    static_assert(has_pred_arc<with_paths>);
    static_assert(has_pred_vertex<with_paths>);
    static_assert(has_critical_path_to<with_paths>);
    // control: the default traits withdraw all three, so the negatives below
    // cannot be passing on a misspelled member name
    static_assert(!has_pred_arc<without>);
    static_assert(!has_pred_vertex<without>);
    static_assert(!has_critical_path_to<without>);
    // ranks are a separate flag and must not come along
    static_assert(!has_rank<without>);
}

GTEST_TEST(topological_sort, critical_path_to_a_source_is_empty) {
    // Sources carry an empty _pred_arcs_map entry, which is what stops the
    // path iterator; asking one for its critical path yields nothing rather
    // than walking off the start of the order.
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc(0, 2).add_arc(1, 2);
    auto [graph] = builder.build();

    topological_sort<graph_ref_view<static_digraph>,
                     topological_sort_pred_traits>
        alg(graph);
    alg.run();

    ASSERT_TRUE(EMPTY(alg.critical_path_to(0u)));
    ASSERT_TRUE(EMPTY(alg.critical_path_to(1u)));
    ASSERT_EQ(alg.rank(0u), 0);
    ASSERT_FALSE(EMPTY(alg.critical_path_to(2u)));
}

////////////////////////////////////////////////////////////////////////////////
// pred_vertex() also works on graphs that cannot map an arc to its source
////////////////////////////////////////////////////////////////////////////////

// When the graph cannot map an arc back to its source, pred_vertex reads the
// separate _pred_vertices_map instead of calling arc_source -- a branch no
// test reached, because every graph the suite uses here models has_arc_source.
// static_forward_digraph does not.
GTEST_TEST(topological_sort, critical_paths_without_arc_source) {
    static_assert(!has_arc_source<static_forward_digraph>);

    // 0 -> 2, 1 -> 2, 2 -> 3
    std::vector<unsigned int> sources = {0, 1, 2};
    std::vector<unsigned int> targets = {2, 2, 3};
    static_forward_digraph graph(4, sources, targets);

    topological_sort<graph_ref_view<static_forward_digraph>,
                     topological_sort_pred_traits>
        alg(graph);
    alg.run();

    ASSERT_EQ(alg.rank(3u), 2);
    ASSERT_EQ(alg.pred_vertex(3u), 2u);
    ASSERT_EQ(alg.pred_vertex(2u), 1u);

    int steps = 0;
    for(auto u = 3u; alg.rank(u) != 0; u = alg.pred_vertex(u)) {
        ASSERT_EQ(arc_target(graph, alg.pred_arc(u)), u);
        ++steps;
    }
    ASSERT_EQ(steps, alg.rank(3u));
}

////////////////////////////////////////////////////////////////////////////////
// reset() re-seeds the queue and rebuilds the optional state
////////////////////////////////////////////////////////////////////////////////

// Regression: the run decrements every remaining in-degree to zero, so
// clearing the queue and the reached map is not enough to restart: with
// nothing re-seeding the start vertices, reset() leaves an object that
// reports finished() immediately and yields nothing. reset() goes through
// push_start_vertices().
GTEST_TEST(topological_sort, reset_re_seeds_the_queue) {
    static_digraph_builder<static_digraph> builder(5);
    builder.add_arc(0, 1).add_arc(0, 2).add_arc(1, 3).add_arc(2, 3).add_arc(3,
                                                                            4);
    auto [graph] = builder.build();

    topological_sort<graph_ref_view<static_digraph>,
                     topological_sort_pred_traits>
        alg(graph);

    const auto collect = [&alg]() {
        std::vector<vertex_t<static_digraph>> order;
        for(auto && v : alg) order.push_back(v);
        return order;
    };

    const auto first = collect();
    ASSERT_EQ(first.size(), 5u);

    alg.reset();
    ASSERT_FALSE(alg.finished());

    const auto second = collect();
    ASSERT_EQ(second, first);

    // the optional state is rebuilt too, not merely the queue
    for(const auto & v : vertices(graph)) ASSERT_TRUE(alg.reached(v));
    ASSERT_EQ(alg.rank(0u), 0);
    ASSERT_EQ(alg.rank(3u), 2);
    ASSERT_EQ(alg.rank(4u), 3);
    int arcs_on_path = 0;
    for(auto && a : alg.critical_path_to(4u)) {
        (void)a;
        ++arcs_on_path;
    }
    ASSERT_EQ(arcs_on_path, alg.rank(4u));
}

////////////////////////////////////////////////////////////////////////////////
// regression: an algorithm lvalue must not be swallowed by the greedy
// single-argument constructor
////////////////////////////////////////////////////////////////////////////////

// An unconstrained `topological_sort(G &&)` beats the copy constructor for a
// non-const lvalue and tries to build the algorithm out of itself;
// detail::not_self is what excludes it. With copy deleted, the pin is that an
// algorithm is not constructible from an algorithm lvalue at all.
GTEST_TEST(topological_sort, is_not_constructible_from_an_algorithm) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(1, 2).add_arc(2, 3);
    auto [graph] = builder.build();

    topological_sort alg(graph);
    using alg_t = decltype(alg);
    static_assert(!std::is_constructible_v<alg_t, alg_t &>);
    static_assert(!std::is_constructible_v<alg_t, const alg_t &>);
    static_assert(std::is_constructible_v<alg_t, alg_t &&>);

    alg.advance();
    const auto expected = alg.current();
    auto relocated = std::move(alg);
    ASSERT_EQ(relocated.current(), expected);
    relocated.run();
    ASSERT_TRUE(relocated.finished());
}

////////////////////////////////////////////////////////////////////////////////
// is_acyclic() answers what the sweep already knows
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(topological_sort, is_acyclic_on_a_dag) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(1, 2).add_arc(0, 3).add_arc(3, 2);
    auto [graph] = builder.build();

    auto alg = topological_sort(graph);
    std::vector<vertex_t<static_digraph>> order;
    for(auto && v : alg) order.push_back(v);

    ASSERT_EQ(order.size(), 4u);
    ASSERT_TRUE(alg.is_acyclic());
}

GTEST_TEST(topological_sort, is_acyclic_on_a_graph_with_a_cycle) {
    static_digraph_builder<static_digraph> builder(4);
    // 0 -> 1, and the cycle 2 -> 3 -> 2 that no start vertex can reach into
    builder.add_arc(0, 1).add_arc(2, 3).add_arc(3, 2);
    auto [graph] = builder.build();

    auto alg = topological_sort(graph);
    alg.run();

    ASSERT_FALSE(alg.is_acyclic());
    // the cycle's vertices are exactly the ones left unordered
    ASSERT_TRUE(alg.reached(0u));
    ASSERT_TRUE(alg.reached(1u));
    ASSERT_FALSE(alg.reached(2u));
    ASSERT_FALSE(alg.reached(3u));

    // and it survives a reset
    alg.reset().run();
    ASSERT_FALSE(alg.is_acyclic());
}

// answering before the sweep is drained is a precondition violation: the count
// is only meaningful once no vertex can still be ordered
GTEST_TEST(topological_sort, is_acyclic_requires_a_drained_sweep) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc(0, 1).add_arc(1, 2);
    auto [graph] = builder.build();

    auto alg = topological_sort(graph);
    ASSERT_FALSE(alg.finished());
    EXPECT_DEATH((void)alg.is_acyclic(), "");
}
