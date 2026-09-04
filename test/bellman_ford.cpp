#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/bellman_ford.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "arc_list_digraph.hpp"
#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// bellman_ford computes shortest distances from the sources by repeated
// relaxation passes over all arcs; run() drains it in one call, and unlike
// dijkstra it accepts negative lengths
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(bellman_ford, test) {
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

    auto [graph, length_map] = builder.build();

    bellman_ford alg(graph, length_map);

    static_assert(std::movable<decltype(alg)> && !std::copyable<decltype(alg)>);
    std::cout << "bellman_ford size: " << sizeof(decltype(alg)) << std::endl;

    alg.add_source(0).run();
    ASSERT_EQ(alg.dist(0u), 0);
    ASSERT_EQ(alg.dist(1u), 7);
    ASSERT_EQ(alg.dist(2u), 9);
    ASSERT_EQ(alg.dist(5u), 11);
    ASSERT_EQ(alg.dist(4u), 20);
    ASSERT_EQ(alg.dist(3u), 21);
    alg.reset();
}

////////////////////////////////////////////////////////////////////////////////
// store_paths: path_to() returns the arcs of a shortest path
////////////////////////////////////////////////////////////////////////////////

struct bellman_ford_path_traits {
    using semiring = shortest_path_semiring<int>;
    static constexpr bool store_paths = true;
    static constexpr bool detect_negative_cycles = false;
};

struct bellman_ford_detect_traits {
    using semiring = shortest_path_semiring<int>;
    static constexpr bool store_paths = false;
    static constexpr bool detect_negative_cycles = true;
};

struct bellman_ford_full_traits {
    using semiring = shortest_path_semiring<int>;
    static constexpr bool store_paths = true;
    static constexpr bool detect_negative_cycles = true;
};

GTEST_TEST(bellman_ford, path_to) {
    static_digraph_builder<static_digraph, int, int> builder(6);

    builder.add_arc({0, 1}, 7, 1)
        .add_arc({0, 2}, 9, 2)
        .add_arc({0, 5}, 14, 3)
        .add_arc({1, 0}, 7, 3)
        .add_arc({1, 2}, 10, 4)
        .add_arc({1, 3}, 15, 5)
        .add_arc({2, 0}, 9, 6)
        .add_arc({2, 1}, 10, 7)
        .add_arc({2, 3}, 12, 8)
        .add_arc({2, 5}, 2, 9)
        .add_arc({3, 1}, 15, 10)
        .add_arc({3, 2}, 12, 11)
        .add_arc({3, 4}, 6, 12)
        .add_arc({4, 3}, 6, 13)
        .add_arc({4, 5}, 9, 14)
        .add_arc({5, 0}, 14, 15)
        .add_arc({5, 2}, 2, 16)
        .add_arc({5, 4}, 9, 17);

    auto [graph, length_map, id] = builder.build();

    auto alg = bellman_ford(bellman_ford_path_traits{}, graph, length_map);
    alg.add_source(0u).run();

    auto path = alg.path_to(3u);
    static_assert(std::ranges::forward_range<decltype(path)>);
    static_assert(std::ranges::borrowed_range<decltype(path)>);
    static_assert(std::ranges::viewable_range<decltype(path)>);

    ASSERT_TRUE(EQ_MULTISETS(
        std::views::transform(path, [&id](const auto & a) { return id[a]; }),
        {2, 8}));
}

////////////////////////////////////////////////////////////////////////////////
// negative lengths are the point: the shortest path may enter a vertex
// through an arc that a greedy settling order would have frozen too early
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(bellman_ford, negative_lengths) {
    static_digraph_builder<static_digraph, int, int> builder(4);
    // 0 -> 1 direct costs 5, but 0 -> 2 -> 1 costs 2 + (-4) = -2: a greedy
    // label-setting order settles 1 at 5 before the negative arc is seen
    builder.add_arc({0, 1}, 5, 0)
        .add_arc({0, 2}, 2, 1)
        .add_arc({1, 3}, 1, 3)
        .add_arc({2, 1}, -4, 2);
    auto [graph, length_map, id] = builder.build();

    auto alg = bellman_ford(bellman_ford_path_traits{}, graph, length_map);
    alg.add_source(0u).run();

    ASSERT_EQ(alg.dist(0u), 0);
    ASSERT_EQ(alg.dist(2u), 2);
    ASSERT_EQ(alg.dist(1u), -2);
    ASSERT_EQ(alg.dist(3u), -1);

    ASSERT_TRUE(EQ_MULTISETS(
        std::views::transform(alg.path_to(3u),
                              [&id](const auto & a) { return id[a]; }),
        {1, 2, 3}));
}

