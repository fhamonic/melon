#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/algorithm/competing_dijkstras.hpp"
#include "melon/algorithm/dijkstra.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/erdos_renyi.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/subgraph.hpp"

#include "melon/utility/graphviz_printer.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(competing_dijkstras, test) {
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

    auto sgraph = views::subgraph(
        graph, {},
        [](const arc_t<static_digraph> & a) -> bool { return a != 1; });
    competing_dijkstras algo(sgraph, length_map, reduced_length_map);
    algo.add_blue_source(6, length_map[1]);
    algo.add_red_source(0);

    std::vector<vertex_t<static_digraph>> strong_nodes;
    for(const auto & [u, u_dist] : algo) {
        strong_nodes.push_back(u);
    }
    algo.reset();

    ASSERT_TRUE(EQ_MULTISETS(strong_nodes, {6, 5, 4}));
}

template <graph G>
auto compute_competing_dijkstras_map(const G & g,
                                     const arc_map_t<G, int> & length_map,
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

GTEST_TEST(competing_dijkstras, fuzzy) {
    static constexpr std::size_t num_vertices = 15;
    static constexpr double density = 0.35;
    static constexpr int num_tests = 1000;

    static constexpr int min_length = 0;
    static constexpr int max_length = 10;
    std::uniform_int_distribution lower_distr{min_length, max_length};
    std::mt19937 engine{std::random_device{}()};

    for(int i = 0; i < num_tests; ++i) {
        auto graph = erdos_renyi<static_digraph>(num_vertices, density);
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
            auto v = arc_target(graph, uv);
            auto strong_map = create_vertex_map<bool>(graph, false);
            auto certificat_length_map = lower_length_map;

            auto sgraph = views::subgraph(
                graph, {}, [uv](const arc_t<static_digraph> & a) -> bool {
                    return a != uv;
                });
            competing_dijkstras competing_dijkstras_algo(
                sgraph, upper_length_map, lower_length_map);
            competing_dijkstras_algo.add_red_source(u);
            competing_dijkstras_algo.add_blue_source(v, upper_length_map[uv]);

            certificat_length_map[uv] = upper_length_map[uv];
            for(const auto & [t, t_dist] : competing_dijkstras_algo) {
                strong_map[t] = true;
                for(const auto & a : out_arcs(graph, t)) {
                    certificat_length_map[a] = upper_length_map[a];
                }
            }

            auto certificate_fiber_map = compute_competing_dijkstras_map(
                graph, certificat_length_map, uv);

            auto competing_dijkstras_view = std::views::filter(
                vertices(graph), [&](auto && w) { return strong_map[w]; });
            auto certificate_fiber_view = std::views::filter(
                vertices(graph),
                [&](auto && w) { return certificate_fiber_map[w]; });

            // std::cout << '(' << uv << ",(" << arc_source(graph, uv) << ','
            //           << arc_target(graph, uv) << "))" << std::endl;

            if(!EQ_MULTISETS(competing_dijkstras_view,
                             certificate_fiber_view)) {
                std::cout << "shit!" << std::endl;
                graphviz_printer printer(graph);
                printer
                    .set_vertex_color_map(views::map(
                        [&](auto && w)
                            -> std::tuple<unsigned char, unsigned char,
                                          unsigned char> {
                            if(strong_map[w] && certificate_fiber_map[w])
                                return {255, 0, 0};
                            else if(certificate_fiber_map[w])
                                return {255, 0, 255};
                            else if(strong_map[w])
                                return {64, 64, 64};
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
                                return {64, 64, 64};
                        }))
                    .set_arc_label_map(views::map([&](auto && a) {
                        return (std::ostringstream{}
                                << "[" << std::to_string(lower_length_map[a])
                                << ',' << std::to_string(upper_length_map[a])
                                << "]")
                            .str();
                    }));
                printer.print(std::ostream_iterator<char>(std::cout));
            }

            ASSERT_TRUE(
                EQ_MULTISETS(competing_dijkstras_view, certificate_fiber_view));
        }
    }
}

template <graph G>
auto compute_useless_fiber_map(const G & g,
                               const arc_map_t<G, int> & length_map,
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
    fiber_map[u] = true;

    return fiber_map;
}

