#include <gtest/gtest.h>

#include "melon/algorithm/dijkstra.hpp"
#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/algorithm/strong_fiber.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/erdos_renyi.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "melon/utility/graphviz_printer.hpp"

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

GTEST_TEST(strong_fiber, test2) {
    static_digraph_builder<static_digraph, int, int> builder(8);

    builder.add_arc(0, 7, 3, 4)
        .add_arc(1, 0, 11, 13)
        .add_arc(2, 1, 4, 19)
        .add_arc(2, 6, 17, 18)
        .add_arc(3, 4, 13, 19)
        .add_arc(3, 5, 17, 18)
        .add_arc(3, 7, 8, 17)
        .add_arc(4, 1, 7, 19)
        .add_arc(5, 6, 5, 9)
        .add_arc(6, 3, 13, 17)
        .add_arc(7, 6, 19, 20);

    auto [graph, reduced_length_map, length_map] = builder.build();

    strong_fiber algo(graph, reduced_length_map, length_map);

    algo.add_strong_arc_source(5);

    std::vector<vertex_t<static_digraph>> strong_nodes;
    for(const auto & [u, u_dist] : algo) {
        strong_nodes.push_back(u);
    }
    algo.reset();

    ASSERT_TRUE(EQ_MULTISETS(strong_nodes, {6, 5}));
}

template <graph G>
auto compute_strong_fiber_map(const G & g, const arc_map_t<G, int> & length_map,
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
    static constexpr std::size_t nb_vertices = 5;
    static constexpr double density = 0.35;
    static constexpr int nb_tests = 100000;

    static constexpr int min_length = 0;
    static constexpr int max_length = 10;
    std::uniform_int_distribution lower_distr{min_length, max_length};
    std::mt19937 engine{std::random_device{}()};

    for(int i = 0; i < nb_tests; ++i) {
        auto graph = erdos_renyi<static_digraph>(nb_vertices, density);
        auto lower_length_map = create_arc_map<int>(graph);
        auto upper_length_map = create_arc_map<int>(graph);
        for(const auto & a : arcs(graph)) {
            lower_length_map[a] = lower_distr(engine);
            upper_length_map[a] = std::uniform_int_distribution{
                lower_length_map[a], max_length}(engine);
        }

        for(const auto & a : arcs(graph))
            ASSERT_TRUE(lower_length_map[a] <= upper_length_map[a]);

        for(const auto & uv : arcs(graph)) {
            auto strong_map = create_vertex_map<bool>(graph, false);
            auto certificat_length_map = lower_length_map;

            strong_fiber strong_fiber_algo(graph, lower_length_map,
                                           upper_length_map);

            strong_fiber_algo.reset().add_strong_arc_source(uv);

            certificat_length_map[uv] = upper_length_map[uv];
            for(const auto & [t, t_dist] : strong_fiber_algo) {
                strong_map[t] = true;
                for(const auto & a : out_arcs(graph, t)) {
                    certificat_length_map[a] = upper_length_map[a];
                }
            }

            auto certificate_fiber_map =
                compute_strong_fiber_map(graph, certificat_length_map, uv);

            auto strong_fiber_view = std::views::filter(
                vertices(graph), [&](auto && v) { return strong_map[v]; });
            auto certificate_fiber_view = std::views::filter(
                vertices(graph),
                [&](auto && v) { return certificate_fiber_map[v]; });

            // std::cout << '(' << uv << ",(" << arc_source(graph, uv) << ','
            //           << arc_target(graph, uv) << "))" << std::endl;

            if(!EQ_MULTISETS(strong_fiber_view, certificate_fiber_view)) {
                std::cout << "shit!" << std::endl;
                graphviz_printer printer(graph);
                printer
                    .set_vertex_color_map(views::map(
                        [&](auto && v)
                            -> std::tuple<unsigned char, unsigned char,
                                          unsigned char> {
                            if(strong_map[v] && certificate_fiber_map[v])
                                return {255, 0, 0};
                            else if(certificate_fiber_map[v])
                                return {255, 0, 255};
                            else if(strong_map[v])
                                return {0, 0, 0};
                            else
                                return {255, 255, 255};
                        }))
                    .set_arc_color_map(views::map(
                        [&](auto && a)
                            -> std::tuple<unsigned char, unsigned char,
                                          unsigned char> {
                            if(a == uv)
                                return {255, 0, 0};
                            else if(certificat_length_map[a] ==
                                    upper_length_map[a])
                                return {0, 0, 255};
                            else
                                return {0, 0, 0};
                        }))
                    .set_arc_label_map(views::map([&](auto && a) {
                        return "[" + std::to_string(lower_length_map[a]) +
                        ',' +
                               std::to_string(upper_length_map[a]) + "]";
                    }));
                printer.print(std::ostream_iterator<char>(std::cout));
            }

            ASSERT_TRUE(
                EQ_MULTISETS(strong_fiber_view, certificate_fiber_view));
        }
    }
}

