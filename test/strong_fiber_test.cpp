#include <gtest/gtest.h>

#include "melon/algorithm/dijkstra.hpp"
#include "melon/algorithm/strong_fiber.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/erdos_renyi.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(strong_fiber, test) {
    static_digraph_builder<static_digraph, int, int> builder(8);

    builder.add_arc(0, 1, 1, 2);
    builder.add_arc(0, 6, 1, 2);
    builder.add_arc(0, 7, 3, 3);
    builder.add_arc(1, 0, 1, 2);
    builder.add_arc(1, 2, 4, 4);
    builder.add_arc(2, 1, 4, 4);
    builder.add_arc(2, 3, 2, 3);
    builder.add_arc(2, 7, 1, 3);
    builder.add_arc(3, 2, 2, 3);
    builder.add_arc(3, 4, 1, 2);
    builder.add_arc(3, 5, 1, 3);
    builder.add_arc(4, 3, 1, 2);
    builder.add_arc(4, 5, 1, 3);
    builder.add_arc(5, 3, 1, 3);
    builder.add_arc(5, 4, 1, 3);
    builder.add_arc(5, 6, 2, 2);
    builder.add_arc(5, 7, 1, 2);
    builder.add_arc(6, 0, 1, 2);
    builder.add_arc(6, 5, 2, 2);
    builder.add_arc(7, 0, 3, 3);
    builder.add_arc(7, 2, 1, 3);
    builder.add_arc(7, 5, 1, 2);

    auto [graph, reduced_length_map, length_map] = builder.build();

    strong_fiber algo(graph, reduced_length_map, length_map);

    algo.add_strong_arc_source(1);

    std::vector<vertex_t<static_digraph>> strong_nodes;
    for(const auto & [u, u_dist] : algo) {
        strong_nodes.push_back(u);
    }
    algo.reset();

    ASSERT_TRUE(EQ_MULTISETS(strong_nodes, {6, 5, 4}));
}

template <graph G>
auto compute_fiber_map(const G & g, const arc_map_t<G, int> & length_map,
                       const arc_t<G> & uv) {
    const auto & u = arc_source(g, uv);
    const auto & v = arc_target(g, uv);
    auto fiber_map = create_vertex_map<bool>(g, false);
    auto dist_from_u_map =
        create_vertex_map<int>(g, std::numeric_limits<int>::max());
    auto uv_length = length_map[uv];

    for(const auto & [t, t_dist_from_u] : dijkstra(g, length_map, u))
        dist_from_u_map[t] = t_dist_from_u;
    for(const auto & [t, t_dist_from_v] : dijkstra(g, length_map, v))
        fiber_map[t] = (dist_from_u_map[t] == uv_length + t_dist_from_v);

    return fiber_map;
}

GTEST_TEST(strong_fiber, fuzzy) {
    static constexpr std::size_t nb_vertices = 10;
    static constexpr double density = 0.5;
    static constexpr int nb_tests = 10;

    for(int i = 0; i < nb_tests; ++i) {
        auto graph = erdos_renyi<static_digraph>(32, 0.5);
        auto lower_length_map = create_arc_map<int>(graph);
        auto upper_length_map = create_arc_map<int>(graph);

        for(const auto & uv : arcs(graph)) {
            auto strong_map = create_vertex_map<bool>(graph, false);
            auto certificat_length_map = lower_length_map;

            strong_fiber strong_fiber_algo(graph, lower_length_map,
                                           upper_length_map);

            auto u = arc_source(graph, uv);
            strong_fiber_algo.reset().add_strong_arc_source(uv);

            certificat_length_map[uv] = upper_length_map[uv];
            for(const auto & [t, t_dist] : strong_fiber_algo) {
                strong_map[t] = true;
                for(const auto & a : out_arcs(graph, t)) {
                    certificat_length_map[a] = upper_length_map[a];
                }
            }

            auto certificate_fiber_map =
                compute_fiber_map(graph, certificat_length_map, uv);
            for(const auto & v : vertices(graph)) {
                ASSERT_EQ(certificate_fiber_map[v], strong_map[v]);
            }
        }
    }
}
