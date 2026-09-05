#ifndef BOTH_PROTOCOLS_TRIANGLE_HPP
#define BOTH_PROTOCOLS_TRIANGLE_HPP

#include <ranges>
#include <utility>
#include <vector>

// The triangle 0-1, 1-2, 2-0 modelling both protocols at once, the way an
// undirected container with a directed reading would: edge e is the two arcs
// 2e (first endpoint towards second) and 2e + 1 (back). Members throughout,
// so the wrappers' forwarding is what is under test, not ADL.
namespace both_protocols {

using vertex = unsigned int;
using edge = unsigned int;
using arc = unsigned int;
using endpoints = std::pair<vertex, vertex>;

struct triangle {
    std::vector<endpoints> edge_list{{0, 1}, {1, 2}, {2, 0}};

    auto vertices() const { return std::views::iota(0u, 3u); }
    vertex num_vertices() const { return 3u; }

    auto edges() const {
        return std::views::iota(0u, static_cast<edge>(edge_list.size()));
    }
    edge num_edges() const { return static_cast<edge>(edge_list.size()); }
    endpoints edge_endpoints(const edge & e) const { return edge_list[e]; }
    // A member: incidence() below is a filter, so the size fallback cannot
    // answer, and has_degree must come from here.
    unsigned int degree(const vertex & v) const {
        unsigned int d = 0;
        for(const auto & [u, w] : edge_list) {
            if(u == v) ++d;
            if(w == v) ++d;
        }
        return d;
    }
    // A self-loop is listed once per end, twice, as degree() counts it.
    auto incidence(const vertex & v) const {
        return std::views::join(
            std::views::transform(edges(), [this, v](const edge & e) {
                const auto & [u, w] = edge_list[e];
                return std::views::repeat(
                    std::pair<edge, vertex>{e, u == v ? w : u},
                    (u == v ? 1 : 0) + (w == v ? 1 : 0));
            }));
    }
    template <typename V>
    auto create_edge_map() const {
        return std::vector<V>(edge_list.size());
    }
    template <typename V>
    auto create_edge_map(const V & d) const {
        return std::vector<V>(edge_list.size(), d);
    }

    auto arcs() const {
        return std::views::iota(0u, static_cast<arc>(2 * edge_list.size()));
    }
    arc num_arcs() const { return static_cast<arc>(2 * edge_list.size()); }
    vertex arc_source(const arc & a) const {
        return a % 2 ? edge_list[a / 2].second : edge_list[a / 2].first;
    }
    vertex arc_target(const arc & a) const {
        return a % 2 ? edge_list[a / 2].first : edge_list[a / 2].second;
    }
    auto out_arcs(const vertex & v) const {
        return std::views::filter(
            arcs(), [this, v](const arc & a) { return arc_source(a) == v; });
    }
    auto in_arcs(const vertex & v) const {
        return std::views::filter(
            arcs(), [this, v](const arc & a) { return arc_target(a) == v; });
    }
    template <typename V>
    auto create_arc_map() const {
        return std::vector<V>(2 * edge_list.size());
    }
    template <typename V>
    auto create_arc_map(const V & d) const {
        return std::vector<V>(2 * edge_list.size(), d);
    }

    template <typename V>
    auto create_vertex_map() const {
        return std::vector<V>(3);
    }
    template <typename V>
    auto create_vertex_map(const V & d) const {
        return std::vector<V>(3, d);
    }
};

}  // namespace both_protocols

#endif  // BOTH_PROTOCOLS_TRIANGLE_HPP
