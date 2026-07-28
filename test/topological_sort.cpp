#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/topological_sort.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

GTEST_TEST(topological_sort, no_arcs_graph) {
    static_digraph_builder<static_digraph> builder(2);

    auto [graph] = builder.build();

    topological_sort alg(graph);

    static_assert(std::copyable<decltype(alg)>);

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
// ########################### traits: store_rank #############################

// The flag used to be called store_distances and assigned _dist_map[w] from
// whichever predecessor happened to bring w's in-degree to zero, which is an
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

// ################# regression: reached() was always false ###################

// _reached_map was filled with false and read by reached(), but nothing ever
// wrote true to it. Every accessor guarded by assert(reached(u)) -- rank(),
// pred_vertex(), pred_arc() -- therefore fired in a debug build, which no
// test noticed because none of the three flags was ever switched on.
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

// ################## what the predecessor maps actually mean ##################

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

    std::vector<std::size_t> expected_ranks = {0, 0, 1, 1, 2, 3, 4, 1, 5};
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
