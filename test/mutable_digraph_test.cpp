#include <gtest/gtest.h>

#include "melon/concepts/graph.hpp"
#include "melon/mutable_digraph.hpp"

#include "dumb_digraph.hpp"
#include "random_ranges_helper.hpp"
#include "ranges_test_helper.hpp"

using namespace fhamonic;
using namespace fhamonic::melon;

static_assert(melon::concepts::graph<mutable_digraph>);
static_assert(melon::concepts::outward_incidence_graph<mutable_digraph>);
static_assert(melon::concepts::outward_adjacency_graph<mutable_digraph>);
static_assert(melon::concepts::has_vertex_map<mutable_digraph>);
static_assert(melon::concepts::has_vertex_creation<mutable_digraph>);
static_assert(melon::concepts::has_vertex_removal<mutable_digraph>);
static_assert(melon::concepts::has_arc_creation<mutable_digraph>);
static_assert(melon::concepts::has_arc_removal<mutable_digraph>);
static_assert(melon::concepts::has_change_arc_source<mutable_digraph>);
static_assert(melon::concepts::has_change_arc_target<mutable_digraph>);

using Graph = mutable_digraph;
using arc_entries_list = std::initializer_list<
    std::pair<arc_t<Graph>, std::pair<vertex_t<Graph>, vertex_t<Graph>>>>;

GTEST_TEST(mutable_digraph, empty_constructor) {
    Graph graph;
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_FALSE(is_valid_vertex(graph, 0));
    EXPECT_DEATH((void)out_arcs(graph, 0), "");
    EXPECT_DEATH((void)in_arcs(graph, 0), "");
    EXPECT_DEATH((void)out_neighbors(graph, 0), "");
    EXPECT_DEATH((void)in_neighbors(graph, 0), "");
}

GTEST_TEST(mutable_digraph, create_vertices) {
    Graph graph;

    auto a = create_vertex(graph);
    auto b = create_vertex(graph);
    auto c = create_vertex(graph);

    ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {a, b, c}));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(out_arcs(graph, 0)));
    ASSERT_TRUE(EMPTY(out_arcs(graph, 1)));
    ASSERT_TRUE(EMPTY(out_arcs(graph, 2)));
    ASSERT_TRUE(is_valid_vertex(graph, 2));
    ASSERT_FALSE(is_valid_vertex(graph, 3));
    EXPECT_DEATH((void)out_arcs(graph, 3), "");
}

GTEST_TEST(mutable_digraph, create_arcs) {
    Graph graph;

    auto a = create_vertex(graph);
    auto b = create_vertex(graph);
    auto c = create_vertex(graph);

    auto ab = create_arc(graph, a, b);
    auto ac = create_arc(graph, a, c);
    auto cb = create_arc(graph, c, b);
    auto ca = create_arc(graph, c, a);

    ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {a, b, c}));
    ASSERT_TRUE(EQ_MULTISETS(arcs(graph), {ab, ac, cb, ca}));

    ASSERT_EQ(source(graph, ab), a);
    ASSERT_EQ(source(graph, ac), a);
    ASSERT_EQ(source(graph, cb), c);
    ASSERT_EQ(target(graph, ab), b);
    ASSERT_EQ(target(graph, ac), c);
    ASSERT_EQ(target(graph, cb), b);

    ASSERT_TRUE(EQ_MULTISETS(
        arcs_entries(graph),
        arc_entries_list{
            {ab, {a, b}}, {ac, {a, c}}, {cb, {c, b}}, {ca, {c, a}}}));

    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, a), {b, c}));
    ASSERT_TRUE(EMPTY(out_neighbors(graph, b)));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, c), {a, b}));
}

GTEST_TEST(mutable_digraph, remove_arcs) {
    Graph graph;
    auto a = create_vertex(graph);
    auto b = create_vertex(graph);
    auto c = create_vertex(graph);
    auto ab = create_arc(graph, a, b);
    auto ac = create_arc(graph, a, c);
    auto cb = create_arc(graph, c, b);

    remove_arc(graph, ac);

    ASSERT_FALSE(is_valid_arc(graph, ac));
    ASSERT_EQ(source(graph, ab), a);
    EXPECT_DEATH((void)source(graph, ac), "");
    ASSERT_EQ(source(graph, cb), c);
    ASSERT_EQ(target(graph, ab), b);
    EXPECT_DEATH((void)target(graph, ac), "");
    ASSERT_EQ(target(graph, cb), b);

    ASSERT_TRUE(EQ_MULTISETS(arcs_entries(graph),
                             arc_entries_list{{cb, {c, b}}, {ab, {a, b}}}));

    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, a), {b}));
    ASSERT_TRUE(EMPTY(out_neighbors(graph, b)));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, c), {b}));
}

