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

    builder.add_arc(0, 1, 7);
    builder.add_arc(0, 2, 9);
    builder.add_arc(0, 5, 14);
    builder.add_arc(1, 0, 7);
    builder.add_arc(1, 2, 10);
    builder.add_arc(1, 3, 15);
    builder.add_arc(2, 0, 9);
    builder.add_arc(2, 1, 10);
    builder.add_arc(2, 3, 12);
    builder.add_arc(2, 5, 2);
    builder.add_arc(3, 1, 15);
    builder.add_arc(3, 2, 12);
    builder.add_arc(3, 4, 6);
    builder.add_arc(4, 3, 6);
    builder.add_arc(4, 5, 9);
    builder.add_arc(5, 0, 14);
    builder.add_arc(5, 2, 2);
    builder.add_arc(5, 4, 9);

    auto [graph, length_map] = builder.build();

    Dijkstra dijkstra(graph, length_map);
    // std::bitset<3> a;


    dijkstra.add_source(0);
    while(!dijkstra.empty_queue()) dijkstra.next_node();

    std::vector<bool> a;

    return 0;
}
