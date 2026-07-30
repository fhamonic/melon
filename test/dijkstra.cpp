#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/dijkstra.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// dijkstra settles the vertices in order of increasing distance from the
// sources, one advance() at a time
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(dijkstra, test) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 7)
        .add_arc(0, 2, 9)
        .add_arc(0, 5, 14)
        .add_arc(1, 0, 7)
        .add_arc(1, 2, 10)
        .add_arc(1, 3, 15)
        .add_arc(2, 0, 9)
        .add_arc(2, 1, 10)
        .add_arc(2, 3, 12)
        .add_arc(2, 5, 2)
        .add_arc(3, 1, 15)
        .add_arc(3, 2, 12)
        .add_arc(3, 4, 6)
        .add_arc(4, 3, 6)
        .add_arc(4, 5, 9)
        .add_arc(5, 0, 14)
        .add_arc(5, 2, 2)
        .add_arc(5, 4, 9);

    auto [graph, length_map] = builder.build();

    dijkstra alg(graph, length_map);

    static_assert(std::copyable<decltype(alg)>);
    std::cout << "dijkstra size: " << sizeof(decltype(alg)) << std::endl;

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
    alg.reset();
}

////////////////////////////////////////////////////////////////////////////////
// the algorithm is an input range of (vertex, distance) pairs
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(dijkstra, algorithm_iterator) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 7)
        .add_arc(0, 2, 9)
        .add_arc(0, 5, 14)
        .add_arc(1, 0, 7)
        .add_arc(1, 2, 10)
        .add_arc(1, 3, 15)
        .add_arc(2, 0, 9)
        .add_arc(2, 1, 10)
        .add_arc(2, 3, 12)
        .add_arc(2, 5, 2)
        .add_arc(3, 1, 15)
        .add_arc(3, 2, 12)
        .add_arc(3, 4, 6)
        .add_arc(4, 3, 6)
        .add_arc(4, 5, 9)
        .add_arc(5, 0, 14)
        .add_arc(5, 2, 2)
        .add_arc(5, 4, 9);

    auto [graph, length_map] = builder.build();

    dijkstra alg(graph, length_map);
    alg.add_source(0u);

    static_assert(std::ranges::input_range<decltype(alg)>);
    static_assert(std::ranges::viewable_range<decltype(alg)>);

    std::vector traversal = {std::make_pair(0u, 0),  std::make_pair(1u, 7),
                             std::make_pair(2u, 9),  std::make_pair(5u, 11),
                             std::make_pair(4u, 20), std::make_pair(3u, 21)};

    std::size_t cpt = 0;
    for(const auto v : alg) {
        ASSERT_EQ(v, traversal[cpt]);
        ++cpt;
    }
    ASSERT_EQ(cpt, traversal.size());
}

////////////////////////////////////////////////////////////////////////////////
// store_paths: path_to() returns the arcs of a shortest path
////////////////////////////////////////////////////////////////////////////////

struct dijkstra_path_traits {
    using semiring = shortest_path_semiring<int>;
    using heap =
        updatable_d_ary_heap<2, std::pair<vertex_t<static_digraph>, int>,
                             semiring::less_t,
                             vertex_map_t<static_digraph, std::size_t>,
                             maps::element_map<1>, maps::element_map<0>>;

    static constexpr bool store_distances = false;
    static constexpr bool store_paths = true;
};

