#undef NDEBUG
#include <gtest/gtest.h>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <limits>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/algorithm/network_simplex.hpp"
#include "melon/borrowed_graph.hpp"
#include "melon/container/mutable_digraph.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/complete_digraph.hpp"

#include "arc_list_digraph.hpp"
#include "random_ranges_helper.hpp"

using namespace melon;

namespace {

// The optimality certificate the header promises: with status() == optimal,
// every arc satisfies complementary slackness against the returned
// potentials. Checking it instead of exact potential values keeps the tests
// valid across any optimal basis the pivot order lands on.
template <typename Alg, typename Graph, typename UpperMap, typename CostMap>
void check_dual_certificate(const Alg & alg, const Graph & graph,
                            const UpperMap & upper, const CostMap & cost) {
    for(auto && a : arcs(graph)) {
        const auto reduced_cost = cost[a] +
                                  alg.potential(arc_source(graph, a)) -
                                  alg.potential(arc_target(graph, a));
        if(reduced_cost > 0) {
            ASSERT_EQ(alg.flow(a), 0);
        }
        if(reduced_cost < 0) {
            ASSERT_EQ(alg.flow(a), upper[a]);
        }
    }
}

template <typename Alg, typename Graph, typename UpperMap, typename SupplyMap>
void check_flow_is_feasible(const Alg & alg, const Graph & graph,
                            const UpperMap & upper, const SupplyMap & supply) {
    for(auto && a : arcs(graph)) {
        ASSERT_GE(alg.flow(a), 0);
        ASSERT_LE(alg.flow(a), upper[a]);
    }
    for(auto && v : vertices(graph)) {
        int excess = supply[v];
        for(auto && a : arcs(graph)) {
            if(arc_source(graph, a) == v) excess -= alg.flow(a);
            if(arc_target(graph, a) == v) excess += alg.flow(a);
        }
        ASSERT_EQ(excess, 0);
    }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// network_simplex computes a minimum-cost flow, its cost, and a dual
// certificate on a hand-checked transportation instance -- on static_digraph
// through the zero-copy endpoint path, arc_source/arc_target being answered
// from the graph's own arrays
////////////////////////////////////////////////////////////////////////////////

static_assert(has_arc_source<static_digraph> && has_arc_target<static_digraph>);

GTEST_TEST(network_simplex, fixed_transportation_instance) {
    // ship 4 units from vertex 0 to vertex 3; the two cost-3 routes
    // (0>1>2>3 and 0>2>3) each saturate at 2 units, so the optimum of 12
    // and its flow vector are unique.
    static_digraph_builder<static_digraph, int, int, int> builder(4);
    builder.add_arc({0u, 1u}, 4, 1, 2);
    builder.add_arc({0u, 2u}, 2, 2, 2);
    builder.add_arc({1u, 2u}, 2, 1, 2);
    builder.add_arc({1u, 3u}, 3, 3, 0);
    builder.add_arc({2u, 3u}, 5, 1, 4);
    auto [graph, upper, cost, expected_flow] = builder.build();
    std::vector<int> supply = {4, 0, 0, -4};

    network_simplex alg(graph, upper, cost, supply);
    alg.run();
    ASSERT_EQ(alg.status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 12);
    for(auto && a : arcs(graph)) ASSERT_EQ(alg.flow(a), expected_flow[a]);
    check_flow_is_feasible(alg, graph, upper, supply);
    check_dual_certificate(alg, graph, upper, cost);

    // the map views agree with the per-element accessors
    for(auto && a : arcs(graph)) ASSERT_EQ(alg.flows_map()[a], alg.flow(a));
    for(auto && v : vertices(graph))
        ASSERT_EQ(alg.potentials_map()[v], alg.potential(v));
}

// The algorithm only ever reads the graph and the capacity and cost mappings,
// so const references model all three. A running test rather than a
// constructible_from assert, which would leave the pivot loop uninstantiated.
GTEST_TEST(network_simplex, const_graph_and_const_mappings) {
    static_digraph_builder<static_digraph, int, int> builder(4);
    builder.add_arc({0u, 1u}, 4, 1);
    builder.add_arc({0u, 2u}, 2, 2);
    builder.add_arc({1u, 2u}, 2, 1);
    builder.add_arc({1u, 3u}, 3, 3);
    builder.add_arc({2u, 3u}, 5, 1);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {4, 0, 0, -4};

    const static_digraph & const_graph = graph;
    const std::vector<int> & const_upper = upper;
    const std::vector<int> & const_cost = cost;
    network_simplex alg(const_graph, const_upper, const_cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 12);
}

namespace {
using ref_graph = views::graph_all_t<static_digraph &>;
using ref_ns =
    network_simplex<ref_graph, maps::mapping_all_t<std::vector<int> &>,
                    maps::mapping_all_t<std::vector<int> &>,
                    maps::mapping_all_t<std::vector<int> &>>;
}  // namespace

// Where the bulk accessors are *called* nothing can throw: the const& pair
// build a closure over `this` and nothing else, and the terminal pair only
// move the map they hand over. That move is what keeps the terminal pair
// conditional -- an unconditional noexcept would turn a throwing map into
// std::terminate.
static_assert(noexcept(std::declval<const ref_ns &>().flows_map()));
static_assert(noexcept(std::declval<const ref_ns &>().potentials_map()));
static_assert(noexcept(std::declval<ref_ns>().flows_map()) ==
              std::is_nothrow_move_constructible_v<arc_map_t<ref_graph, int>>);
static_assert(
    noexcept(std::declval<ref_ns>().potentials_map()) ==
    std::is_nothrow_move_constructible_v<vertex_map_t<ref_graph, int>>);

GTEST_TEST(network_simplex, terminal_maps_survive_the_algorithm) {
    static_digraph_builder<static_digraph, int, int, int> builder(4);
    builder.add_arc({0u, 1u}, 4, 1, 2);
    builder.add_arc({0u, 2u}, 2, 2, 2);
    builder.add_arc({1u, 2u}, 2, 1, 2);
    builder.add_arc({1u, 3u}, 3, 3, 0);
    builder.add_arc({2u, 3u}, 5, 1, 4);
    auto [graph, upper, cost, expected_flow] = builder.build();
    std::vector<int> supply = {4, 0, 0, -4};

    // rvalue flows_map()/potentials_map() are terminal: they take the
    // algorithm's internal storage with them, so they must keep answering
    // after the algorithm object is gone
    auto owned_flows = [&] {
        network_simplex alg(graph, upper, cost, supply);
        return std::move(alg.run()).flows_map();
    }();
    for(auto && a : arcs(graph)) ASSERT_EQ(owned_flows[a], expected_flow[a]);

    auto owned_potentials = [&] {
        network_simplex alg(graph, upper, cost, supply);
        return std::move(alg.run()).potentials_map();
    }();
    // the potentials are basis-dependent, so only certify with them
    for(auto && a : arcs(graph)) {
        const int reduced_cost = cost[a] +
                                 owned_potentials[arc_source(graph, a)] -
                                 owned_potentials[arc_target(graph, a)];
        if(reduced_cost > 0) {
            ASSERT_EQ(expected_flow[a], 0);
        }
        if(reduced_cost < 0) {
            ASSERT_EQ(expected_flow[a], upper[a]);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// the steppable interface reaches the same optimum one pivot at a time, and
// reset() re-reads the live maps -- there is no internal copy to go stale
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(network_simplex, steppable_advance_matches_run) {
    static_digraph_builder<static_digraph, int, int> builder(4);
    builder.add_arc({0u, 1u}, 4, 1);
    builder.add_arc({0u, 2u}, 2, 2);
    builder.add_arc({1u, 2u}, 2, 1);
    builder.add_arc({1u, 3u}, 3, 3);
    builder.add_arc({2u, 3u}, 5, 1);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {4, 0, 0, -4};

    network_simplex alg(graph, upper, cost, supply);
    static_assert(requires(const decltype(alg) & const_alg) {
        { const_alg.finished() } -> std::same_as<bool>;
    });
    int num_pivots = 0;
    while(!alg.finished()) {
        alg.advance();
        ++num_pivots;
    }
    ASSERT_GT(num_pivots, 0);
    ASSERT_EQ(alg.status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 12);
}

GTEST_TEST(network_simplex, reset_rereads_the_maps) {
    static_digraph_builder<static_digraph, int, int> builder(3);
    builder.add_arc({0u, 1u}, 5, 2);
    builder.add_arc({1u, 2u}, 5, 3);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {3, 0, -3};

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().total_cost(), 15);

    // the algorithm holds reference views on the lvalue maps, so mutating
    // them between runs and calling reset() must re-solve the new problem
    supply = {1, 0, -1};
    ASSERT_EQ(alg.reset().run().total_cost(), 5);

    // costs too: they are read live by the pivot search, not snapshotted
    cost[0u] = 4;
    ASSERT_EQ(alg.reset().run().total_cost(), 7);
}

////////////////////////////////////////////////////////////////////////////////
// infeasible and unbounded instances are reported as such, not as silently
// wrong optima
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(network_simplex, infeasible_when_capacity_is_insufficient) {
    static_digraph_builder<static_digraph, int, int> builder(2);
    builder.add_arc({0u, 1u}, 2, 1);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {4, -4};

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::infeasible);
}

GTEST_TEST(network_simplex, unbounded_on_a_negative_uncapacitated_cycle) {
    constexpr int INF = std::numeric_limits<int>::max();
    static_digraph_builder<static_digraph, int, int> builder(2);
    builder.add_arc({0u, 1u}, INF, -1);
    builder.add_arc({1u, 0u}, INF, -1);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {0, 0};

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::unbounded);
}

////////////////////////////////////////////////////////////////////////////////
// zero supplies do not mean zero flow: a negative-cost cycle must be
// saturated, and a capacitated one must saturate at its bottleneck
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(network_simplex, negative_cycle_circulation_saturates) {
    static_digraph_builder<static_digraph, int, int> builder(3);
    builder.add_arc({0u, 1u}, 5, -2);
    builder.add_arc({1u, 2u}, 5, 1);
    builder.add_arc({2u, 0u}, 5, 0);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {0, 0, 0};

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), -5);
    for(auto && a : arcs(graph)) ASSERT_EQ(alg.flow(a), 5);
}