template <graph G>
auto compute_useless_fiber_map(const G & g, const arc_map_t<G, int> & length_map,
                       const arc_t<G> & uv) {
    const auto & u = arc_source(g, uv);
    const auto & v = arc_target(g, uv);

    auto dist_from_v_map =
        create_vertex_map<int>(g, std::numeric_limits<int>::max());
    for(const auto & [t, t_dist_from_v] : dijkstra(g, length_map, v))
        dist_from_v_map[t] = t_dist_from_v;

    auto fiber_map = create_vertex_map<bool>(g, true);
    for(const auto & [t, t_dist_from_u] : dijkstra(g, length_map, u)) {
        if(dist_from_v_map[t] == std::numeric_limits<int>::max()) {
            fiber_map[t] = true;
            continue;
        }
        fiber_map[t] = t_dist_from_u < length_map[uv] + dist_from_v_map[t];
    }

    return fiber_map;
}

struct useless_fiber_traits {
    using semiring = shortest_path_semiring<int>;
    template <typename CMP = std::less<std::pair<vertex_t<static_digraph>, int>>>
    using heap =
        d_ary_heap<2, vertex_t<static_digraph>, int, CMP, vertex_map_t<static_digraph, std::size_t>>;

    static constexpr bool strictly_strong = true;
    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};

GTEST_TEST(useless_fiber, fuzzy) {
    static constexpr std::size_t nb_vertices = 5;
    static constexpr double density = 0.35;
    static constexpr int nb_tests = 10000;

    static constexpr int min_length = 1;
    static constexpr int max_length = 10;
    std::uniform_int_distribution lower_distr{min_length, max_length};
    std::mt19937 engine{std::random_device{}()};

    for(int i = 0; i < nb_tests; ++i) {
        auto graph = erdos_renyi<static_digraph>(nb_vertices, density);
        auto lower_length_map = create_arc_map<int>(graph);
        auto upper_length_map = create_arc_map<int>(graph);
        for(const auto & a : arcs(graph)) {
            lower_length_map[a] = lower_distr(engine);
            upper_length_map[a] = std::uniform_int_distribution{
                lower_length_map[a], max_length}(engine);
        }

        for(const auto & a : arcs(graph))
            ASSERT_TRUE(lower_length_map[a] <= upper_length_map[a]);

        for(const auto & uv : arcs(graph)) {
            auto u = arc_source(graph, uv);
            auto useless_map = create_vertex_map<bool>(graph, true);
            for(auto v : breadth_first_search(graph, u)) 
                useless_map[v] = false;
            useless_map[u] = true;

            strong_fiber<decltype(graph), decltype(lower_length_map), decltype(upper_length_map), useless_fiber_traits> strong_fiber_algo(graph, lower_length_map,
                                           upper_length_map);

            strong_fiber_algo.reset().add_useless_arc_source(uv);

            auto certificat_length_map = lower_length_map;
            certificat_length_map[uv] = lower_length_map[uv];
            for(const auto a : out_arcs(graph, u)) {
                if(a == uv) continue;
                certificat_length_map[a] = upper_length_map[a];
            }
            for(const auto & [t, t_dist] : strong_fiber_algo) {
                useless_map[t] = true;
                for(const auto & a : out_arcs(graph, t)) {
                    certificat_length_map[a] = upper_length_map[a];
                }
            }

            auto certificate_fiber_map =
                compute_useless_fiber_map(graph, certificat_length_map, uv);

            auto strong_fiber_view = std::views::filter(
                vertices(graph), [&](auto && v) { return useless_map[v]; });
            auto certificate_fiber_view = std::views::filter(
                vertices(graph),
                [&](auto && v) { return certificate_fiber_map[v]; });

            // std::cout << '(' << uv << ",(" << arc_source(graph, uv) << ','
            //           << arc_target(graph, uv) << "))" << std::endl;

            if(!EQ_MULTISETS(strong_fiber_view, certificate_fiber_view)) {
                std::cout << "shit!" << std::endl;
                graphviz_printer printer(graph);
                printer
                    .set_vertex_color_map(views::map(
                        [&](auto && v)
                            -> std::tuple<unsigned char, unsigned char,
                                          unsigned char> {
                            if(useless_map[v] && certificate_fiber_map[v])
                                return {255, 0, 0};
                            else if(certificate_fiber_map[v])
                                return {255, 0, 255};
                            else if(useless_map[v])
                                return {0, 0, 0};
                            else
                                return {255, 255, 255};
                        }))
                    .set_arc_color_map(views::map(
                        [&](auto && a)
                            -> std::tuple<unsigned char, unsigned char,
                                          unsigned char> {
                            if(a == uv)
                                return {255, 0, 0};
                            else if(certificat_length_map[a] ==
                                    upper_length_map[a])
                                return {0, 0, 255};
                            else
                                return {0, 0, 0};
                        }))
                    .set_arc_label_map(views::map([&](auto && a) {
                        return "[" + std::to_string(lower_length_map[a]) +
                        ',' +
                               std::to_string(upper_length_map[a]) + "]";
                    }));
                printer.print(std::ostream_iterator<char>(std::cout));
            } else {
                // std::cout << "ok!" << std::endl;
            }

            ASSERT_TRUE(
                EQ_MULTISETS(strong_fiber_view, certificate_fiber_view));
        }
    }
}
