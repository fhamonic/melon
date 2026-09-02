#undef NDEBUG
#include <gtest/gtest.h>

#include <utility>

#include "melon/algorithm/strongly_connected_components.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/maps/constant.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/subgraph.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// the algorithm yields the strongly connected components one advance() at a
// time, current() naming the vertices of the current one
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(strongly_connected_components, graph1_test) {
    static_assert(
        views::cpo::detail::can_graph_ref_view<const static_digraph &>);
    static_assert(views::cpo::detail::can_graph_ref_view<static_digraph &>);
    static_assert(!views::cpo::detail::can_graph_ref_view<static_digraph &&>);
    static_assert(!views::cpo::detail::can_graph_ref_view<static_digraph>);

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

////////////////////////////////////////////////////////////////////////////////
// the algorithm is a range whose elements are the component vertex ranges
////////////////////////////////////////////////////////////////////////////////

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
        static_assert(std::ranges::range<decltype(component)>);
        auto & alg_component = alg_components.emplace_back();
        for(vertex v : component) {
            alg_component.push_back(v);
        }
    }

    for(auto && [component, alg_component] :
        std::views::zip(components, alg_components)) {
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
        (void)component;
        ++component_count;
    }

    ASSERT_EQ(component_count, 3);
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
        std::views::zip(components, alg_components)) {
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
        (void)component;
        ++component_count;
    }

    ASSERT_EQ(component_count, 4);
}

////////////////////////////////////////////////////////////////////////////////
// a graph with no arcs yields singleton components, and an rvalue graph is
// owned by the algorithm's view
////////////////////////////////////////////////////////////////////////////////

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
        std::views::zip(components, alg_components)) {
        ASSERT_TRUE(EQ_MULTISETS(component, alg_component));
    }
}

////////////////////////////////////////////////////////////////////////////////
// the algorithm respects the filters of subgraph views, whatever form the
// filter map takes
////////////////////////////////////////////////////////////////////////////////

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

    auto sgraph = views::subgraph(graph, maps::true_map{}, maps::true_map{});

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
        std::views::zip(components, alg_components)) {
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
        std::views::zip(components, alg_components)) {
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
        std::views::zip(components, alg_components)) {
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
        std::views::zip(components, alg_components)) {
        ASSERT_TRUE(EQ_MULTISETS(component, alg_component));
    }
}

////////////////////////////////////////////////////////////////////////////////
// reset() restores the constructor's state exactly, first component included
////////////////////////////////////////////////////////////////////////////////

// reset() has to restore the constructor's state exactly, first component
// included. _index_map left behind -- and reached() is what reads it -- makes
// every vertex still look visited, so a restarted run walks straight to
// finished(); the constructor's advance() has to be redone as well.
GTEST_TEST(strongly_connected_components, reset_restarts_the_whole_run) {
    // one 3-cycle plus an isolated vertex: two components
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(1, 2).add_arc(2, 0);
    auto [graph] = builder.build();

    strongly_connected_components alg(graph);

    const auto count_components = [](auto & a) {
        int n = 0;
        for(; !a.finished(); a.advance()) ++n;
        return n;
    };
    const auto collect_first = [](auto & a) {
        return std::ranges::to<std::vector<vertex_t<static_digraph>>>(
            a.current());
    };

    const auto first_component = collect_first(alg);
    ASSERT_EQ(count_components(alg), 2);
    // num_components() agrees, default traits included
    ASSERT_EQ(alg.num_components(), 2u);

    alg.reset();

    // current() is valid straight after reset(), just as it is after the
    // constructor, and it names the same component
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_RANGES(alg.current(), first_component));
    ASSERT_EQ(count_components(alg), 2);

    // and again, to show reset() is idempotent rather than merely non-empty
    alg.reset();
    ASSERT_EQ(count_components(alg), 2);
}

////////////////////////////////////////////////////////////////////////////////
// a graph with no vertices is finished() from the start
////////////////////////////////////////////////////////////////////////////////

// advance() reads _remaining_vertices.current(); called unconditionally from
// the constructor, it reads past the end of an empty view on a graph with no
// vertices. Such a graph is finished() from the start.
GTEST_TEST(strongly_connected_components, empty_graph) {
    static_digraph_builder<static_digraph> builder(0);
    auto [graph] = builder.build();

    strongly_connected_components alg(graph);
    ASSERT_TRUE(alg.finished());

    int component_count = 0;
    for(auto component : strongly_connected_components(graph)) {
        (void)component;
        ++component_count;
    }
    ASSERT_EQ(component_count, 0);

    alg.reset();
    ASSERT_TRUE(alg.finished());
}

////////////////////////////////////////////////////////////////////////////////
// with store_component_ids the algorithm writes each vertex's component id
// when its component is popped; equal component_id() is the same-component
// query
////////////////////////////////////////////////////////////////////////////////

struct scc_store_ids_traits {
    static constexpr bool store_component_ids = true;
};