GTEST_TEST(network_simplex, negative_self_loop_saturates) {
    static_digraph_builder<static_digraph, int, int> builder(1);
    builder.add_arc({0u, 0u}, 7, -3);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {0};

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), -21);

    // the same loop with an infinite capacity is an unbounded ray
    static_digraph_builder<static_digraph, int, int> builder2(1);
    builder2.add_arc({0u, 0u}, std::numeric_limits<int>::max(), -3);
    auto [graph2, upper2, cost2] = builder2.build();
    network_simplex alg2(graph2, upper2, cost2, supply);
    ASSERT_EQ(alg2.run().status(), mcf_status::unbounded);
}

GTEST_TEST(network_simplex, zero_supplies_and_positive_costs_stay_at_zero) {
    static_digraph_builder<static_digraph, int, int> builder(3);
    builder.add_arc({0u, 1u}, 5, 1);
    builder.add_arc({1u, 2u}, 5, 1);
    builder.add_arc({2u, 0u}, 5, 1);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {0, 0, 0};

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 0);
    for(auto && a : arcs(graph)) ASSERT_EQ(alg.flow(a), 0);
}

////////////////////////////////////////////////////////////////////////////////
// a capacity equal to numeric_limits<value>::max() is +infinity for routing
// purposes, and parallel arcs are filled cheapest-first
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(network_simplex, max_capacity_is_infinity) {
    static_digraph_builder<static_digraph, int, int> builder(2);
    builder.add_arc({0u, 1u}, std::numeric_limits<int>::max(), 2);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {3, -3};

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 6);
}

