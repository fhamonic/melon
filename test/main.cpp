#include <iostream>
#include <bitset>

#include "melon/dijkstra.hpp"
#include "melon/static_digraph.hpp"
#include "melon/static_digraph_builder.hpp"

using namespace fhamonic;

// #include "lemon/static_graph.h"
// #include "lemon/smart_graph.h"

using namespace fhamonic::melon;

template <typename T1, typename T2>
std::ostream & operator<<(std::ostream & os, const std::pair<T1, T2> & p) {
    return os << '(' << p.first << ',' << p.second << ')';
}

int main() {
    StaticDigraphBuilder<int> builder(6);

    builder.addArc(0, 1, 7);
    builder.addArc(0, 2, 9);
    builder.addArc(0, 5, 14);
    builder.addArc(1, 0, 7);
    builder.addArc(1, 2, 10);
    builder.addArc(1, 3, 15);
    builder.addArc(2, 0, 9);
    builder.addArc(2, 1, 10);
    builder.addArc(2, 3, 12);
    builder.addArc(2, 5, 2);
    builder.addArc(3, 1, 15);
    builder.addArc(3, 2, 12);
    builder.addArc(3, 4, 6);
    builder.addArc(4, 3, 6);
    builder.addArc(4, 5, 9);
    builder.addArc(5, 0, 14);
    builder.addArc(5, 2, 2);
    builder.addArc(5, 4, 9);

    auto [graph, length_map] = builder.build();

    Dijkstra dijkstra(graph, length_map);
    // std::bitset<3> a;


    dijkstra.addSource(0);
    while(!dijkstra.emptyQueue()) dijkstra.processNextNode();

    std::vector<bool> a;

    return 0;
}
