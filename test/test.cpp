#include <gtest/gtest.h>
#include <iostream>

#include "algorithms/identify_strong_arcs.h"

int main(int argc, char ** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

GTEST_TEST(IdentifyStrong2, test) {
    std::cout << "1 == 1 ?" << std::endl;
    EXPECT_EQ(1, 1);
    using Graph = lemon::ListDigraph;
    using Node = Graph::Node;
    using Arc = Graph::Arc;
    using ArcMap = Graph::ArcMap<double>;

    Graph graph;
    ArcMap lower(graph), upper(graph);
    auto addArc = [&](Node u, Node v, double l1, double l2) {
        Arc uv = graph.addArc(u, v);
        lower[uv] = l1;
        upper[uv] = l2;
        return uv;
    };

    Node a = graph.addNode();
    Node b = graph.addNode();
    Node c = graph.addNode();
    Node d = graph.addNode();
    Node e = graph.addNode();
    Node f = graph.addNode();
    Node g = graph.addNode();
    Node h = graph.addNode();

    Arc ab = addArc(a, b, 1, 2);
    Arc ag = addArc(a, g, 1, 2);
    Arc ah = addArc(a, h, 3, 3);
    Arc ba = addArc(b, a, 1, 2);
    Arc bc = addArc(b, c, 4, 4);
    Arc cb = addArc(c, b, 4, 4);
    Arc cd = addArc(c, d, 2, 3);
    Arc ch = addArc(c, h, 1, 3);
    Arc dc = addArc(d, c, 2, 3);
    Arc de = addArc(d, e, 1, 2);
    Arc df = addArc(d, f, 1, 3);
    Arc ed = addArc(e, d, 1, 2);
    Arc ef = addArc(e, f, 1, 3);
    Arc fd = addArc(f, d, 1, 3);
    Arc fe = addArc(f, e, 1, 3);
    Arc fg = addArc(f, g, 2, 2);
    Arc fh = addArc(f, h, 1, 2);
    Arc ga = addArc(g, a, 1, 2);
    Arc gf = addArc(g, f, 2, 2);
    Arc ha = addArc(h, a, 3, 3);
    Arc hc = addArc(h, c, 1, 3);
    Arc hf = addArc(h, f, 1, 2);

    lemon::IdentifyStrong<Graph, ArcMap> identifyStrong(graph, upper, lower);
    identifyStrong.run(ag);
    auto strong_nodes = identifyStrong.getLabeledNodesList();

    EXPECT_EQ(strong_nodes.size(), 3);
    EXPECT_EQ(strong_nodes[0], g);
    EXPECT_EQ(strong_nodes[1], f);
    EXPECT_EQ(strong_nodes[2], e);
}

GTEST_TEST(IdentifyStrong1, test) {
    std::cout << "1 == 1 ?" << std::endl;
    EXPECT_EQ(1, 1);
    using Graph = lemon::ListDigraph;
    using Node = Graph::Node;
    using Arc = Graph::Arc;
    using ArcMap = Graph::ArcMap<double>;

    Graph graph;
    ArcMap lower(graph), upper(graph);
    auto addArc = [&](Node u, Node v, double l1, double l2) {
        Arc uv = graph.addArc(u, v);
        lower[uv] = l1;
        upper[uv] = l2;
        return uv;
    };

    Node a = graph.addNode();
    Node b = graph.addNode();
    Node c = graph.addNode();

    Arc ab = addArc(a, b, 1, 2);
    Arc bc = addArc(b, c, 3, 3);

    lemon::IdentifyStrong<Graph, ArcMap> identifyStrong(graph, upper, lower);
    identifyStrong.run(ab);
    auto strong_nodes = identifyStrong.getLabeledNodesList();

    EXPECT_EQ(strong_nodes.size(), 2);
    EXPECT_EQ(strong_nodes[0], b);
    EXPECT_EQ(strong_nodes[1], c);
}