GTEST_TEST(network_simplex, parallel_arcs_fill_cheapest_first) {
    static_digraph_builder<static_digraph, int, int, int> builder(2);
    builder.add_arc({0u, 1u}, 3, 5, 1);
    builder.add_arc({0u, 1u}, 3, 1, 3);
    auto [graph, upper, cost, expected_flow] = builder.build();
    std::vector<int> supply = {4, -4};

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 8);
    for(auto && a : arcs(graph)) ASSERT_EQ(alg.flow(a), expected_flow[a]);
}

////////////////////////////////////////////////////////////////////////////////
// an assignment problem exercises non-trivial pivoting: several bases are
// optimal, so only the optimum value is pinned
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(network_simplex, three_by_three_assignment) {
    // cost matrix rows for left vertices 0..2 to right vertices 3..5;
    // the best permutations cost 12
    static_digraph_builder<static_digraph, int, int> builder(6);
    builder.add_arc({0u, 3u}, 1, 4);
    builder.add_arc({0u, 4u}, 1, 2);
    builder.add_arc({0u, 5u}, 1, 8);
    builder.add_arc({1u, 3u}, 1, 4);
    builder.add_arc({1u, 4u}, 1, 3);
    builder.add_arc({1u, 5u}, 1, 7);
    builder.add_arc({2u, 3u}, 1, 3);
    builder.add_arc({2u, 4u}, 1, 1);
    builder.add_arc({2u, 5u}, 1, 6);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {1, 1, 1, -1, -1, -1};

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 12);
    check_flow_is_feasible(alg, graph, upper, supply);
    check_dual_certificate(alg, graph, upper, cost);
}

////////////////////////////////////////////////////////////////////////////////
// degenerate networks are handled: no vertices, no arcs with balanced or
// unbalanceable supplies
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(network_simplex, empty_graph) {
    static_digraph_builder<static_digraph, int, int> builder(0);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {};

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 0);
}

GTEST_TEST(network_simplex, no_arcs) {
    static_digraph_builder<static_digraph, int, int> builder(2);
    auto [graph, upper, cost] = builder.build();

    std::vector<int> zero_supply = {0, 0};
    network_simplex balanced(graph, upper, cost, zero_supply);
    ASSERT_EQ(balanced.run().status(), mcf_status::optimal);
    ASSERT_EQ(balanced.total_cost(), 0);

    std::vector<int> moving_supply = {1, -1};
    network_simplex stuck(graph, upper, cost, moving_supply);
    ASSERT_EQ(stuck.run().status(), mcf_status::infeasible);
}

////////////////////////////////////////////////////////////////////////////////
// the algorithm also runs on a lazy graph view with lambda maps; the arc
// endpoints are then computed by the view on every read
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(network_simplex, complete_digraph_view_with_lambda_maps) {
    auto graph = views::complete_digraph<>(4ul);
    network_simplex alg(
        graph, [](const auto &) { return 1; }, [](const auto &) { return 1; },
        [](const auto & v) { return v == 0ul ? 3 : -1; });
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 3);
}

