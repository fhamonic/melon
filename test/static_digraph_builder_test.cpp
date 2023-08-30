#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(static_digraph_builder, build_without_map) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(3, 4);
    builder.add_arc(1, 7);
    builder.add_arc(5, 2);
    builder.add_arc(2, 4);
    builder.add_arc(5, 3);
    builder.add_arc(6, 5);
    builder.add_arc(1, 2);
    builder.add_arc(1, 6);
    builder.add_arc(2, 3);

    auto [graph] = builder.build();

    ASSERT_TRUE(EQ_RANGES(
        arcs_entries(graph),
        std::vector<std::pair<
            arc_t<static_digraph>,
            std::pair<vertex_t<static_digraph>, vertex_t<static_digraph>>>>(
            {{0, {1, 2}},
             {1, {1, 6}},
             {2, {1, 7}},
             {3, {2, 3}},
             {4, {2, 4}},
             {5, {3, 4}},
             {6, {5, 2}},
             {7, {5, 3}},
             {8, {6, 5}}})));
}

GTEST_TEST(static_digraph_builder, build_with_map) {
    constexpr std::size_t n = 8;
    static_digraph_builder<static_digraph, int> builder(n);

    std::vector<std::pair<vertex_t<static_digraph>, vertex_t<static_digraph>>>
        pairs{{3, 4}, {1, 7}, {5, 2}, {2, 4}, {5, 3},
              {6, 5}, {1, 2}, {1, 6}, {2, 3}};

    auto weight = [n](vertex_t<static_digraph> u, vertex_t<static_digraph> v) {
        return static_cast<int>(u * n + v);
    };

    for(auto & [u, v] : pairs) builder.add_arc(u, v, weight(u, v));

    auto [graph, map] = builder.build();

    ASSERT_TRUE(EQ_RANGES(
        arcs_entries(graph),
        std::vector<std::pair<
            arc_t<static_digraph>,
            std::pair<vertex_t<static_digraph>, vertex_t<static_digraph>>>>(
            {{0, {1, 2}},
             {1, {1, 6}},
             {2, {1, 7}},
             {3, {2, 3}},
             {4, {2, 4}},
             {5, {3, 4}},
             {6, {5, 2}},
             {7, {5, 3}},
             {8, {6, 5}}})));

    for(arc_t<static_digraph> a : arcs(graph)) {
        auto u = arc_source(graph,a);
        auto v = arc_target(graph,a);
        ASSERT_EQ(map[a], weight(u, v));
    }
}
