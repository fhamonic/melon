#include <iostream>

#include "melon/static_graph.hpp"

// #include "lemon/static_graph.h"
// #include "lemon/smart_graph.h"

auto fill_arcs() {
    melon::StaticDigraph::ArcList arcs;
    arcs.emplace_back(1,2);
    arcs.emplace_back(1,6);
    arcs.emplace_back(1,7);
    arcs.emplace_back(2,3);
    arcs.emplace_back(2,4);
    arcs.emplace_back(3,4);
    arcs.emplace_back(5,2);
    arcs.emplace_back(5,3);
    arcs.emplace_back(6,5);
    return arcs;
}

int main(int argc, char ** argv) {
    melon::StaticDigraph::ArcList arcs = fill_arcs();

    melon::StaticDigraph graph(8, std::move(arcs));

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
