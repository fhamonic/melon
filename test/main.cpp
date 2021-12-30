#include <gtest/gtest.h>
#include <iostream>

#include "melon/static_graph.hpp"

#include "lemon/static_graph.h"
#include "lemon/smart_graph.h"

int main(int argc, char ** argv) {
    StaticDigraphBuilder builder;

    builder.setNbNodes(8);
    builder.add(1,2);
    builder.add(1,6);
    builder.add(1,7);
    builder.add(2,3);
    builder.add(2,4);
    builder.add(3,4);
    builder.add(5,2);
    builder.add(5,3);
    builder.add(6,5);

    StaticDigraph graph = builder.build();

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
    for(const auto & [u,v] : graph.arcs_pairs()) {
        std::cout << u << " " << v << std::endl;
    }

    return 0;
}