////////////////////////////////////////////////////////////////////////////////
// with detect_negative_cycles opted in, a negative cycle reachable from the
// sources is reported, not folded into wrong distances; run() stays
// idempotent once it is found, and reset() clears it
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(bellman_ford, negative_cycle) {
    static_digraph_builder<static_digraph, int> builder(5);
    // 1 -> 2 -> 1 sums to -2 and hangs off 0; the component {3, 4} is clean
    builder.add_arc({0, 1}, 1)
        .add_arc({1, 2}, -3)
        .add_arc({2, 1}, 1)
        .add_arc({3, 4}, 1);
    auto [graph, length_map] = builder.build();

    auto alg = bellman_ford(bellman_ford_detect_traits{}, graph, length_map);
    alg.add_source(0u).run();
    ASSERT_TRUE(alg.found_negative_cycle());

    // run() is a no-op once the cycle is found: the distances, meaningless as
    // they are, must at least stop moving
    const int snapshot = alg.dist(1u);
    alg.run();
    ASSERT_TRUE(alg.found_negative_cycle());
    ASSERT_EQ(alg.dist(1u), snapshot);

    // reset() restores the constructor's state: seeded from the clean
    // component, the cycle is unreachable and the flag stays down
    alg.reset().add_source(3u).run();
    ASSERT_FALSE(alg.found_negative_cycle());
    ASSERT_EQ(alg.dist(3u), 0);
    ASSERT_EQ(alg.dist(4u), 1);
    ASSERT_FALSE(alg.reached(1u));
}

////////////////////////////////////////////////////////////////////////////////
// with store_paths as well, the detected cycle is retrievable as its arcs in
// path order
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(bellman_ford, negative_cycle_retrieval) {
    static_digraph_builder<static_digraph, int, int> builder(4);
    // the cycle 1 -> 2 -> 1 sums to -2; vertex 3 hangs downstream of it, so
    // the witness the certifying pass records may sit off the cycle and the
    // pred walk has to march back onto it
    builder.add_arc({0, 1}, 1, 0)
        .add_arc({1, 2}, -3, 1)
        .add_arc({2, 1}, 1, 2)
        .add_arc({2, 3}, 1, 3);
    auto [graph, length_map, id] = builder.build();

    auto alg = bellman_ford(bellman_ford_full_traits{}, graph, length_map);
    alg.add_source(0u).run();
    ASSERT_TRUE(alg.found_negative_cycle());

    const auto cycle = alg.negative_cycle();
    ASSERT_TRUE(EQ_MULTISETS(
        std::views::transform(cycle, [&id](const auto & a) { return id[a]; }),
        {1, 2}));
    for(std::size_t i = 0; i < cycle.size(); ++i)
        ASSERT_EQ(arc_target(graph, cycle[i]),
                  arc_source(graph, cycle[(i + 1) % cycle.size()]));
}

GTEST_TEST(bellman_ford, negative_self_loop_is_a_one_arc_cycle) {
    static_digraph_builder<static_digraph, int> builder(2);
    builder.add_arc({0, 1}, 1).add_arc({1, 1}, -1);
    auto [graph, length_map] = builder.build();

    auto alg = bellman_ford(bellman_ford_full_traits{}, graph, length_map);
    alg.add_source(0u).run();
    ASSERT_TRUE(alg.found_negative_cycle());

    const auto cycle = alg.negative_cycle();
    ASSERT_EQ(cycle.size(), 1u);
    ASSERT_EQ(arc_source(graph, cycle.front()), 1u);
    ASSERT_EQ(arc_target(graph, cycle.front()), 1u);
}