////////////////////////////////////////////////////////////////////////////////
// custom traits plug in through the leading-Traits constructor; a block size
// of one is the most aggressive exercise of the wrap-around entering-arc
// search
////////////////////////////////////////////////////////////////////////////////

namespace {
// first_eligible IS block_search<0.0, 1>, so this one config covers both
// the single-arc-block wrap stress and the first-eligible rule.
struct single_arc_block_traits {
    using pivot_rule = pivot_rules::first_eligible;
    static constexpr bool arc_mixing = false;
    using total_cost_type = long long;
};
struct best_eligible_traits {
    using pivot_rule = pivot_rules::best_eligible;
    static constexpr bool arc_mixing = true;
    using total_cost_type = long long;
};
}  // namespace
static_assert(network_simplex_traits<single_arc_block_traits>);
static_assert(network_simplex_traits<best_eligible_traits>);
static_assert(std::same_as<pivot_rules::first_eligible,
                           pivot_rules::block_search<0.0, 1>>);

// A rule the constructor accepts but reset() cannot reassign: the concepts
// must refuse it, or the class constraint says yes and the constructor body
// hard-errors.
namespace {
struct const_member_rule {
    const std::size_t block_size;
    explicit const_member_rule(std::size_t n) : block_size(n) {}
    template <typename Context>
    std::optional<typename Context::arc_type> find_entering_arc(Context &) {
        return std::nullopt;
    }
};
struct const_member_rule_traits {
    using pivot_rule = const_member_rule;
    [[maybe_unused]] static constexpr bool arc_mixing = false;
    using total_cost_type = long long;
};
struct probe_search_context {
    using arc_type = arc_t<static_digraph>;
};
template <typename Traits>
concept network_simplex_admits_traits =
    requires(Traits traits, static_digraph & g, std::vector<int> & arc_values,
             std::vector<int> & vertex_values) {
        network_simplex(traits, g, arc_values, arc_values, vertex_values);
    };
}  // namespace
static_assert(std::constructible_from<const_member_rule, std::size_t> &&
              !std::movable<const_member_rule>);
static_assert(
    !network_simplex_pivot_rule<const_member_rule, probe_search_context>);
static_assert(!network_simplex_traits<const_member_rule_traits>);
static_assert(network_simplex_admits_traits<single_arc_block_traits>);
static_assert(!network_simplex_admits_traits<const_member_rule_traits>);

GTEST_TEST(network_simplex, traits_plug_in_through_the_leading_constructor) {
    static_digraph_builder<static_digraph, int, int> builder(4);
    builder.add_arc({0u, 1u}, 4, 1);
    builder.add_arc({0u, 2u}, 2, 2);
    builder.add_arc({1u, 2u}, 2, 1);
    builder.add_arc({1u, 3u}, 3, 3);
    builder.add_arc({2u, 3u}, 5, 1);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {4, 0, 0, -4};

    network_simplex alg(single_arc_block_traits{}, graph, upper, cost, supply);
    static_assert(std::same_as<decltype(alg.total_cost()), long long>);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 12);
}

////////////////////////////////////////////////////////////////////////////////
// capacities and supplies share one value domain -- their common_type -- so
// mixed widths widen instead of truncating, and costs keep a domain of their
// own. Types the arithmetic would silently betray are rejected at the
// concept level: unsigned value or cost domains never see a negative reduced
// cost, so every feasible instance would be reported infeasible; a capacity
// type without a genuine numeric_limits specialization has a zero
// MAX-infinity
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(network_simplex, value_domain_is_the_capacity_supply_common_type) {
    static_digraph_builder<static_digraph, int, double> builder(3);
    builder.add_arc({0u, 1u}, 5, 1.5);
    builder.add_arc({1u, 2u}, 5, 2.0);
    auto [graph, upper, cost] = builder.build();
    std::vector<long long> supply = {3, 0, -3};

    network_simplex alg(graph, upper, cost, supply);
    static_assert(
        std::same_as<decltype(alg.flow(arc_t<static_digraph>{})), long long>);
    static_assert(std::same_as<decltype(alg.total_cost()), double>);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 10.5);
}

namespace admission_probes {
struct opaque_capacity {
    int v;
};
}  // namespace admission_probes

template <typename UpperV, typename CostV = int, typename SupplyV = int>
concept network_simplex_admits =
    requires(static_digraph & g, std::vector<UpperV> & upper_bounds,
             std::vector<CostV> & costs, std::vector<SupplyV> & supplies) {
        network_simplex(g, upper_bounds, costs, supplies);
    };
static_assert(network_simplex_admits<int>);
static_assert(network_simplex_admits<double>);
static_assert(network_simplex_admits<int, double>);
static_assert(network_simplex_admits<int, int, long long>);
static_assert(!network_simplex_admits<unsigned>);
static_assert(!network_simplex_admits<int, unsigned>);
static_assert(!network_simplex_admits<int, int, unsigned>);
static_assert(!network_simplex_admits<admission_probes::opaque_capacity>);