// The lowlink counterexample: for the single-component graph 0->1, 0->2,
// 1->0, 2->1 the finished lowlinks are not uniform, so a same-component query
// answered by comparing lowlinks says yes for (0,1) but no for (0,2). The
// stored ids are uniform.
GTEST_TEST(strongly_connected_components, component_ids_single_scc) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc(0, 1).add_arc(0, 2).add_arc(1, 0).add_arc(2, 1);
    auto [graph] = builder.build();

    strongly_connected_components alg(scc_store_ids_traits{}, graph);
    alg.run();

    ASSERT_EQ(alg.component_id(0u), 0u);
    ASSERT_EQ(alg.component_id(1u), 0u);
    ASSERT_EQ(alg.component_id(2u), 0u);
}

// Ids are dense and follow emission order -- the k-th component yielded is id
// k -- and equal ids answer the same-component question.
GTEST_TEST(strongly_connected_components, component_ids_emission_order) {
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

    strongly_connected_components alg(scc_store_ids_traits{}, graph);

    // the ids of the current component's members are readable as soon as
    // current() names them, before the run is finished; num_components()
    // counts the components yielded so far, current() included
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {2u, 1u, 0u}));
    ASSERT_EQ(alg.num_components(), 1u);
    ASSERT_EQ(alg.component_id(0u), 0u);

    alg.run();
    ASSERT_EQ(alg.num_components(), 4u);

    // components come out {0,1,2}, {4,6}, {3,5}, {7}; equal ids answer the
    // same-component query
    for(auto && [v, id] :
        std::vector<std::pair<unsigned, unsigned>>{{0u, 0u},
                                                   {1u, 0u},
                                                   {2u, 0u},
                                                   {4u, 1u},
                                                   {6u, 1u},
                                                   {3u, 2u},
                                                   {5u, 2u},
                                                   {7u, 3u}}) {
        ASSERT_EQ(alg.component_id(v), id);
    }

    // component_ids_map() views the same ids, for passing to anything that
    // takes a vertex map
    auto ids = alg.component_ids_map();
    for(auto && v : melon::vertices(graph)) {
        ASSERT_EQ(ids[v], alg.component_id(v));
    }

    // reset() clears the ids and the count, and a rerun reassigns the same
    // ones
    alg.reset();
    alg.run();
    ASSERT_EQ(alg.num_components(), 4u);
    ASSERT_EQ(alg.component_id(7u), 3u);
}

// Without the flag the map is not stored and the id queries do not exist --
// and there is no same_component() at all: one answering from lowlinks would
// compare values that are not uniform within a finished component.
template <typename A>
concept answers_component_id = requires(
    const A & a, const vertex_t<static_digraph> & v) { a.component_id(v); };
template <typename A>
concept answers_component_ids_map =
    requires(const A & a) { a.component_ids_map(); };
template <typename A>
concept answers_same_component =
    requires(const A & a, const vertex_t<static_digraph> & v) {
        a.same_component(v, v);
    };

GTEST_TEST(strongly_connected_components, no_ids_without_the_flag) {
    using default_alg =
        strongly_connected_components<graph_ref_view<static_digraph>>;
    static_assert(!answers_component_id<default_alg>);
    static_assert(!answers_component_ids_map<default_alg>);
    // the count is not gated: it is meaningful without the ids
    static_assert(requires(const default_alg & a) { a.num_components(); });

    using ids_alg =
        strongly_connected_components<graph_ref_view<static_digraph>,
                                      scc_store_ids_traits>;
    static_assert(answers_component_id<ids_alg>);
    static_assert(answers_component_ids_map<ids_alg>);
    // no configuration exposes a same_component()
    static_assert(!answers_same_component<default_alg>);
    static_assert(!answers_same_component<ids_alg>);
    // the guarded map is [[no_unique_address]] air when the flag is off
    static_assert(sizeof(default_alg) < sizeof(ids_alg));
}

////////////////////////////////////////////////////////////////////////////////
// regression: an algorithm lvalue must not be swallowed by the greedy
// single-argument constructor
////////////////////////////////////////////////////////////////////////////////

// An unconstrained `strongly_connected_components(T &&)` beats the copy
// constructor for a non-const lvalue and tries to build the algorithm out of
// itself; detail::not_self is what excludes it. With copy deleted, the pin is
// that an algorithm is not constructible from an algorithm lvalue at all.
GTEST_TEST(strongly_connected_components,
           is_not_constructible_from_an_algorithm) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(1, 0).add_arc(1, 2).add_arc(2, 3);
    auto [graph] = builder.build();

    strongly_connected_components alg(graph);
    using alg_t = decltype(alg);
    static_assert(!std::is_constructible_v<alg_t, alg_t &>);
    static_assert(!std::is_constructible_v<alg_t, const alg_t &>);
    static_assert(std::is_constructible_v<alg_t, alg_t &&>);

    std::vector<unsigned> first(alg.current().begin(), alg.current().end());
    auto relocated = std::move(alg);
    ASSERT_TRUE(EQ_MULTISETS(relocated.current(), first));
    relocated.advance();
    ASSERT_FALSE(EQ_MULTISETS(relocated.current(), first));
}
