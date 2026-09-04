#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/network_voronoi.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// network_voronoi assigns each vertex to its closest kernel, enumerated in
// increasing distance order
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(network_voronoi, test) {
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

    std::vector<vertex_t<static_digraph>> kernels = {0u, 3u, 5u};

    network_voronoi alg(graph, length_map, kernels);

    static_assert(std::movable<decltype(alg)> && !std::copyable<decltype(alg)>);

    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(0u, std::make_pair(0, 0u)));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(3u, std::make_pair(0, 3u)));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(5u, std::make_pair(0, 5u)));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(2u, std::make_pair(2, 5u)));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(4u, std::make_pair(6, 3u)));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(1u, std::make_pair(7, 0u)));
    alg.advance();
    ASSERT_TRUE(alg.finished());
    alg.reset();
}

// iterating the algorithm as a range gives the same sequence
GTEST_TEST(network_voronoi, test_for) {
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

    std::vector<vertex_t<static_digraph>> kernels = {0u, 3u, 5u};

    std::vector<std::pair<vertex_t<static_digraph>,
                          std::pair<int, vertex_t<static_digraph>>>>
        expected_output = {
            {0u, std::make_pair(0, 0u)}, {3u, std::make_pair(0, 3u)},
            {5u, std::make_pair(0, 5u)}, {2u, std::make_pair(2, 5u)},
            {4u, std::make_pair(6, 3u)}, {1u, std::make_pair(7, 0u)}};

    ASSERT_TRUE(EQ_RANGES(network_voronoi(graph, length_map, kernels),
                          expected_output));
}

////////////////////////////////////////////////////////////////////////////////
// custom traits must carry store_cluster_adjacency, but the flag is inert
// today
////////////////////////////////////////////////////////////////////////////////

// network_voronoi_traits *requires* every traits struct to define
// store_cluster_adjacency, so a custom traits struct has to carry it -- but
// the class body never reads it, and no accessor is gated on it. The flag is
// inert today, which is exactly what this test pins down: flipping it changes
// neither the concept check, nor construction, nor the output.
//
// When cluster adjacency is actually implemented, this test is expected to
// fail and should be rewritten to assert the new behaviour.
namespace {
struct network_voronoi_adjacency_traits
    : network_voronoi_default_traits<static_digraph, int> {
    static constexpr bool store_cluster_adjacency = true;
};
}  // namespace

static_assert(network_voronoi_traits<network_voronoi_adjacency_traits>);

GTEST_TEST(network_voronoi, store_cluster_adjacency_is_inert) {
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

    std::vector<vertex_t<static_digraph>> kernels = {0u, 3u, 5u};

    std::vector<std::pair<vertex_t<static_digraph>,
                          std::pair<int, vertex_t<static_digraph>>>>
        expected_output = {
            {0u, std::make_pair(0, 0u)}, {3u, std::make_pair(0, 3u)},
            {5u, std::make_pair(0, 5u)}, {2u, std::make_pair(2, 5u)},
            {4u, std::make_pair(6, 3u)}, {1u, std::make_pair(7, 0u)}};

    ASSERT_TRUE(EQ_RANGES(network_voronoi(network_voronoi_adjacency_traits{},
                                          graph, length_map, kernels),
                          expected_output));
}

////////////////////////////////////////////////////////////////////////////////
// regression (2.3): the Traits default lives on the class, not the deduction
// guides
////////////////////////////////////////////////////////////////////////////////

// Same as dijkstra: with the default on the deduction guides only,
// `network_voronoi<G, LM>` does not compile and CTAD's result cannot be
// named. It lives on the class, and the guides carry none.
namespace traits_default {
using G = views::graph_all_t<static_digraph &>;
using LM = maps::mapping_all_t<static_map<arc_t<static_digraph>, int> &>;

using written_out = network_voronoi<G, LM>;

template <typename... Ts>
using deduced = decltype(network_voronoi(std::declval<Ts>()...));
}  // namespace traits_default

static_assert(
    std::same_as<traits_default::written_out,
                 network_voronoi<
                     traits_default::G, traits_default::LM,
                     network_voronoi_default_traits<traits_default::G, int>>>);
