#undef NDEBUG
#include <gtest/gtest.h>

#include <vector>

#include "melon/algorithm/a_star.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

using namespace melon;

namespace {

// The same graph as dijkstra's tests: every arc has its reverse with equal
// length, so exact distances to a target are readable off a dijkstra run from
// it -- which is what perfect_heuristic below hardcodes.
auto small_instance() {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc({0, 1}, 7)
        .add_arc({0, 2}, 9)
        .add_arc({0, 5}, 14)
        .add_arc({1, 0}, 7)
        .add_arc({1, 2}, 10)
        .add_arc({1, 3}, 15)
        .add_arc({2, 0}, 9)
        .add_arc({2, 1}, 10)
        .add_arc({2, 3}, 12)
        .add_arc({2, 5}, 2)
        .add_arc({3, 1}, 15)
        .add_arc({3, 2}, 12)
        .add_arc({3, 4}, 6)
        .add_arc({4, 3}, 6)
        .add_arc({4, 5}, 9)
        .add_arc({5, 0}, 14)
        .add_arc({5, 2}, 2)
        .add_arc({5, 4}, 9);

    return builder.build();
}

// Exact distances to vertex 3.
const std::vector<int> perfect_heuristic = {21, 15, 12, 0, 6, 14};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// with a zero heuristic the traversal is dijkstra's, one advance() at a time
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(a_star, zero_heuristic_settles_in_distance_order) {
    auto [graph, length_map] = small_instance();

    a_star alg(graph, length_map, [](unsigned int) { return 0; });

    static_assert(std::movable<decltype(alg)> && !std::copyable<decltype(alg)>);

    alg.add_source(0);
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(0u, 0));
    alg.advance();
    ASSERT_EQ(alg.current(), std::make_pair(1u, 7));
    alg.advance();
    ASSERT_EQ(alg.current(), std::make_pair(2u, 9));
    alg.advance();
    ASSERT_EQ(alg.current(), std::make_pair(5u, 11));
    alg.advance();
    ASSERT_EQ(alg.current(), std::make_pair(4u, 20));
    alg.advance();
    ASSERT_EQ(alg.current(), std::make_pair(3u, 21));
    alg.advance();
    ASSERT_TRUE(alg.finished());
    alg.reset();
}

////////////////////////////////////////////////////////////////////////////////
// a perfect heuristic settles only the shortest path's vertices, in key order,
// while current() and dist() report plain distances
////////////////////////////////////////////////////////////////////////////////

namespace {
struct a_star_path_traits
    : a_star_default_traits<views::graph_all_t<static_digraph &>, int> {
    static constexpr bool store_distances = true;
    static constexpr bool store_paths = true;
};
}  // namespace

GTEST_TEST(a_star, perfect_heuristic_prunes_to_the_shortest_path) {
    auto [graph, length_map] = small_instance();

    auto alg =
        a_star(a_star_path_traits{}, graph, length_map, perfect_heuristic, 0u);

    std::vector<std::pair<unsigned int, int>> settled;
    while(!alg.finished()) {
        settled.push_back(alg.current());
        alg.advance();
        if(settled.back().first == 3u) break;
    }

    const std::vector<std::pair<unsigned int, int>> expected = {
        {0u, 0}, {2u, 9}, {3u, 21}};
    ASSERT_EQ(settled, expected);

    ASSERT_TRUE(alg.visited(3u));
    ASSERT_FALSE(alg.visited(1u));
    ASSERT_FALSE(alg.visited(4u));
    ASSERT_FALSE(alg.visited(5u));

    ASSERT_EQ(alg.dist(3u), 21);
    int walked = 0;
    std::size_t arcs_count = 0;
    for(auto && a : alg.path_to(3u)) {
        walked += length_map[a];
        ++arcs_count;
    }
    ASSERT_EQ(walked, 21);
    ASSERT_EQ(arcs_count, 2u);
}

////////////////////////////////////////////////////////////////////////////////
// heuristic consistency is asserted at the offending arc, including one whose
// target is already settled -- the shape that silently corrupts distances,
// since the traversal never reopens a settled vertex
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(a_star, heuristic_inconsistency_is_asserted_at_the_offending_arc) {
    static_digraph_builder<static_digraph, int> builder(3);
    builder.add_arc({0, 1}, 1).add_arc({0, 2}, 10).add_arc({1, 2}, 1);
    auto [graph, length_map] = builder.build();

    // Inconsistent on arc 1->2 alone: h(1) = 100 > length + h(2) = 1.
    // Vertex 2 settles at distance 10 through 0->2 before 1 pops at key 101,
    // so the violating arc lands on an already-settled target; without the
    // assert the run would finish silently with dist(2) = 10 where dijkstra
    // gives 2.
    const std::vector<int> inconsistent_heuristic = {0, 100, 0};

    a_star alg(graph, length_map, inconsistent_heuristic, 0u);
    EXPECT_DEATH(
        {
            while(!alg.finished()) alg.advance();
        },
        "");
}

////////////////////////////////////////////////////////////////////////////////
// deliberately no defaulted zero heuristic: that instance is dijkstra, so the
// two-map spelling must not deduce
////////////////////////////////////////////////////////////////////////////////

namespace ctad {
template <typename... Args>
concept spellable = requires(Args &... args) { a_star(args...); };
}  // namespace ctad

static_assert(
    ctad::spellable<static_digraph, std::vector<int>, std::vector<int>>);
static_assert(!ctad::spellable<static_digraph, std::vector<int>>);
