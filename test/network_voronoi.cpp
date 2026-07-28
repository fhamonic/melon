#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/network_voronoi.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

GTEST_TEST(network_voronoi, test) {
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

    std::vector<vertex_t<static_digraph>> kernels = {0u, 3u, 5u};

    network_voronoi alg(graph, length_map, kernels);

    static_assert(std::copyable<decltype(alg)>);
    std::cout << "network_voronoi size: " << sizeof(decltype(alg)) << std::endl;

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

GTEST_TEST(network_voronoi, test_for) {
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

// ################ traits: store_cluster_adjacency = true #####################

// network_voronoi_trait *requires* every traits struct to define
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

static_assert(network_voronoi_trait<network_voronoi_adjacency_traits>);

GTEST_TEST(network_voronoi, store_cluster_adjacency_is_inert) {
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