////////////////////////////////////////////////////////////////////////////////
// the certifying pass is not fooled by a graph that legitimately needs all
// n-1 computing passes: still updating on the last of them is not a cycle
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(bellman_ford, needing_every_pass_is_not_a_cycle) {
    // chain 3 -> 2 -> 1 -> 0: static_digraph's arcs_entries is sorted by
    // source, so each pass settles exactly one more vertex and pass n-1
    // still updates -- only the certifying pass observes quiescence
    static_digraph_builder<static_digraph, int> builder(4);
    builder.add_arc({1, 0}, 1).add_arc({2, 1}, 1).add_arc({3, 2}, 1);
    auto [graph, length_map] = builder.build();

    auto alg = bellman_ford(bellman_ford_detect_traits{}, graph, length_map);
    alg.add_source(3u).run();

    ASSERT_FALSE(alg.found_negative_cycle());
    ASSERT_EQ(alg.dist(0u), 3);
}

////////////////////////////////////////////////////////////////////////////////
// detection and retrieval are opt-in: without their traits flags the
// accessors leave the overload set and the state leaves the object
////////////////////////////////////////////////////////////////////////////////

namespace opt_in {
using G = views::graph_all_t<static_digraph &>;
using LM = maps::mapping_all_t<static_map<arc_t<static_digraph>, int> &>;

template <typename A>
concept reports_negative_cycles =
    requires(const A & a) { a.found_negative_cycle(); };
template <typename A>
concept retrieves_negative_cycles =
    requires(const A & a) { a.negative_cycle(); };
}  // namespace opt_in

static_assert(
    !opt_in::reports_negative_cycles<bellman_ford<opt_in::G, opt_in::LM>>);
static_assert(opt_in::reports_negative_cycles<
              bellman_ford<opt_in::G, opt_in::LM, bellman_ford_detect_traits>>);
// negative_cycle() needs both flags: detection to certify one exists, the
// pred maps to walk it
static_assert(!opt_in::retrieves_negative_cycles<
              bellman_ford<opt_in::G, opt_in::LM, bellman_ford_detect_traits>>);
static_assert(!opt_in::retrieves_negative_cycles<
              bellman_ford<opt_in::G, opt_in::LM, bellman_ford_path_traits>>);
static_assert(opt_in::retrieves_negative_cycles<
              bellman_ford<opt_in::G, opt_in::LM, bellman_ford_full_traits>>);

////////////////////////////////////////////////////////////////////////////////
// regression: arcs out of unreached vertices are skipped, not relaxed --
// relaxing one is infty + length, signed overflow for int, and the wrapped
// value beats every genuine distance
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(bellman_ford, unreached_vertices_stay_at_infty) {
    static_digraph_builder<static_digraph, int> builder(3);
    // vertex 1 is unreachable from the source; its out-arc must stay inert
    builder.add_arc({1, 2}, 1);
    auto [graph, length_map] = builder.build();

    bellman_ford alg(graph, length_map);
    alg.add_source(0u).run();

    ASSERT_TRUE(alg.reached(0u));
    ASSERT_FALSE(alg.reached(1u));
    ASSERT_FALSE(alg.reached(2u));
    ASSERT_EQ(alg.dist(2u), std::numeric_limits<int>::max());
}

////////////////////////////////////////////////////////////////////////////////
// relaxation reads only arcs_entries, which the base graph concept already
// synthesizes -- so the class head requires no incidence, and an
// arc-list-only structure qualifies
////////////////////////////////////////////////////////////////////////////////

static_assert(melon::graph<arc_list_digraph>);
static_assert(melon::has_vertex_map<arc_list_digraph>);
static_assert(!melon::outward_incidence_graph<arc_list_digraph>);

GTEST_TEST(bellman_ford, runs_on_an_arc_list_only_graph) {
    arc_list_digraph graph{3u, {{0u, 1u}, {1u, 2u}}};
    auto length_map = static_map<unsigned int, int>(2u, 5);

    bellman_ford alg(graph, length_map);
    alg.add_source(0u).run();
    ASSERT_EQ(alg.dist(2u), 10);
}

////////////////////////////////////////////////////////////////////////////////
// regression: the advancing members run user code, so they must not be
// noexcept
////////////////////////////////////////////////////////////////////////////////

// run(), add_source() and reset() execute the user's length map and semiring;
// an unconditional noexcept would turn any throw from those into
// std::terminate.
namespace {
using probe_bellman_ford = bellman_ford<
    graph_ref_view<static_digraph>,
    mapping_ref_view<const static_map<unsigned int, int>>,
    bellman_ford_default_traits<graph_ref_view<static_digraph>, int>>;
}  // namespace