GTEST_TEST(mutable_digraph, fuzzy_test) {
    mutable_digraph graph;
    dumb_digraph dummy_graph;

    enum Operation {
        CREATE_VERTEX,
        REMOVE_VERTEX,
        CREATE_ARC,
        REMOVE_ARC,
        CHANGE_SOURCE,
        CHANGE_TARGET
    };
    std::vector<Operation> operations = {
        CREATE_VERTEX, REMOVE_VERTEX, CREATE_ARC,    CREATE_ARC,
        CREATE_ARC,    REMOVE_ARC,    CHANGE_SOURCE, CHANGE_TARGET};

    for(std::size_t i = 0; i < 2000; ++i) {
        Operation op;
        for(;;) {
            op = random_element(operations);
            auto nb_vertices = std::ranges::distance(dummy_graph.vertices());
            auto nb_arcs = std::ranges::distance(dummy_graph.arcs());
            if(op == REMOVE_VERTEX && nb_vertices == 0) continue;
            if(op == CREATE_ARC && nb_vertices < 2) continue;
            if(op == REMOVE_ARC && nb_arcs == 0) continue;
            if((op == CHANGE_SOURCE || op == CHANGE_TARGET) &&
               (nb_arcs == 0 || nb_vertices < 3))
                continue;
            break;
        }

        if(op == CREATE_VERTEX) {
            auto u = create_vertex(graph);
            // std::cout << "create_vertex() -> " << u << std::endl;
            dummy_graph.create_vertex(u);
        }
        if(op == REMOVE_VERTEX) {
            auto u = random_element(dummy_graph.vertices());
            // std::cout << "remove_vertex(" << u << ")" << std::endl;
            remove_vertex(graph, u);
            dummy_graph.remove_vertex(u);
        }
        if(op == CREATE_ARC) {
            auto s = random_element(dummy_graph.vertices());
            auto t = random_element(dummy_graph.vertices());
            auto a = create_arc(graph, s, t);
            dummy_graph.create_arc(a, s, t);
            // std::cout << "create_arc(" << s << ", " << t << ") -> " << a
            //           << std::endl;
        }
        if(op == REMOVE_ARC) {
            auto a = random_element(dummy_graph.arcs());
            // std::cout << "remove_arc(" << a << ")" << std::endl;
            remove_arc(graph, a);
            dummy_graph.remove_arc(a);
        }
        if(op == CHANGE_SOURCE) {
            auto a = random_element(dummy_graph.arcs());
            auto s = random_element(dummy_graph.vertices());
            // std::cout << "change_arc_source(" << a << ", " << s << ")" <<
            // std::endl;
            change_arc_source(graph, a, s);
            dummy_graph.change_arc_source(a, s);
        }
        if(op == CHANGE_TARGET) {
            auto a = random_element(dummy_graph.arcs());
            auto t = random_element(dummy_graph.vertices());
            // std::cout << "change_arc_target(" << a << ", " << t << ")" <<
            // std::endl;
            change_arc_target(graph, a, t);
            dummy_graph.change_arc_target(a, t);
        }

        ASSERT_TRUE(EQ_MULTISETS(vertices(graph), dummy_graph.vertices()));
        ASSERT_TRUE(EQ_MULTISETS(arcs(graph), dummy_graph.arcs()));
        ASSERT_TRUE(
            EQ_MULTISETS(arcs_entries(graph), dummy_graph.arcs_entries()));

        // std::cout << "v=";
        for(auto && v : vertices(graph)) {
            // std::cout << v << ",";
            ASSERT_TRUE(is_valid_vertex(graph, v));
            ASSERT_TRUE(dummy_graph.is_valid_vertex(v));
            ASSERT_TRUE(
                EQ_MULTISETS(in_arcs(graph, v), dummy_graph.in_arcs(v)));
            ASSERT_TRUE(
                EQ_MULTISETS(out_arcs(graph, v), dummy_graph.out_arcs(v)));
            ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, v),
                                     dummy_graph.in_neighbors(v)));
            ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, v),
                                     dummy_graph.out_neighbors(v)));
        }
        // std::cout << std::endl;
        for(auto && a : arcs(graph)) {
            ASSERT_TRUE(is_valid_arc(graph, a));
            ASSERT_TRUE(dummy_graph.is_valid_arc(a));
            ASSERT_EQ(target(graph, a), dummy_graph.target(a));
            ASSERT_EQ(source(graph, a), dummy_graph.source(a));
        }
    }
}
