#undef NDEBUG
#include <gtest/gtest.h>

#include <range/v3/view/zip.hpp>

#include "melon/algorithm/strongly_connected_components.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/subgraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(strongly_connected_components, graph1_test) {
    static_assert(views::__cust_access::__detail::__can_graph_ref_view<
                  const static_digraph &>);
    static_assert(
        views::__cust_access::__detail::__can_graph_ref_view<static_digraph &>);
    static_assert(!views::__cust_access::__detail::__can_graph_ref_view<
                  static_digraph &&>);
    static_assert(
        !views::__cust_access::__detail::__can_graph_ref_view<static_digraph>);

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

    strongly_connected_components alg(graph);

    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {5u, 4u, 3u, 2u, 1u, 0u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {6u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {7u}));
    alg.advance();
    ASSERT_TRUE(alg.finished());
}

GTEST_TEST(strongly_connected_components, graph1_algorithm_iterator) {
    using vertex = vertex_t<static_digraph>;
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

    std::vector<std::vector<vertex>> components(
        {{5u, 4u, 3u, 2u, 1u, 0u}, {6u}, {7u}});

    std::vector<std::vector<vertex>> alg_components;

    for(auto component : strongly_connected_components(graph)) {
        auto & alg_component = alg_components.emplace_back();
        for(vertex v : component) {
            alg_component.push_back(v);
        }
    }

    for(auto && [component, alg_component] :
        ranges::zip_view(components, alg_components)) {
        ASSERT_TRUE(EQ_MULTISETS(component, alg_component));
    }
}

GTEST_TEST(strongly_connected_components, graph1_components_count) {
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

    int component_count = 0;
    for(auto component : strongly_connected_components(graph)) {
        ++component_count;
    }

    ASSERT_EQ(component_count, 3);
}

GTEST_TEST(strongly_connected_components, graph2_test) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(0, 1)
        .add_arc(1, 2)
        .add_arc(2, 0)
        .add_arc(3, 1)
        .add_arc(3, 2)
        .add_arc(3, 5)
        .add_arc(4, 2)
        .add_arc(4, 6)
        .add_arc(5, 3)
        .add_arc(5, 4)
        .add_arc(6, 4)
        .add_arc(7, 5)
        .add_arc(7, 6);

    auto [graph] = builder.build();

    strongly_connected_components alg(graph);

    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {2u, 1u, 0u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {6u, 4u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {5u, 3u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {7u}));
    alg.advance();
    ASSERT_TRUE(alg.finished());
}

GTEST_TEST(strongly_connected_components, graph2_algorithm_iterator) {
    using vertex = vertex_t<static_digraph>;
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(0, 1)
        .add_arc(1, 2)
        .add_arc(2, 0)
        .add_arc(3, 1)
        .add_arc(3, 2)
        .add_arc(3, 5)
        .add_arc(4, 2)
        .add_arc(4, 6)
        .add_arc(5, 3)
        .add_arc(5, 4)
        .add_arc(6, 4)
        .add_arc(7, 5)
        .add_arc(7, 6);

    auto [graph] = builder.build();

    std::vector<std::vector<vertex>> components(
        {{2u, 1u, 0u}, {6u, 4u}, {5u, 3u}, {7u}});

    std::vector<std::vector<vertex>> alg_components;

    for(auto component : strongly_connected_components(graph)) {
        auto & alg_component = alg_components.emplace_back();
        for(vertex v : component) {
            alg_component.push_back(v);
        }
    }

    for(auto && [component, alg_component] :
        ranges::zip_view(components, alg_components)) {
        ASSERT_TRUE(EQ_MULTISETS(component, alg_component));
    }
}

GTEST_TEST(strongly_connected_components, graph2_components_count) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(0, 1)
        .add_arc(1, 2)
        .add_arc(2, 0)
        .add_arc(3, 1)
        .add_arc(3, 2)
        .add_arc(3, 5)
        .add_arc(4, 2)
        .add_arc(4, 6)
        .add_arc(5, 3)
        .add_arc(5, 4)
        .add_arc(6, 4)
        .add_arc(7, 5)
        .add_arc(7, 6);

    auto [graph] = builder.build();

    int component_count = 0;
    for(auto component : strongly_connected_components(graph)) {
        ++component_count;
    }

    ASSERT_EQ(component_count, 4);
}

GTEST_TEST(strongly_connected_components, subgraph_true_map_test) {
    using vertex = vertex_t<static_digraph>;
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(0, 1)
        .add_arc(1, 2)
        .add_arc(2, 0)
        .add_arc(3, 1)
        .add_arc(3, 2)
        .add_arc(3, 5)
        .add_arc(4, 2)
        .add_arc(4, 6)
        .add_arc(5, 3)
        .add_arc(5, 4)
        .add_arc(6, 4)
        .add_arc(7, 5)
        .add_arc(7, 6);

    auto [graph] = builder.build();

    auto sgraph = views::subgraph(graph, views::true_map{}, views::true_map{});

    std::vector<std::vector<vertex>> components(
        {{2u, 1u, 0u}, {6u, 4u}, {5u, 3u}, {7u}});
    std::vector<std::vector<vertex>> alg_components;

    for(auto component : strongly_connected_components(sgraph)) {
        auto & alg_component = alg_components.emplace_back();
        for(vertex v : component) {
            alg_component.push_back(v);
        }
    }

    for(auto && [component, alg_component] :
        ranges::zip_view(components, alg_components)) {
        ASSERT_TRUE(EQ_MULTISETS(component, alg_component));
    }
}