static_assert(!noexcept(std::declval<probe_bellman_ford &>().run()));
static_assert(!noexcept(std::declval<probe_bellman_ford &>().reset()));
static_assert(!noexcept(std::declval<probe_bellman_ford &>().add_source(
    std::declval<const unsigned int &>())));

////////////////////////////////////////////////////////////////////////////////
// pred_arc() on a source and path_to() on an unreached vertex are
// precondition violations, asserted -- not empty answers
////////////////////////////////////////////////////////////////////////////////

// A source has no predecessor arc, and an unreached vertex has no path; both
// spellings would otherwise yield the same empty-looking state a source
// legitimately has, so a caller could not tell "no path exists" from "t is at
// distance zero".
GTEST_TEST(bellman_ford, pred_arc_on_a_source_is_a_precondition) {
    static_digraph_builder<static_digraph> plain(3);
    plain.add_arc({0, 1}).add_arc({1, 2});
    auto [graph] = plain.build();
    auto length_map = create_arc_map<int>(graph, 1);

    auto alg = bellman_ford(bellman_ford_path_traits{}, graph, length_map);
    alg.add_source(0u).run();

    // reachable, non-source vertices are unaffected
    ASSERT_EQ(arc_target(graph, alg.pred_arc(1u)), 1u);
    ASSERT_EQ(arc_target(graph, alg.pred_arc(2u)), 2u);

    EXPECT_DEATH((void)alg.pred_arc(0u), "");
}

GTEST_TEST(bellman_ford, path_to_an_unreached_vertex_is_a_precondition) {
    static_digraph_builder<static_digraph> plain(3);
    plain.add_arc({0, 1});
    auto [graph] = plain.build();
    auto length_map = create_arc_map<int>(graph, 1);

    auto alg = bellman_ford(bellman_ford_path_traits{}, graph, length_map);
    alg.add_source(0u).run();

    ASSERT_FALSE(alg.reached(2u));
    EXPECT_DEATH((void)alg.path_to(2u), "");
}

////////////////////////////////////////////////////////////////////////////////
// the Traits default lives on the class, so the written-out type can be named
// and matches CTAD
////////////////////////////////////////////////////////////////////////////////

// With the default on the deduction guides only, CTAD would work while the
// written-out `bellman_ford<G, LM>` did not compile, and there would be no
// way to name the type CTAD had just produced. The default is on the class
// and the guides carry none, so the two spellings name the same type.
namespace traits_default {
using G = views::graph_all_t<static_digraph &>;
using LM = maps::mapping_all_t<static_map<arc_t<static_digraph>, int> &>;

// this alias compiling at all is the first assertion
using written_out = bellman_ford<G, LM>;

template <typename... Ts>
using deduced = decltype(bellman_ford(std::declval<Ts>()...));
}  // namespace traits_default

// and the default it picks is the library's own, computed over the *view*
// type the class is instantiated with
static_assert(
    std::same_as<
        traits_default::written_out,
        bellman_ford<traits_default::G, traits_default::LM,
                     bellman_ford_default_traits<traits_default::G, int>>>);

// CTAD lands on exactly the type the two-argument spelling names, from every
// constructor shape
static_assert(std::same_as<
              traits_default::deduced<static_digraph &,
                                      static_map<arc_t<static_digraph>, int> &>,
              traits_default::written_out>);
static_assert(
    std::same_as<traits_default::deduced<
                     static_digraph &, static_map<arc_t<static_digraph>, int> &,
                     const vertex_t<static_digraph> &>,
                 traits_default::written_out>);

GTEST_TEST(bellman_ford, default_traits_spelling_runs) {
    static_digraph_builder<static_digraph, int> builder(4);
    builder.add_arc({0, 1}, 2).add_arc({1, 2}, 3).add_arc({0, 2}, 9);
    auto [graph, length_map] = builder.build();

    // spelled out rather than CTAD on purpose: a deduced variable would pass
    // even with the default moved onto the guides
    bellman_ford<views::graph_all_t<decltype(graph) &>,
                 maps::mapping_all_t<decltype(length_map) &>>
        alg(graph, length_map);
    alg.add_source(0u).run();

    ASSERT_EQ(alg.dist(0u), 0);
    ASSERT_EQ(alg.dist(1u), 2);
    ASSERT_EQ(alg.dist(2u), 5);
}