static_assert(std::same_as<
              traits_default::deduced<static_digraph &,
                                      static_map<arc_t<static_digraph>, int> &>,
              traits_default::written_out>);
// the three-argument form, which deduces Kernels too, lands on the same type
static_assert(
    std::same_as<traits_default::deduced<
                     static_digraph &, static_map<arc_t<static_digraph>, int> &,
                     std::vector<vertex_t<static_digraph>> &>,
                 traits_default::written_out>);

GTEST_TEST(network_voronoi, default_traits_spelling_runs) {
    static_digraph_builder<static_digraph, int> builder(4);
    builder.add_arc({0, 1}, 2).add_arc({1, 2}, 3).add_arc({3, 2}, 1);
    auto [graph, length_map] = builder.build();
    std::vector<vertex_t<static_digraph>> kernels = {0u, 3u};

    network_voronoi<views::graph_all_t<decltype(graph) &>,
                    maps::mapping_all_t<decltype(length_map) &>>
        alg(graph, length_map, kernels);

    ASSERT_TRUE(EQ_RANGES(
        alg, std::vector<std::pair<vertex_t<static_digraph>,
                                   std::pair<int, vertex_t<static_digraph>>>>{
                 {0u, {0, 0u}}, {3u, {0, 3u}}, {2u, {1, 3u}}, {1u, {2, 0u}}}));
}

////////////////////////////////////////////////////////////////////////////////
// store_distances / store_clusters expose per-vertex accessors after the run
////////////////////////////////////////////////////////////////////////////////

// Iteration yields each (vertex, (distance, kernel)) once and then forgets it;
// the storing traits keep them for per-vertex lookup, like dijkstra's
// store_distances.
namespace {
struct network_voronoi_storing_traits
    : network_voronoi_default_traits<static_digraph, int> {
    static constexpr bool store_distances = true;
    static constexpr bool store_clusters = true;
};
}  // namespace

static_assert(network_voronoi_traits<network_voronoi_storing_traits>);

// regression: a cluster id names a kernel vertex, so it is the graph's vertex
// type. A hardcoded cluster_id_t = unsigned int silently truncates kernel ids
// on a graph with wider vertex ids (std::pair's forwarding constructor
// bypasses the braced-init narrowing check).
static_assert(std::same_as<
              network_voronoi_default_traits<static_digraph, int>::cluster_id_t,
              vertex_t<static_digraph>>);

// dist()/cluster() require their flag: the default traits store neither.
namespace {
template <typename Alg>
concept has_dist_accessor =
    requires(const Alg & a, vertex_t<static_digraph> v) { a.dist(v); };
template <typename Alg>
concept has_cluster_accessor =
    requires(const Alg & a, vertex_t<static_digraph> v) { a.cluster(v); };
using default_traits_alg =
    network_voronoi<traits_default::G, traits_default::LM>;
}  // namespace
static_assert(!has_dist_accessor<default_traits_alg>);
static_assert(!has_cluster_accessor<default_traits_alg>);

GTEST_TEST(network_voronoi, stored_distances_and_clusters) {
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

    std::vector<vertex_t<static_digraph>> kernels = {0u, 3u, 5u};

    network_voronoi alg(network_voronoi_storing_traits{}, graph, length_map,
                        kernels);
    static_assert(has_dist_accessor<decltype(alg)>);
    static_assert(has_cluster_accessor<decltype(alg)>);
    alg.run();

    ASSERT_EQ(alg.dist(0u), 0);
    ASSERT_EQ(alg.dist(1u), 7);
    ASSERT_EQ(alg.dist(2u), 2);
    ASSERT_EQ(alg.dist(3u), 0);
    ASSERT_EQ(alg.dist(4u), 6);
    ASSERT_EQ(alg.dist(5u), 0);

    ASSERT_EQ(alg.cluster(0u), 0u);
    ASSERT_EQ(alg.cluster(1u), 0u);
    ASSERT_EQ(alg.cluster(2u), 5u);
    ASSERT_EQ(alg.cluster(3u), 3u);
    ASSERT_EQ(alg.cluster(4u), 3u);
    ASSERT_EQ(alg.cluster(5u), 5u);
}