////////////////////////////////////////////////////////////////////////////////
// the graph contract, pinned from both sides: num_vertices/num_arcs and both
// map factories are required -- the bare arc list (vertex maps only) is
// rejected -- while an arc list WITH both factories runs through the
// endpoint-map branch, its endpoint maps created and filled from
// arcs_entries because it has no arc_source to delegate to. The judgment is
// per endpoint: a graph answering only arc_target gets only a sources map
// created. NEITHER id space carries an integrality requirement -- see
// opaque_digraph below
////////////////////////////////////////////////////////////////////////////////

namespace {
struct mappable_arc_list_digraph : arc_list_digraph {
    template <typename T>
    auto create_arc_map() const {
        return melon::static_map<unsigned int, T>(ends.size());
    }
    template <typename T>
    auto create_arc_map(const T & d) const {
        return melon::static_map<unsigned int, T>(ends.size(), d);
    }
};
struct target_only_arc_list_digraph : mappable_arc_list_digraph {
    auto arc_target(unsigned int a) const { return ends[a].second; }
};
}  // namespace
static_assert(!has_arc_source<mappable_arc_list_digraph>);
static_assert(!has_arc_source<target_only_arc_list_digraph> &&
              has_arc_target<target_only_arc_list_digraph>);

template <typename G>
concept network_simplex_admits_graph = requires(
    G & g, std::vector<int> & arc_values, std::vector<int> & vertex_values) {
    network_simplex(g, arc_values, arc_values, vertex_values);
};
static_assert(network_simplex_admits_graph<static_digraph>);
static_assert(network_simplex_admits_graph<mappable_arc_list_digraph>);
static_assert(!network_simplex_admits_graph<arc_list_digraph>);

GTEST_TEST(network_simplex, runs_on_an_arc_list_with_map_factories) {
    mappable_arc_list_digraph graph{
        {4u, {{0u, 1u}, {0u, 2u}, {1u, 2u}, {1u, 3u}, {2u, 3u}}}};
    std::vector<int> upper = {4, 2, 2, 3, 5};
    std::vector<int> cost = {1, 2, 1, 3, 1};
    std::vector<int> supply = {4, 0, 0, -4};

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 12);
}

GTEST_TEST(network_simplex, runs_with_only_one_delegated_endpoint) {
    target_only_arc_list_digraph graph{
        {{4u, {{0u, 1u}, {0u, 2u}, {1u, 2u}, {1u, 3u}, {2u, 3u}}}}};
    std::vector<int> upper = {4, 2, 2, 3, 5};
    std::vector<int> cost = {1, 2, 1, 3, 1};
    std::vector<int> supply = {4, 0, 0, -4};

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 12);
}

////////////////////////////////////////////////////////////////////////////////
// neither id space is integral here: the algorithm may copy ids, compare
// them for equality and use them as map and mapping keys, never do
// arithmetic on them -- id arithmetic creeping back in (a minted root, a
// sentinel, an id-sized array) breaks this instantiation
////////////////////////////////////////////////////////////////////////////////

namespace {

struct opaque_vertex {
    unsigned int index;
    bool operator==(const opaque_vertex &) const = default;
};
struct opaque_arc {
    unsigned int index;
    bool operator==(const opaque_arc &) const = default;
};

template <typename K, typename T>
struct opaque_map {
    std::vector<T> values;
    T & operator[](const K & k) { return values[k.index]; }
    const T & operator[](const K & k) const { return values[k.index]; }
};

struct opaque_digraph {
    unsigned int n;
    std::vector<std::pair<unsigned int, unsigned int>> ends;

    auto vertices() const {
        return std::views::iota(0u, n) |
               std::views::transform(
                   [](const unsigned int i) { return opaque_vertex{i}; });
    }
    auto arcs() const {
        return std::views::iota(0u, static_cast<unsigned int>(ends.size())) |
               std::views::transform(
                   [](const unsigned int i) { return opaque_arc{i}; });
    }
    auto arc_source(const opaque_arc & a) const {
        return opaque_vertex{ends[a.index].first};
    }
    auto arc_target(const opaque_arc & a) const {
        return opaque_vertex{ends[a.index].second};
    }
    template <typename T>
    auto create_vertex_map() const {
        return opaque_map<opaque_vertex, T>{std::vector<T>(n)};
    }
    template <typename T>
    auto create_vertex_map(const T & d) const {
        return opaque_map<opaque_vertex, T>{std::vector<T>(n, d)};
    }
    template <typename T>
    auto create_arc_map() const {
        return opaque_map<opaque_arc, T>{std::vector<T>(ends.size())};
    }
    template <typename T>
    auto create_arc_map(const T & d) const {
        return opaque_map<opaque_arc, T>{std::vector<T>(ends.size(), d)};
    }
};

}  // namespace

static_assert(!std::integral<vertex_t<opaque_digraph>> &&
              !std::integral<arc_t<opaque_digraph>>);

