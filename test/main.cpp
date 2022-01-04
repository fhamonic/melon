#include <iostream>

#include "melon/static_digraph.hpp"
#include "melon/static_digraph_builder.hpp"
#include "melon/dijkstra.hpp"

using namespace fhamonic;

// #include "lemon/static_graph.h"
// #include "lemon/smart_graph.h"

int main() {
    melon::StaticDigraphBuilder<double, int> builder(8);

    builder.addArc(3, 4, 3.14, 9);
    builder.addArc(1, 7, 3.14, 9);
    builder.addArc(5, 2, 3.14, 9);
    builder.addArc(2, 4, 3.14, 9);
    builder.addArc(5, 3, 3.14, 9);
    builder.addArc(6, 5, 3.14, 9);
    builder.addArc(1, 2, 3.1415, 9);
    builder.addArc(1, 6, 3.14, 9);
    builder.addArc(2, 3, 3.14, 9);

    auto [graph, map1, map2] = builder.build();

    for(const auto u : graph.nodes()) {
        std::cout << u << std::endl;
    }
    for(const auto a : graph.arcs()) {
        std::cout << a << std::endl;
    }

    std::cout << "out neighbors of 1: " << std::endl;
    for(const auto & a : graph.out_neighbors(1)) {
        std::cout << a << std::endl;
    }
    std::cout << "pairs: " << std::endl;
    for(const auto & [u, v] : graph.arcs_pairs()) {
        std::cout << u << " " << v << std::endl;
    }

    std::cout << "dijkstra" << std::endl;
    melon::Dijkstra<melon::StaticDigraph, melon::StaticDigraph::ArcMap<double>> dijkstra(graph, map1);
    dijkstra.init(1);
    while(!dijkstra.emptyQueue()) {
        auto [u, dist] = dijkstra.processNextNode();
        std::cout << u << " at " << dist << std::endl;
    }

    return 0;
}
