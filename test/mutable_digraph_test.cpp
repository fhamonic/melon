#include <gtest/gtest.h>

#include "melon/concepts/graph.hpp"
#include "melon/mutable_digraph.hpp"

#include "dumb_digraph.hpp"
#include "random_ranges_helper.hpp"
#include "ranges_test_helper.hpp"

using namespace fhamonic;
using namespace fhamonic::melon;

static_assert(melon::concepts::graph<mutable_digraph>);
static_assert(melon::concepts::incidence_list_graph<mutable_digraph>);
static_assert(melon::concepts::adjacency_list_graph<mutable_digraph>);
static_assert(melon::concepts::has_vertices_map<mutable_digraph>);
static_assert(melon::concepts::has_vertex_creation<mutable_digraph>);
static_assert(melon::concepts::has_vertex_removal<mutable_digraph>);
static_assert(melon::concepts::has_arc_creation<mutable_digraph>);
static_assert(melon::concepts::has_arc_removal<mutable_digraph>);
static_assert(melon::concepts::has_arc_change_source<mutable_digraph>);
static_assert(melon::concepts::has_arc_change_target<mutable_digraph>);

using Graph = mutable_digraph;
using vertices_pair_list =
    std::initializer_list<std::pair<vertex_t<Graph>, vertex_t<Graph>>>;

GTEST_TEST(mutable_digraph, empty_constructor) {
    Graph graph;
    ASSERT_TRUE(EMPTY(graph.vertices()));
    ASSERT_TRUE(EMPTY(graph.arcs()));
    ASSERT_TRUE(EMPTY(graph.arcs_pairs()));

    ASSERT_FALSE(graph.is_valid_vertex(0));
    EXPECT_DEATH(graph.out_arcs(0), "");
    EXPECT_DEATH(graph.in_arcs(0), "");
    EXPECT_DEATH(graph.out_neighbors(0), "");
    EXPECT_DEATH(graph.in_neighbors(0), "");
}

GTEST_TEST(mutable_digraph, create_vertices) {
    Graph graph;

    auto a = graph.create_vertex();
    auto b = graph.create_vertex();
    auto c = graph.create_vertex();

    ASSERT_TRUE(EQ_MULTISETS(graph.vertices(), {a, b, c}));
    ASSERT_TRUE(EMPTY(graph.arcs()));
    ASSERT_TRUE(EMPTY(graph.out_arcs(0)));
    ASSERT_TRUE(EMPTY(graph.out_arcs(1)));
    ASSERT_TRUE(EMPTY(graph.out_arcs(2)));
    ASSERT_TRUE(graph.is_valid_vertex(2));
    ASSERT_FALSE(graph.is_valid_vertex(3));
    EXPECT_DEATH(graph.out_arcs(3), "");
}

GTEST_TEST(mutable_digraph, create_arcs) {
    Graph graph;

    auto a = graph.create_vertex();
    auto b = graph.create_vertex();
    auto c = graph.create_vertex();

    auto ab = graph.create_arc(a, b);
    auto ac = graph.create_arc(a, c);
    auto cb = graph.create_arc(c, b);
    auto ca = graph.create_arc(c, a);

    ASSERT_TRUE(EQ_MULTISETS(graph.vertices(), {a, b, c}));
    ASSERT_TRUE(EQ_MULTISETS(graph.arcs(), {ab, ac, cb, ca}));

    ASSERT_EQ(graph.source(ab), a);
    ASSERT_EQ(graph.source(ac), a);
    ASSERT_EQ(graph.source(cb), c);
    ASSERT_EQ(graph.target(ab), b);
    ASSERT_EQ(graph.target(ac), c);
    ASSERT_EQ(graph.target(cb), b);

    ASSERT_TRUE(
        EQ_MULTISETS(graph.arcs_pairs(),
                     vertices_pair_list{{a, b}, {a, c}, {c, b}, {c, a}}));

    ASSERT_TRUE(EQ_MULTISETS(graph.out_neighbors(a), {b, c}));
    ASSERT_TRUE(EMPTY(graph.out_neighbors(b)));
    ASSERT_TRUE(EQ_MULTISETS(graph.out_neighbors(c), {a, b}));
}

