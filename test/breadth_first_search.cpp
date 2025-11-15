#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(breadth_first_search, no_arcs_graph) {
    static_digraph_builder<static_digraph> builder(2);

    auto [graph] = builder.build();

    breadth_first_search alg(graph, 0u);

    static_assert(std::copyable<decltype(alg)>);

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