GTEST_TEST(network_simplex, ids_need_not_be_integral) {
    opaque_digraph graph{4u,
                         {{0u, 1u}, {0u, 2u}, {1u, 2u}, {1u, 3u}, {2u, 3u}}};
    std::vector<int> upper = {4, 2, 2, 3, 5};
    std::vector<int> cost = {1, 2, 1, 3, 1};
    std::vector<int> supply = {4, 0, 0, -4};

    network_simplex alg(
        graph, [&](const opaque_arc & a) { return upper[a.index]; },
        [&](const opaque_arc & a) { return cost[a.index]; },
        [&](const opaque_vertex & v) { return supply[v.index]; });
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 12);
    for(auto && a : arcs(graph)) {
        ASSERT_GE(alg.flow(a), 0);
        ASSERT_LE(alg.flow(a), upper[a.index]);
    }
}

////////////////////////////////////////////////////////////////////////////////
// negative capacities and unbalanced supplies are preconditions, not silent
// wrong answers
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(network_simplex, negative_capacity_is_a_precondition) {
    static_digraph_builder<static_digraph, int, int> builder(2);
    builder.add_arc({0u, 1u}, -1, 0);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {0, 0};
    auto construct = [&]() {
        network_simplex alg(graph, upper, cost, supply);
        (void)alg;
    };
    EXPECT_DEATH(construct(), "");
}

GTEST_TEST(network_simplex, unbalanced_supplies_are_a_precondition) {
    static_digraph_builder<static_digraph, int, int> builder(2);
    builder.add_arc({0u, 1u}, 4, 1);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {1, 0};
    auto construct = [&]() {
        network_simplex alg(graph, upper, cost, supply);
        (void)alg;
    };
    EXPECT_DEATH(construct(), "");
}

////////////////////////////////////////////////////////////////////////////////
// the artificial arcs' cost has to outrun a whole residual path, so the
// bound it is built from is num_vertices * max|cost| -- a magnitude, and
// data-dependent. Both halves are pinned: instances under the bound solve,
// instances over it are a precondition rather than a silent verdict
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(network_simplex, all_negative_costs_need_the_cost_magnitude) {
    // Every cost here is <= 0, so a bound taken from max(cost) alone yields an
    // artificial cost of 3 while a residual path -- which traverses a -4 arc
    // backwards at +4 -- reaches 8. The artificial basis then looks cheaper
    // than the real route and this feasible instance comes back `infeasible`.
    static_digraph_builder<static_digraph, int, int> builder(3);
    builder.add_arc({0u, 2u}, 2, -4);
    builder.add_arc({2u, 2u}, 1, 0);
    builder.add_arc({1u, 2u}, 2, -1);
    builder.add_arc({0u, 0u}, 3, -3);
    builder.add_arc({2u, 0u}, 2, -1);
    auto [graph, upper, cost] = builder.build();
    std::vector<int> supply = {-2, 2, 0};

    // v1 must push its 2 units down 1>2, which forces 2>0 to 2 and 0>2 to 0;
    // both negative self-loops saturate: 2*(-1) + 3*(-3) + 2*(-1) = -13
    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), -13);
    check_flow_is_feasible(alg, graph, upper, supply);
    check_dual_certificate(alg, graph, upper, cost);
}

namespace {
// one unit down a path of `len` arcs, each costing a fifth of int's max/2
auto unit_along_a_costly_path(int len) {
    static_digraph_builder<static_digraph, int, int> builder(
        static_cast<std::size_t>(len + 1));
    for(int i = 0; i < len; ++i)
        builder.add_arc(
            {static_cast<unsigned int>(i), static_cast<unsigned int>(i + 1)}, 1,
            200'000'000);
    return builder.build();
}
}  // namespace

