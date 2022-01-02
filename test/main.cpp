#include <iostream>

#include "melon/static_digraph.hpp"
#include "melon/static_digraph_builder.hpp"

using namespace fhamonic;

// #include "lemon/static_graph.h"
// #include "lemon/smart_graph.h"

int main(int argc, char ** argv) {
    melon::StaticDigraphBuilder<double, int> builder(8);

    builder.addArc(3, 4, 3.14, 5.12);
    builder.addArc(1, 7, 3.14, 5.12);
    builder.addArc(5, 2, 3.14, 5.12);
    builder.addArc(2, 4, 3.14, 5.12);
    builder.addArc(5, 3, 3.14, 5.12);
    builder.addArc(6, 5, 3.14, 5.12);
    builder.addArc(1, 2, 3.1415, 5.12);
    builder.addArc(1, 6, 3.14, 5.12);
    builder.addArc(2, 3, 3.14, 5.12);

    auto [graph, map1, map2] = builder.build();

    std::cout << map1[0] << std::endl;

    for(const auto u : graph.nodes()) {
        std::cout << u << std::endl;
    }
    for(const auto a : graph.arcs()) {
        std::cout << a << std::endl;
    }

    std::cout << "out begins: " << std::endl;
    for(const auto & begin : graph.out_arc_begin) {
        std::cout << begin << std::endl;
    }
    std::cout << "out targets: " << std::endl;
    for(const auto & target : graph.arc_target) {
        std::cout << target << std::endl;
    }

    std::cout << "out neighbors of 1: " << std::endl;
    for(const auto & a : graph.out_neighbors(1)) {
        std::cout << a << std::endl;
    }
    std::cout << "pairs: " << std::endl;
    for(const auto & [u, v] : graph.arcs_pairs()) {
        std::cout << u << " " << v << std::endl;
    }

    return 0;
}
