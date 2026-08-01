#undef NDEBUG
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <vector>

#include "melon/algorithm/bidirectional_dijkstra.hpp"
#include "melon/algorithm/dijkstra.hpp"
#include "melon/algorithm/dinitz.hpp"
#include "melon/algorithm/edmonds_karp.hpp"
#include "melon/algorithm/network_voronoi.hpp"
#include "melon/algorithm/strongly_connected_components.hpp"
#include "melon/algorithm/topological_sort.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "random_ranges_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// Every algorithm below is checked against an independent reference on random
// graphs, rather than against hand-written expected values on one fixed graph.
//
// Keep the graphs tiny -- at most a dozen vertices. The reference is cubic,
// and small dense graphs hit the degeneracies that matter (parallel arcs,
// self-loops, ties, disconnected parts) far more often than large sparse ones,
// so raising the size trades coverage for runtime.
//
// A failure here names a seed: random_ranges_helper.hpp prints it and
// MELON_TEST_SEED replays it.
////////////////////////////////////////////////////////////////////////////////

namespace {

using vertex = vertex_t<static_digraph>;
using G = views::graph_all_t<static_digraph &>;

constexpr int INF = 1 << 20;
constexpr std::size_t NUM_INSTANCES = 300;

struct instance {
    static_digraph graph;
    std::vector<int> length_map;
    // all-pairs shortest distances, Floyd-Warshall over the same lengths
    std::vector<std::vector<int>> dist;
    std::size_t n;
};

// Dense enough that most instances are strongly connected somewhere and
// degenerate somewhere else: parallel arcs and self-loops are deliberately not
// filtered out, both being things a graph library has to survive.
instance random_instance() {
    const std::size_t n = 2 + (test_rng()() % 11);
    const std::size_t m = test_rng()() % (2 * n + 4);

    static_digraph_builder<static_digraph, int> builder(n);
    for(std::size_t k = 0; k < m; ++k) {
        const auto u = static_cast<vertex>(test_rng()() % n);
        const auto v = static_cast<vertex>(test_rng()() % n);
        builder.add_arc(u, v, static_cast<int>(1 + test_rng()() % 9));
    }
    auto [graph, length_map] = builder.build();

    std::vector<std::vector<int>> dist(n, std::vector<int>(n, INF));
    for(std::size_t v = 0; v < n; ++v) dist[v][v] = 0;
    for(auto && [a, endpoints] : arcs_entries(graph))
        dist[endpoints.first][endpoints.second] =
            std::min(dist[endpoints.first][endpoints.second], length_map[a]);
    for(std::size_t k = 0; k < n; ++k)
        for(std::size_t i = 0; i < n; ++i)
            for(std::size_t j = 0; j < n; ++j)
                dist[i][j] = std::min(dist[i][j], dist[i][k] + dist[k][j]);

    return instance{std::move(graph), std::move(length_map), std::move(dist),
                    n};
}

struct shortest_path_traits : dijkstra_default_traits<G, int> {
    static constexpr bool store_distances = true;
    static constexpr bool store_paths = true;
};
struct voronoi_traits : network_voronoi_default_traits<G, int> {
    static constexpr bool store_distances = true;
    static constexpr bool store_clusters = true;
};
struct scc_traits : strongly_connected_components_default_traits {
    static constexpr bool store_component_ids = true;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// shortest paths
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(differential, dijkstra_matches_floyd_warshall) {
    for(std::size_t it = 0; it < NUM_INSTANCES; ++it) {
        auto in = random_instance();
        const auto s = static_cast<vertex>(test_rng()() % in.n);

        auto alg = dijkstra(shortest_path_traits{}, in.graph, in.length_map, s);
        alg.run();

        for(std::size_t v = 0; v < in.n; ++v) {
            const bool reachable = in.dist[s][v] < INF;
            ASSERT_EQ(alg.reached(static_cast<vertex>(v)), reachable)
                << "vertex " << v << " from " << s;
            if(!reachable) continue;
            ASSERT_EQ(alg.dist(static_cast<vertex>(v)), in.dist[s][v])
                << "vertex " << v << " from " << s;

            // the stored path must be a real walk of exactly that length
            int walked = 0;
            for(auto && a : alg.path_to(static_cast<vertex>(v)))
                walked += in.length_map[a];
            ASSERT_EQ(walked, in.dist[s][v]) << "path to " << v;
        }
    }
}

GTEST_TEST(differential, bidirectional_dijkstra_matches_floyd_warshall) {
    for(std::size_t it = 0; it < NUM_INSTANCES; ++it) {
        auto in = random_instance();
        const auto s = static_cast<vertex>(test_rng()() % in.n);
        const auto t = static_cast<vertex>(test_rng()() % in.n);
        if(s == t) continue;

        bidirectional_dijkstra alg(in.graph, in.length_map, s, t);
        alg.run();

        const bool reachable = in.dist[s][t] < INF;
        ASSERT_EQ(alg.path_found(), reachable) << s << " -> " << t;
        if(!reachable) continue;

        ASSERT_EQ(alg.dist(), in.dist[s][t]) << s << " -> " << t;
        int walked = 0;
        for(auto && a : alg.path()) walked += in.length_map[a];
        ASSERT_EQ(walked, in.dist[s][t]) << s << " -> " << t;
    }
}

GTEST_TEST(differential, network_voronoi_assigns_the_nearest_kernel) {
    for(std::size_t it = 0; it < NUM_INSTANCES; ++it) {
        auto in = random_instance();

        std::vector<vertex> kernels;
        for(std::size_t v = 0; v < in.n; ++v)
            if(test_rng()() % 3 == 0) kernels.push_back(static_cast<vertex>(v));
        if(kernels.empty()) continue;

        network_voronoi alg(voronoi_traits{}, in.graph, in.length_map);
        alg.set_kernels(kernels).run();

        for(std::size_t v = 0; v < in.n; ++v) {
            int nearest = INF;
            for(const auto k : kernels)
                nearest = std::min(nearest, in.dist[k][v]);

            const auto u = static_cast<vertex>(v);
            ASSERT_EQ(alg.reached(u), nearest < INF) << "vertex " << v;
            if(nearest == INF) continue;
            ASSERT_EQ(alg.dist(u), nearest) << "vertex " << v;
            // the cell it landed in must be one of the nearest kernels, which
            // is weaker than naming one -- ties are broken by kernel id
            ASSERT_EQ(in.dist[alg.cluster(u)][v], nearest) << "vertex " << v;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// flows
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(differential, dinitz_and_edmonds_karp_agree_on_a_valid_flow) {
    for(std::size_t it = 0; it < NUM_INSTANCES; ++it) {
        auto in = random_instance();
        const auto s = static_cast<vertex>(test_rng()() % in.n);
        const auto t = static_cast<vertex>(test_rng()() % in.n);
        if(s == t) continue;

        auto ek = edmonds_karp(in.graph, in.length_map, s, t);
        auto dz = dinitz(in.graph, in.length_map, s, t);
        ek.run();
        dz.run();

        ASSERT_EQ(ek.flow_value(), dz.flow_value()) << s << " -> " << t;

        // ... and what dinitz reports really is a flow
        std::vector<long> balance(in.n, 0);
        for(auto && [a, endpoints] : arcs_entries(in.graph)) {
            const int f = dz.flow(a);
            ASSERT_GE(f, 0) << "arc " << a;
            ASSERT_LE(f, in.length_map[a]) << "arc " << a;
            balance[endpoints.first] -= f;
            balance[endpoints.second] += f;
        }
        for(std::size_t v = 0; v < in.n; ++v) {
            if(v == s || v == t) continue;
            ASSERT_EQ(balance[v], 0) << "conservation at " << v;
        }
        ASSERT_EQ(balance[t], dz.flow_value());
    }
}

////////////////////////////////////////////////////////////////////////////////
// traversals
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(differential, strongly_connected_components_match_reachability) {
    for(std::size_t it = 0; it < NUM_INSTANCES; ++it) {
        auto in = random_instance();

        auto alg = strongly_connected_components(scc_traits{}, in.graph);
        alg.run();

        for(std::size_t u = 0; u < in.n; ++u) {
            for(std::size_t v = 0; v < in.n; ++v) {
                const bool same = alg.component_id(static_cast<vertex>(u)) ==
                                  alg.component_id(static_cast<vertex>(v));
                const bool mutually_reachable =
                    in.dist[u][v] < INF && in.dist[v][u] < INF;
                ASSERT_EQ(same, mutually_reachable) << u << " and " << v;
            }
        }

        // ids are emitted in reverse topological order of the condensation, so
        // no arc may point from a lower id to a higher one
        for(auto && [a, endpoints] : arcs_entries(in.graph))
            ASSERT_GE(alg.component_id(endpoints.first),
                      alg.component_id(endpoints.second))
                << "arc " << a;
    }
}

GTEST_TEST(differential, topological_sort_orders_every_arc_it_reaches) {
    for(std::size_t it = 0; it < NUM_INSTANCES; ++it) {
        auto in = random_instance();

        auto alg = topological_sort(in.graph);
        std::vector<int> position(in.n, -1);
        int next = 0;
        for(auto && v : alg) position[v] = next++;

        // Only arcs between two *ordered* vertices are constrained: a vertex
        // sitting behind a cycle keeps a positive remaining in-degree and is
        // never emitted, so an ordered tail does not force an ordered head.
        for(auto && [a, endpoints] : arcs_entries(in.graph)) {
            if(position[endpoints.first] < 0 || position[endpoints.second] < 0)
                continue;
            ASSERT_LT(position[endpoints.first], position[endpoints.second])
                << "arc " << a;
        }

        // a vertex goes unordered exactly when a cycle holds it back, which is
        // what is_acyclic() reports -- cross-checked against the condensation
        auto scc = strongly_connected_components(scc_traits{}, in.graph);
        scc.run();
        std::vector<int> component_size(in.n, 0);
        for(std::size_t v = 0; v < in.n; ++v)
            ++component_size[scc.component_id(static_cast<vertex>(v))];
        bool acyclic = true;
        for(std::size_t v = 0; v < in.n; ++v)
            if(component_size[scc.component_id(static_cast<vertex>(v))] > 1)
                acyclic = false;
        for(auto && [a, endpoints] : arcs_entries(in.graph))
            if(endpoints.first == endpoints.second) acyclic = false;

        ASSERT_EQ(alg.is_acyclic(), acyclic);
        ASSERT_EQ(static_cast<std::size_t>(next) == in.n, acyclic);
    }
}