template <outward_incidence_graph _Graph, typename _ValueType>
struct useless_competing_dijkstras_traits {
    using semiring = shortest_path_semiring<_ValueType>;
    using entry = std::pair<_ValueType, bool>;
    static bool compare_entries(const entry & e1, const entry & e2) {
        if(e1.first == e2.first) {
            return e2.second && !e1.second;
        }
        return semiring::less(e1.first, e2.first);
    }
    struct entry_cmp {
        [[nodiscard]] constexpr bool operator()(
            const auto & e1, const auto & e2) const noexcept {
            return compare_entries(e1, e2);
        }
    };
    using heap = updatable_d_ary_heap<2, std::pair<vertex_t<_Graph>, entry>, entry_cmp,
                            vertex_map_t<_Graph, std::size_t>,
                            views::element_map<1>, views::element_map<0>>;

    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};

GTEST_TEST(useless_fiber, fuzzy) {
    static constexpr std::size_t num_vertices = 15;
    static constexpr double density = 0.35;
    static constexpr int num_tests = 1000;

    static constexpr int min_length = 0;
    static constexpr int max_length = 10;
    std::uniform_int_distribution lower_distr{min_length, max_length};
    std::mt19937 engine{std::random_device{}()};

    for(int i = 0; i < num_tests; ++i) {
        auto graph = erdos_renyi<static_digraph>(num_vertices, density);
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
            auto v = arc_target(graph, uv);
            auto useless_map = create_vertex_map<bool>(graph, true);

            for(auto w : breadth_first_search(graph, u)) useless_map[w] = false;
            useless_map[u] = true;

            auto sgraph = views::subgraph(
                graph, {}, [&](const arc_t<static_digraph> & a) -> bool {
                    return a != uv;
                });
            auto competing_dijkstras_algo = competing_dijkstras(
                useless_competing_dijkstras_traits<decltype(sgraph), int>{},
                sgraph, upper_length_map, lower_length_map);
            competing_dijkstras_algo.add_blue_source(u);
            competing_dijkstras_algo.add_red_source(v, lower_length_map[uv]);

            auto certificat_length_map = lower_length_map;
            for(const auto a : out_arcs(graph, u)) {
                if(a == uv) continue;
                certificat_length_map[a] = upper_length_map[a];
            }

            // std::cout << "run\n";
            for(const auto & [t, t_dist] : competing_dijkstras_algo) {
                useless_map[t] = true;
                for(const auto & a : out_arcs(graph, t)) {
                    certificat_length_map[a] = upper_length_map[a];
                }
                // std::cout << t << " " << t_dist << std::endl;
            }
            certificat_length_map[uv] = lower_length_map[uv];

            auto certificate_fiber_map =
                compute_useless_fiber_map(graph, certificat_length_map, uv);

            auto competing_dijkstras_view = std::views::filter(
                vertices(graph), [&](auto && w) { return useless_map[w]; });
            auto certificate_fiber_view = std::views::filter(
                vertices(graph),
                [&](auto && w) { return certificate_fiber_map[w]; });

            // std::cout << '(' << uv << ",(" << arc_source(graph, uv) << ','
            //           << arc_target(graph, uv) << "))" << std::endl;

            if(!EQ_MULTISETS(competing_dijkstras_view,
                             certificate_fiber_view)) {
                std::cout << "shit!" << std::endl;
                graphviz_printer printer(graph);
                printer
                    .set_vertex_color_map(views::map(
                        [&](auto && w)
                            -> std::tuple<unsigned char, unsigned char,
                                          unsigned char> {
                            if(useless_map[w] && certificate_fiber_map[w])
                                return {255, 0, 0};
                            else if(certificate_fiber_map[w])
                                return {255, 0, 255};
                            else if(useless_map[w])
                                return {64, 64, 64};
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
                                return {64, 64, 64};
                        }))
                    .set_arc_label_map(views::map([&](auto && a) {
                        return (std::ostringstream{}
                                << "[" << std::to_string(lower_length_map[a])
                                << ',' << std::to_string(upper_length_map[a])
                                << "]")
                            .str();
                    }));
                printer.print(std::ostream_iterator<char>(std::cout));
            } else {
                // std::cout << "ok!" << std::endl;
            }

            ASSERT_TRUE(
                EQ_MULTISETS(competing_dijkstras_view, certificate_fiber_view));
        }
    }
}