GTEST_TEST(network_simplex, costs_are_bounded_by_the_path_not_the_arc) {
    // Five arcs: the path costs 1e9, under the 1.07e9 an artificial arc used
    // to cost unconditionally, and the whole instance fits an int with room.
    auto [graph, upper, cost] = unit_along_a_costly_path(5);
    std::vector<int> supply(6, 0);
    supply.front() = 1;
    supply.back() = -1;

    network_simplex alg(graph, upper, cost, supply);
    ASSERT_EQ(alg.run().status(), mcf_status::optimal);
    ASSERT_EQ(alg.total_cost(), 1'000'000'000);
}

GTEST_TEST(network_simplex, oversized_costs_are_a_precondition) {
    // One arc more, and no artificial cost both outruns the 1.2e9 path and
    // leaves the potentials room inside an int: the instance needs a wider
    // cost type, and every verdict would be a guess. It used to be answered
    // `infeasible`.
    auto [graph, upper, cost] = unit_along_a_costly_path(6);
    std::vector<int> supply(7, 0);
    supply.front() = 1;
    supply.back() = -1;
    auto construct = [&]() {
        network_simplex alg(graph, upper, cost, supply);
        (void)alg;
    };
    EXPECT_DEATH(construct(), "");
}

////////////////////////////////////////////////////////////////////////////////
// differential: on random tiny instances the status and optimum match an
// exhaustive enumeration of every integral flow vector. Tiny and dense on
// purpose -- parallel arcs, self-loops, zero capacities and infeasibility all
// occur constantly at this size, and the reference is exponential.
// A failure names a seed: random_ranges_helper.hpp prints it and
// MELON_TEST_SEED replays it.
////////////////////////////////////////////////////////////////////////////////

namespace {

struct mcf_instance {
    unsigned num_vertices;
    std::vector<std::pair<unsigned, unsigned>> arc_ends;
    std::vector<int> upper, cost, supply;
};

mcf_instance random_mcf_instance() {
    mcf_instance out;
    out.num_vertices = 2 + test_rng()() % 4;
    const std::size_t num_arcs = 1 + test_rng()() % 6;
    for(std::size_t e = 0; e < num_arcs; ++e) {
        out.arc_ends.emplace_back(test_rng()() % out.num_vertices,
                                  test_rng()() % out.num_vertices);
        out.upper.push_back(static_cast<int>(test_rng()() % 4));
        out.cost.push_back(static_cast<int>(test_rng()() % 11) - 4);
    }
    out.supply.assign(out.num_vertices, 0);
    for(int k = 0; k < 3; ++k) {
        const int quantity = static_cast<int>(test_rng()() % 3);
        out.supply[test_rng()() % out.num_vertices] += quantity;
        out.supply[test_rng()() % out.num_vertices] -= quantity;
    }
    return out;
}

std::optional<long long> exhaustive_min_cost_flow(const mcf_instance & I) {
    const std::size_t num_arcs = I.arc_ends.size();
    std::vector<int> flow(num_arcs, 0);
    std::optional<long long> best;
    for(;;) {
        std::vector<int> excess(I.supply.begin(), I.supply.end());
        long long flow_cost = 0;
        for(std::size_t e = 0; e < num_arcs; ++e) {
            excess[I.arc_ends[e].first] -= flow[e];
            excess[I.arc_ends[e].second] += flow[e];
            flow_cost += static_cast<long long>(I.cost[e]) * flow[e];
        }
        if(std::ranges::all_of(excess, [](const int x) { return x == 0; }) &&
           (!best || flow_cost < *best))
            best = flow_cost;
        // odometer increment over the box [0, upper]
        std::size_t e = 0;
        while(e < num_arcs && flow[e] == I.upper[e]) flow[e++] = 0;
        if(e == num_arcs) break;
        ++flow[e];
    }
    return best;
}

}  // namespace

namespace {

// The instance on a mutable_digraph whose id spaces have removal holes: a
// junk vertex precedes every real one (so id 0 is always a hole) and a junk
// arc precedes every real one, all removed again before solving. This is the
// graph shape the dense-id requirement used to exclude.
struct holed_instance {
    mutable_digraph graph;
    std::vector<unsigned int> vertex_of;  // instance vertex -> graph id
    std::vector<unsigned int> arc_of;     // instance arc -> graph id
};

holed_instance build_holed_mutable_digraph(const mcf_instance & I) {
    holed_instance out;
    std::vector<unsigned int> junk_vertices, junk_arcs;
    for(unsigned int v = 0; v < I.num_vertices; ++v) {
        junk_vertices.push_back(out.graph.create_vertex());
        out.vertex_of.push_back(out.graph.create_vertex());
    }
    for(std::size_t e = 0; e < I.arc_ends.size(); ++e) {
        junk_arcs.push_back(
            out.graph.create_arc(junk_vertices[e % junk_vertices.size()],
                                 out.vertex_of[I.arc_ends[e].second]));
        out.arc_of.push_back(
            out.graph.create_arc(out.vertex_of[I.arc_ends[e].first],
                                 out.vertex_of[I.arc_ends[e].second]));
    }
    // arcs before vertices: remove_vertex frees its incident arcs, and a junk
    // arc removed twice trips mutable_digraph's validity assert
    for(const unsigned int a : junk_arcs) out.graph.remove_arc(a);
    for(const unsigned int v : junk_vertices) out.graph.remove_vertex(v);
    return out;
}

}  // namespace

static_assert(network_simplex_admits_graph<mutable_digraph>);

GTEST_TEST(network_simplex, mutable_digraph_with_id_holes_matches_exhaustive) {
    for(std::size_t iteration = 0; iteration < 300; ++iteration) {
        const mcf_instance I = random_mcf_instance();
        holed_instance H = build_holed_mutable_digraph(I);
        auto upper = create_arc_map<int>(H.graph);
        auto cost = create_arc_map<int>(H.graph);
        auto supply = create_vertex_map<int>(H.graph);
        for(std::size_t e = 0; e < I.arc_ends.size(); ++e) {
            upper[H.arc_of[e]] = I.upper[e];
            cost[H.arc_of[e]] = I.cost[e];
        }
        for(unsigned int v = 0; v < I.num_vertices; ++v)
            supply[H.vertex_of[v]] = I.supply[v];

        network_simplex alg(H.graph, upper, cost, supply);
        alg.run();
        const std::optional<long long> reference = exhaustive_min_cost_flow(I);
        ASSERT_EQ(alg.status() == mcf_status::optimal, reference.has_value());

        // a block size of one stresses the wrap of the resumable cursor over
        // the intrusive-list arcs() range
        network_simplex blocked(single_arc_block_traits{}, H.graph, upper, cost,
                                supply);
        ASSERT_EQ(blocked.run().status(), alg.status());

        if(!reference) continue;
        ASSERT_EQ(alg.total_cost(), *reference);
        ASSERT_EQ(blocked.total_cost(), *reference);
        check_flow_is_feasible(alg, H.graph, upper, supply);
        check_dual_certificate(alg, H.graph, upper, cost);
    }
}

// The shape that separates the two branches of the cursor's relocation: the
// arcs range is not borrowed, so a borrowed-range test alone would rebuild,
// yet a moved graph_ref_view left the graph exactly where it was.
static_assert(borrowed_graph<views::graph_all_t<mutable_digraph &>> &&
              !std::ranges::borrowed_range<
                  arcs_range_t<views::graph_all_t<mutable_digraph &>>>);

GTEST_TEST(network_simplex, move_mid_solve_rebuilds_the_arc_cursor) {
    // mutable_digraph's arcs() range is not borrowed, so the entering-arc
    // cursor refers into the graph; when the algorithm owns the graph, a
    // memberwise move would leave the cursor walking the moved-from object
    // -- ASan sees through this test if it does. Moving mid-solve, after
    // advance() parked the cursor mid-range, exercises the offset reseek.
    const mcf_instance I{4,
                         {{0, 1}, {0, 2}, {1, 2}, {1, 3}, {2, 3}},
                         {4, 2, 2, 3, 5},
                         {1, 2, 1, 3, 1},
                         {4, 0, 0, -4}};
    holed_instance H = build_holed_mutable_digraph(I);
    auto upper = create_arc_map<int>(H.graph);
    auto cost = create_arc_map<int>(H.graph);
    auto supply = create_vertex_map<int>(H.graph);
    for(std::size_t e = 0; e < I.arc_ends.size(); ++e) {
        upper[H.arc_of[e]] = I.upper[e];
        cost[H.arc_of[e]] = I.cost[e];
    }
    for(unsigned int v = 0; v < I.num_vertices; ++v)
        supply[H.vertex_of[v]] = I.supply[v];

    // An lvalue graph takes the other branch: only a graph_ref_view moved, so
    // the cursor is kept where it stands rather than walked back to its
    // offset. Same answer, and ASan still sees a kept cursor that should not
    // have been.
    {
        network_simplex ref_alg(H.graph, upper, cost, supply);
        ASSERT_FALSE(ref_alg.finished());
        ref_alg.advance();
        auto ref_moved = std::move(ref_alg);
        ref_moved.run();
        ASSERT_EQ(ref_moved.status(), mcf_status::optimal);
        ASSERT_EQ(ref_moved.total_cost(), 12);
    }

    network_simplex alg(std::move(H.graph), upper, cost, supply);
    ASSERT_FALSE(alg.finished());
    alg.advance();

    auto moved = std::move(alg);
    moved.run();
    ASSERT_EQ(moved.status(), mcf_status::optimal);
    ASSERT_EQ(moved.total_cost(), 12);

    // move assignment takes the same rebuild path
    network_simplex target(mutable_digraph{}, upper, cost, supply);
    target = std::move(moved);
    ASSERT_EQ(target.total_cost(), 12);
}

GTEST_TEST(network_simplex, differential_matches_exhaustive_enumeration) {
    for(std::size_t iteration = 0; iteration < 300; ++iteration) {
        const mcf_instance I = random_mcf_instance();
        static_digraph_builder<static_digraph, int, int> builder(
            I.num_vertices);
        for(std::size_t e = 0; e < I.arc_ends.size(); ++e)
            builder.add_arc({I.arc_ends[e].first, I.arc_ends[e].second},
                            I.upper[e], I.cost[e]);
        auto [graph, upper, cost] = builder.build();

        network_simplex alg(graph, upper, cost, I.supply);
        alg.run();
        const std::optional<long long> reference = exhaustive_min_cost_flow(I);

        ASSERT_EQ(alg.status() == mcf_status::optimal, reference.has_value());

        // every pivot rule takes a different pivot sequence through the same
        // instance -- first-eligible (a block of one) stresses the
        // wrap-around search, and best_eligible's traits turn the mixed
        // scan order on
        network_simplex blocked(single_arc_block_traits{}, graph, upper, cost,
                                I.supply);
        ASSERT_EQ(blocked.run().status(), alg.status());
        network_simplex best(best_eligible_traits{}, graph, upper, cost,
                             I.supply);
        ASSERT_EQ(best.run().status(), alg.status());

        if(!reference) continue;
        ASSERT_EQ(alg.total_cost(), *reference);
        ASSERT_EQ(blocked.total_cost(), *reference);
        ASSERT_EQ(best.total_cost(), *reference);
        check_flow_is_feasible(alg, graph, upper, I.supply);
        check_dual_certificate(alg, graph, upper, cost);
    }
}