GTEST_TEST(dijkstra, path_to) {
    static_digraph_builder<static_digraph, int, int> builder(6);

    builder.add_arc(0, 1, 7, 1)
        .add_arc(0, 2, 9, 2)
        .add_arc(0, 5, 14, 3)
        .add_arc(1, 0, 7, 3)
        .add_arc(1, 2, 10, 4)
        .add_arc(1, 3, 15, 5)
        .add_arc(2, 0, 9, 6)
        .add_arc(2, 1, 10, 7)
        .add_arc(2, 3, 12, 8)
        .add_arc(2, 5, 2, 9)
        .add_arc(3, 1, 15, 10)
        .add_arc(3, 2, 12, 11)
        .add_arc(3, 4, 6, 12)
        .add_arc(4, 3, 6, 13)
        .add_arc(4, 5, 9, 14)
        .add_arc(5, 0, 14, 15)
        .add_arc(5, 2, 2, 16)
        .add_arc(5, 4, 9, 17);

    auto [graph, length_map, id] = builder.build();

    auto alg = dijkstra(dijkstra_path_traits{}, graph, length_map);
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
// regression: the advancing members allocate and run user code, so they must
// not be noexcept
////////////////////////////////////////////////////////////////////////////////

// advance(), add_source(), run() and reset() push into the heap -- which
// allocates -- and run the user's length map and semiring, so an unconditional
// noexcept turned any throw from those into std::terminate. The same applied
// to algorithm_iterator, which forwards straight into advance().
namespace {
using probe_dijkstra =
    dijkstra<graph_ref_view<static_digraph>,
             mapping_ref_view<const static_map<unsigned int, int>>,
             dijkstra_default_traits<static_digraph, int>>;
using probe_iterator = decltype(std::declval<probe_dijkstra &>().begin());
}  // namespace

static_assert(!noexcept(std::declval<probe_dijkstra &>().advance()));
static_assert(!noexcept(std::declval<probe_dijkstra &>().run()));
static_assert(!noexcept(std::declval<probe_dijkstra &>().reset()));
static_assert(!noexcept(std::declval<probe_dijkstra &>().add_source(
    std::declval<const unsigned int &>())));
static_assert(!noexcept(++std::declval<probe_iterator &>()));
static_assert(noexcept(std::declval<const probe_dijkstra &>().finished()));

////////////////////////////////////////////////////////////////////////////////
// regression: current_dist() reads the heap, so it must not require
// store_distances

// current_dist reads the heap, not _distances_map, so requiring
// store_distances kept it out of reach of every default-configured dijkstra.
namespace {
struct traits_without_distances : dijkstra_default_traits<static_digraph, int> {
    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};
template <typename D>
concept has_current_dist =
    requires(const D & d, const unsigned int & u) { d.current_dist(u); };
}  // namespace

static_assert(dijkstra_traits<traits_without_distances>);

GTEST_TEST(dijkstra, current_dist_without_store_distances) {
    static_digraph_builder<static_digraph, int> builder(3);
    builder.add_arc(0u, 1u, 4);
    builder.add_arc(1u, 2u, 6);
    auto [graph, length_map] = builder.build();

    dijkstra algo(traits_without_distances{}, graph, length_map, 0u);
    // the whole point: available even though store_distances is false
    static_assert(!traits_without_distances::store_distances);
    static_assert(has_current_dist<decltype(algo)>);
    algo.advance();  // settles 0, puts 1 in the heap at distance 4
    ASSERT_TRUE(algo.reached(1u));
    ASSERT_FALSE(algo.visited(1u));
    ASSERT_EQ(algo.current_dist(1u), 4);
}

////////////////////////////////////////////////////////////////////////////////
// store_distances: dist() is gated on its flag, keeps its own map, and
// survives reset() and a rerun
////////////////////////////////////////////////////////////////////////////////

// Every dijkstra test above runs with store_distances = false -- the default,
// and the only value the suite ever exercised -- so _distances_map, the
// vertex_map_if that allocates it, the write in advance() and the dist()
// accessor gated on it were all dead code as far as the tests were concerned.
namespace {
struct dijkstra_traits_distances
    : dijkstra_default_traits<static_digraph, int> {
    static constexpr bool store_distances = true;
    static constexpr bool store_paths = false;
};
struct dijkstra_traits_distances_and_paths
    : dijkstra_default_traits<static_digraph, int> {
    static constexpr bool store_distances = true;
    static constexpr bool store_paths = true;
};

template <typename A>
concept has_dist =
    requires(const A & a, const vertex_t<static_digraph> & u) { a.dist(u); };
template <typename A>
concept has_path_to =
    requires(const A & a, const vertex_t<static_digraph> & u) { a.path_to(u); };
}  // namespace

static_assert(dijkstra_traits<dijkstra_traits_distances>);
static_assert(dijkstra_traits<dijkstra_traits_distances_and_paths>);

namespace {
auto build_dijkstra_test_graph() {
    static_digraph_builder<static_digraph, int, int> builder(6);
    builder.add_arc(0, 1, 7, 1)
        .add_arc(0, 2, 9, 2)
        .add_arc(0, 5, 14, 3)
        .add_arc(1, 0, 7, 3)
        .add_arc(1, 2, 10, 4)
        .add_arc(1, 3, 15, 5)
        .add_arc(2, 0, 9, 6)
        .add_arc(2, 1, 10, 7)
        .add_arc(2, 3, 12, 8)
        .add_arc(2, 5, 2, 9)
        .add_arc(3, 1, 15, 10)
        .add_arc(3, 2, 12, 11)
        .add_arc(3, 4, 6, 12)
        .add_arc(4, 3, 6, 13)
        .add_arc(4, 5, 9, 14)
        .add_arc(5, 0, 14, 15)
        .add_arc(5, 2, 2, 16)
        .add_arc(5, 4, 9, 17);
    return builder.build();
}
}  // namespace

GTEST_TEST(dijkstra, store_distances_true) {
    auto [graph, length_map, id] = build_dijkstra_test_graph();

    auto alg = dijkstra(dijkstra_traits_distances{}, graph, length_map);
    alg.add_source(0u).run();

    using with_dist = decltype(alg);
    static_assert(has_dist<with_dist>);
    // paths stay off, so their accessor must not come along for the ride
    static_assert(!has_path_to<with_dist>);

    // dist() reads _distances_map, written once per vertex as it is settled;
    // same values the default-traits test observes through current()
    ASSERT_EQ(alg.dist(0u), 0);
    ASSERT_EQ(alg.dist(1u), 7);
    ASSERT_EQ(alg.dist(2u), 9);
    ASSERT_EQ(alg.dist(5u), 11);
    ASSERT_EQ(alg.dist(4u), 20);
    ASSERT_EQ(alg.dist(3u), 21);

    // and it survives a reset + rerun from a different source
    alg.reset();
    alg.add_source(3u).run();
    ASSERT_EQ(alg.dist(3u), 0);
    ASSERT_EQ(alg.dist(4u), 6);
    ASSERT_EQ(alg.dist(2u), 12);
}

GTEST_TEST(dijkstra, store_distances_and_paths_together) {
    auto [graph, length_map, id] = build_dijkstra_test_graph();

    auto alg =
        dijkstra(dijkstra_traits_distances_and_paths{}, graph, length_map);
    alg.add_source(0u).run();

    using both = decltype(alg);
    static_assert(has_dist<both>);
    static_assert(has_path_to<both>);

    // the two flags allocate separate maps; enabling both must not make
    // either one read the other's storage
    ASSERT_EQ(alg.dist(3u), 21);
    ASSERT_TRUE(EQ_MULTISETS(
        std::views::transform(alg.path_to(3u),
                              [&id](const auto & a) { return id[a]; }),
        {2, 8}));

    ASSERT_EQ(alg.dist(4u), 20);
    ASSERT_TRUE(EQ_MULTISETS(
        std::views::transform(alg.path_to(4u),
                              [&id](const auto & a) { return id[a]; }),
        {2, 9, 17}));
}

////////////////////////////////////////////////////////////////////////////////
// regression: pred_arc() on a source is a precondition, not a silent
// std::terminate
////////////////////////////////////////////////////////////////////////////////

// add_source() resets the source's optional predecessor arc, and pred_arc()
// returned it with .value() from a noexcept function -- so asking a source for
// its predecessor threw std::bad_optional_access straight into std::terminate,
// with no diagnostic, in release builds too. It is a precondition violation,
// now asserted like the one pred_vertex() already made.
GTEST_TEST(dijkstra, pred_arc_on_a_source_is_a_precondition) {
    static_digraph_builder<static_digraph> plain(3);
    plain.add_arc(0, 1).add_arc(1, 2);
    auto [graph] = plain.build();
    auto length_map = create_arc_map<int>(graph, 1);

    auto alg = dijkstra(dijkstra_path_traits{}, graph, length_map);
    alg.add_source(0u).run();

    // reachable, non-source vertices are unaffected
    ASSERT_EQ(arc_target(graph, alg.pred_arc(1u)), 1u);
    ASSERT_EQ(arc_target(graph, alg.pred_arc(2u)), 2u);

    // the source is reached, so reached() alone never caught this
    ASSERT_TRUE(alg.reached(0u));
    EXPECT_DEATH((void)alg.pred_arc(0u), "");
}

////////////////////////////////////////////////////////////////////////////////
// regression: 2.3, the Traits default lives on the class, so the written-out
// type can be named and matches CTAD
////////////////////////////////////////////////////////////////////////////////

// The default lived on the deduction guides only, so CTAD worked while the
// written-out `dijkstra<G, LM>` did not compile -- "wrong number of template
// arguments (2, should be 3)" -- and there was no way to name the type CTAD
// had just produced. The default is on the class now, and the guides no longer
// carry one, so there is a single definition of it and the two spellings name
// the same type.
namespace traits_default {
using G = views::graph_all_t<static_digraph &>;
using LM = maps::mapping_all_t<static_map<arc_t<static_digraph>, int> &>;

// the two-argument spelling compiles at all -- this is the item
using written_out = dijkstra<G, LM>;

template <typename... Ts>
using deduced = decltype(dijkstra(std::declval<Ts>()...));
}  // namespace traits_default

// and the default it picks is the library's own, computed over the *view*
// type the class is instantiated with
static_assert(
    std::same_as<traits_default::written_out,
                 dijkstra<traits_default::G, traits_default::LM,
                          dijkstra_default_traits<traits_default::G, int>>>);

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

// an explicit Traits still wins over the default, spelled either way
static_assert(
    std::same_as<
        traits_default::deduced<dijkstra_traits_distances, static_digraph &,
                                static_map<arc_t<static_digraph>, int> &>,
        dijkstra<traits_default::G, traits_default::LM,
                 dijkstra_traits_distances>>);
static_assert(!std::same_as<dijkstra<traits_default::G, traits_default::LM,
                                     dijkstra_traits_distances>,
                            traits_default::written_out>);

GTEST_TEST(dijkstra, default_traits_spelling_runs) {
    static_digraph_builder<static_digraph, int> builder(4);
    builder.add_arc(0, 1, 2).add_arc(1, 2, 3).add_arc(0, 2, 9);
    auto [graph, length_map] = builder.build();

    // the point of the item: a variable of the written-out type, seeded and run
    dijkstra<views::graph_all_t<decltype(graph) &>,
             maps::mapping_all_t<decltype(length_map) &>>
        alg(graph, length_map);
    alg.add_source(0u);

    // default traits store neither distances nor paths, so the range is the
    // whole interface: (vertex, distance) in order of increasing distance
    ASSERT_TRUE(
        EQ_RANGES(alg, std::vector<std::pair<vertex_t<static_digraph>, int>>{
                           {0u, 0}, {1u, 2}, {2u, 5}}));
}
