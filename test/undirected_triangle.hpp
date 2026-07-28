#ifndef UNDIRECTED_TRIANGLE_HPP
#define UNDIRECTED_TRIANGLE_HPP

#include <cstddef>
#include <ranges>
#include <utility>
#include <vector>

// The triangle 0-1, 1-2, 2-0, exposed only through free functions so that the
// CPOs of melon/undirected_graph.hpp are resolved through their ADL branch.
// It is deliberately not a melon view: it is the concrete undirected graph the
// view adaptors are tested against.
namespace adl_ugraph {

using vertex = unsigned int;
using edge = unsigned int;
using endpoints = std::pair<vertex, vertex>;

struct triangle {
    std::vector<endpoints> edge_list{{0, 1}, {1, 2}, {2, 0}};
};

inline auto vertices(const triangle &) { return std::views::iota(0u, 3u); }
inline vertex num_vertices(const triangle &) { return 3u; }
inline auto edges(const triangle & g) {
    return std::views::iota(0u, static_cast<edge>(g.edge_list.size()));
}
inline edge num_edges(const triangle & g) {
    return static_cast<edge>(g.edge_list.size());
}
inline endpoints edge_endpoints(const triangle & g, const edge & e) {
    return g.edge_list[e];
}
inline unsigned int degree(const triangle & g, const vertex & v) {
    unsigned int d = 0;
    for(const auto & [u, w] : g.edge_list) {
        if(u == v) ++d;
        if(w == v) ++d;
    }
    return d;
}
// Deliberately an unsized range: the degree CPO must not be able to fall back
// on std::ranges::size here, it has to reach the free function above.
inline auto incidence(const triangle & g, const vertex & v) {
    return std::views::transform(
        std::views::filter(edges(g),
                           [&g, v](const edge & e) {
                               return g.edge_list[e].first == v ||
                                      g.edge_list[e].second == v;
                           }),
        [&g, v](const edge & e) {
            const auto & [u, w] = g.edge_list[e];
            return std::pair<edge, vertex>{e, u == v ? w : u};
        });
}
template <typename V>
auto create_vertex_map(const triangle &) {
    return std::vector<V>(3);
}
template <typename V>
auto create_vertex_map(const triangle &, const V & d) {
    return std::vector<V>(3, d);
}
template <typename V>
auto create_edge_map(const triangle & g) {
    return std::vector<V>(g.edge_list.size());
}
template <typename V>
auto create_edge_map(const triangle & g, const V & d) {
    return std::vector<V>(g.edge_list.size(), d);
}

}  // namespace adl_ugraph

#endif  // UNDIRECTED_TRIANGLE_HPP