GTEST_TEST(strongly_connected_components, subgraph_true_map2_test) {
    using vertex = vertex_t<static_digraph>;
    static_digraph_builder<static_digraph, char> builder(8);

    builder.add_arc(0, 1, true)
        .add_arc(1, 2, true)
        .add_arc(2, 0, true)
        .add_arc(3, 1, true)
        .add_arc(3, 2, true)
        .add_arc(3, 5, true)
        .add_arc(4, 2, true)
        .add_arc(4, 6, true)
        .add_arc(5, 3, true)
        .add_arc(5, 4, true)
        .add_arc(6, 4, true)
        .add_arc(7, 5, true)
        .add_arc(7, 6, true);

    auto [graph, filter_map] = builder.build();

    std::vector<std::vector<vertex>> components(
        {{2u, 1u, 0u}, {6u, 4u}, {5u, 3u}, {7u}});
    std::vector<std::vector<vertex>> alg_components;

    for(auto && component :
        strongly_connected_components(views::subgraph(graph, {}, filter_map))) {
        auto & alg_component = alg_components.emplace_back();
        for(auto && v : component) {
            alg_component.push_back(v);
        }
    }

    for(auto && [component, alg_component] :
        ranges::zip_view(components, alg_components)) {
        ASSERT_TRUE(EQ_MULTISETS(component, alg_component));
    }
}

GTEST_TEST(strongly_connected_components, subgraph_test) {
    using vertex = vertex_t<static_digraph>;
    static_digraph_builder<static_digraph, char> builder(8);

    builder.add_arc(0, 1, true)
        .add_arc(1, 2, true)
        .add_arc(2, 0, true)
        .add_arc(3, 1, true)
        .add_arc(3, 2, true)
        .add_arc(3, 5, true)
        .add_arc(4, 2, true)
        .add_arc(4, 6, true)
        .add_arc(5, 3, true)
        .add_arc(5, 4, true)
        .add_arc(6, 4, false)
        .add_arc(7, 5, true)
        .add_arc(7, 6, true);

    auto [graph, filter_map] = builder.build();

    std::vector<std::vector<vertex>> components(
        {{2u, 1u, 0u}, {6u}, {4u}, {5u, 3u}, {7u}});
    std::vector<std::vector<vertex>> alg_components;

    for(auto component :
        strongly_connected_components(views::subgraph(graph, {}, filter_map))) {
        auto & alg_component = alg_components.emplace_back();
        for(vertex v : component) {
            alg_component.push_back(v);
        }
    }

    for(auto && [component, alg_component] :
        ranges::zip_view(components, alg_components)) {
        ASSERT_TRUE(EQ_MULTISETS(component, alg_component));
    }
}

GTEST_TEST(strongly_connected_components, no_arcs_test) {
    using vertex = vertex_t<static_digraph>;
    static_digraph_builder<static_digraph> builder(2);

    auto [graph] = builder.build();

    std::vector<std::vector<vertex>> components({{0u}, {1u}});
    std::vector<std::vector<vertex>> alg_components;

    static_assert(std::same_as<views::graph_all_t<decltype(std::move(graph))>,
                               graph_owning_view<static_digraph>>);

    for(auto component : strongly_connected_components(std::move(graph))) {
        auto & alg_component = alg_components.emplace_back();
        for(vertex v : component) {
            alg_component.push_back(v);
        }
    }

    for(auto && [component, alg_component] :
        ranges::zip_view(components, alg_components)) {
        ASSERT_TRUE(EQ_MULTISETS(component, alg_component));
    }
}

GTEST_TEST(strongly_connected_components, subgraph_lambda_test) {
    using vertex = vertex_t<static_digraph>;
    using arc = arc_t<static_digraph>;
    static_digraph_builder<static_digraph, double> builder(6);

    builder.add_arc(0, 1, 0.0)
        .add_arc(2, 3, 1.0)
        .add_arc(4, 5, 1.0)
        .add_arc(1, 2, 0.0)
        .add_arc(3, 0, 0.0)
        .add_arc(3, 4, 1.0)
        .add_arc(5, 2, 1.0)
        .add_arc(5, 0, 1.0)
        .add_arc(1, 4, 0.0);

    auto [graph, filter_map] = builder.build();

    std::vector<std::vector<vertex>> components({{0u}, {1u}, {5u, 4u, 3u, 2u}});
    std::vector<std::vector<vertex>> alg_components;

    for(auto && component : strongly_connected_components(views::subgraph(
            graph, {}, [&](const arc & a) { return filter_map[a] == 1.0; }))) {
        auto & alg_component = alg_components.emplace_back();
        for(auto && v : component) {
            alg_component.push_back(v);
        }
    }

    for(auto && [component, alg_component] :
        ranges::zip_view(components, alg_components)) {
        ASSERT_TRUE(EQ_MULTISETS(component, alg_component));
    }
}