GTEST_TEST(mutable_digraph, remove_arcs) {
    Graph graph;
    auto a = graph.create_vertex();
    auto b = graph.create_vertex();
    auto c = graph.create_vertex();
    auto ab = graph.create_arc(a, b);
    auto ac = graph.create_arc(a, c);
    auto cb = graph.create_arc(c, b);

    graph.remove_arc(ac);

    ASSERT_FALSE(graph.is_valid_arc(ac));
    ASSERT_EQ(graph.source(ab), a);
    EXPECT_DEATH(graph.source(ac), "");
    ASSERT_EQ(graph.source(cb), c);
    ASSERT_EQ(graph.target(ab), b);
    EXPECT_DEATH(graph.target(ac), "");
    ASSERT_EQ(graph.target(cb), b);

    ASSERT_TRUE(
        EQ_MULTISETS(graph.arcs_pairs(), vertices_pair_list{{c, b}, {a, b}}));

    ASSERT_TRUE(EQ_MULTISETS(graph.out_neighbors(a), {b}));
    ASSERT_TRUE(EMPTY(graph.out_neighbors(b)));
    ASSERT_TRUE(EQ_MULTISETS(graph.out_neighbors(c), {b}));
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
    std::vector<Operation> operations = {CREATE_VERTEX, REMOVE_VERTEX,
                                         CREATE_ARC,    REMOVE_ARC,
                                         CHANGE_SOURCE, CHANGE_TARGET};

    for(std::size_t i = 0; i < 100; ++i) {
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
            auto u = graph.create_vertex();
            std::cout << "create_vertex() -> " << u << std::endl;
            dummy_graph.create_vertex(u);
        }
        if(op == REMOVE_VERTEX) {
            auto u = random_element(dummy_graph.vertices());
            std::cout << "remove_vertex(" << u << ")" << std::endl;
            // graph.remove_vertex(u);
            dummy_graph.remove_vertex(u);
        }
        if(op == CREATE_ARC) {
            auto s = random_element(dummy_graph.vertices());
            auto t = random_element(dummy_graph.vertices());
            auto a = graph.create_arc(s, t);
            dummy_graph.create_arc(a, s, t);
            std::cout << "create_arc(" << s << ", " << t << ") -> " << a
                      << std::endl;
        }
        if(op == REMOVE_ARC) {
            auto a = random_element(dummy_graph.arcs());
            std::cout << "remove_arc(" << a << ")" << std::endl;
            // graph.remove_arc(a);
            dummy_graph.remove_arc(a);
        }
        if(op == CHANGE_SOURCE) {
            auto a = random_element(dummy_graph.arcs());
            auto s = random_element(dummy_graph.vertices());
            std::cout << "change_source(" << a << ", " << s << ")" << std::endl;
            // graph.change_source(a, s);
            dummy_graph.change_source(a, s);
        }
        if(op == CHANGE_TARGET) {
            auto a = random_element(dummy_graph.arcs());
            auto t = random_element(dummy_graph.vertices());
            std::cout << "change_target(" << a << ", " << t << ")" << std::endl;
            // graph.change_target(a, t);
            dummy_graph.change_target(a, t);
        }

        // ASSERT_TRUE(EQ_MULTISETS(graph.vertices(), dummy_graph.vertices()));
        // ASSERT_TRUE(EQ_MULTISETS(graph.arcs(), dummy_graph.arcs()));
        // ASSERT_TRUE(EQ_MULTISETS(graph.arcs_pairs(),
        // dummy_graph.arcs_pairs()));

        for(auto && v : graph.vertices()) {
            ASSERT_TRUE(graph.is_valid_vertex(v));
            // ASSERT_TRUE(dummy_graph.is_valid_vertex(v));
            // ASSERT_TRUE(EQ_MULTISETS(graph.in_arcs(v),
            // dummy_graph.in_arcs(v))); ASSERT_TRUE(
            //     EQ_MULTISETS(graph.out_arcs(v), dummy_graph.out_arcs(v)));
            // ASSERT_TRUE(EQ_MULTISETS(graph.in_neighbors(v),
            //                          dummy_graph.in_neighbors(v)));
            // ASSERT_TRUE(EQ_MULTISETS(graph.out_neighbors(v),
            //                          dummy_graph.out_neighbors(v)));
        }
        for(auto && a : graph.arcs()) {
            ASSERT_TRUE(graph.is_valid_arc(a));
            // ASSERT_TRUE(dummy_graph.is_valid_arc(a));
            // ASSERT_EQ(graph.target(a), dummy_graph.target(a));
            // ASSERT_EQ(graph.source(a), dummy_graph.source(a));
        }
    }
}
