#undef NDEBUG
#include <gtest/gtest.h>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/algorithm/onaga.hpp"
#include "melon/container/d_ary_heap.hpp"
#include "melon/container/mutable_digraph.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/maps/element.hpp"
#include "melon/views/graph_view.hpp"

using namespace melon;

namespace {

using graph_t = static_digraph;
using vertex = vertex_t<graph_t>;
using arc = arc_t<graph_t>;
constexpr double unbounded = std::numeric_limits<double>::infinity();

template <bool SparseReset, bool FlowingArcLists>
struct flag_traits : onaga_default_traits<graph_t, double> {
    static constexpr bool sparse_reset = SparseReset;
    static constexpr bool flowing_arc_lists = FlowingArcLists;
};

// The off path of each flag is a second implementation of its mechanism,
// which the default traits never run: every test runs all four.
template <typename Check>
void for_each_flag_combination(Check && check) {
    {
        SCOPED_TRACE("sparse_reset, flowing_arc_lists");
        check(flag_traits<true, true>{});
    }
    {
        SCOPED_TRACE("sparse_reset");
        check(flag_traits<true, false>{});
    }
    {
        SCOPED_TRACE("flowing_arc_lists");
        check(flag_traits<false, true>{});
    }
    {
        SCOPED_TRACE("no flag");
        check(flag_traits<false, false>{});
    }
}

struct network {
    graph_t graph;
    arc_map_t<graph_t, double> gains;
    arc_map_t<graph_t, double> capacities;
};

// gains in [0.2, 1], or among 0.25, 0.5, 0.75 and 1 when tied, a share of
// the arcs with a finite capacity, parallel arcs and cycles allowed
network random_network(std::mt19937_64 & rng, const unsigned n,
                       const unsigned m, const double finite_share,
                       const bool tied = false) {
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    std::vector<std::tuple<vertex, vertex, double, double>> arc_list;
    while(arc_list.size() < m) {
        const auto u = static_cast<vertex>(rng() % n);
        const auto v = static_cast<vertex>(rng() % n);
        if(u == v) continue;
        arc_list.emplace_back(u, v,
                              tied ? 0.25 * static_cast<double>(1 + rng() % 4)
                                   : 0.2 + 0.8 * uniform(rng),
                              uniform(rng) < finite_share
                                  ? 0.05 + 2.0 * uniform(rng)
                                  : unbounded);
    }
    std::ranges::sort(arc_list);
    std::vector<vertex> sources, targets;
    for(const auto & [u, v, gain, capacity] : arc_list) {
        sources.push_back(u);
        targets.push_back(v);
    }
    graph_t graph(n, std::move(sources), std::move(targets));
    auto gains = graph.create_arc_map<double>();
    auto capacities = graph.create_arc_map<double>();
    for(std::size_t a = 0; a < arc_list.size(); ++a) {
        gains[static_cast<arc>(a)] = std::get<2>(arc_list[a]);
        capacities[static_cast<arc>(a)] = std::get<3>(arc_list[a]);
    }
    return {std::move(graph), std::move(gains), std::move(capacities)};
}

double random_supply(std::mt19937_64 & rng) {
    return 0.1 + 10.0 * (static_cast<double>(rng() % 1000) / 1000.0);
}

// ρ_s · supply + Σ_a capacity_a (gain_a ρ_v - ρ_u)^+ over the arcs of
// finite capacity: the dual value of the potentials, if they are feasible
// on the unbounded arcs
double dual_value(const network & net, const auto & alg, const vertex s,
                  const double supply) {
    double value = alg.potential(s) * supply;
    for(const auto a : arcs(net.graph)) {
        if(net.capacities[a] == unbounded) continue;
        const double rho_u = alg.potential(arc_source(net.graph, a));
        const double rho_v = alg.potential(arc_target(net.graph, a));
        value +=
            net.capacities[a] * std::max(net.gains[a] * rho_v - rho_u, 0.0);
    }
    return value;
}

void expect_feasible_dual(const network & net, const auto & alg,
                          const vertex t) {
    EXPECT_EQ(alg.potential(t), 1.0);
    for(const auto a : arcs(net.graph)) {
        if(net.capacities[a] != unbounded) continue;
        const double rho_u = alg.potential(arc_source(net.graph, a));
        const double rho_v = alg.potential(arc_target(net.graph, a));
        EXPECT_GE(rho_u, net.gains[a] * rho_v * (1 - 1e-12));
    }
}

// A feasible flow and a feasible dual of equal value: both are optimal.
void expect_optimal(const network & net, const auto & alg, const vertex s,
                    const vertex t, const double supply) {
    const double tolerance = 1e-9 * std::max(1.0, supply);
    std::vector<double> excess(num_vertices(net.graph), 0.0);
    for(const auto a : arcs(net.graph)) {
        const double f = alg.flow(a);
        EXPECT_GE(f, 0.0);
        EXPECT_LE(f, net.capacities[a] * (1 + 1e-12));
        excess[arc_source(net.graph, a)] -= f;
        excess[arc_target(net.graph, a)] += net.gains[a] * f;
    }
    for(const auto v : vertices(net.graph)) {
        if(v != s && v != t) {
            EXPECT_GE(excess[v], -tolerance);
        }
    }
    EXPECT_GE(excess[s], -supply - tolerance);
    EXPECT_NEAR(excess[t], alg.flow_value(), tolerance);
    expect_feasible_dual(net, alg, t);
    EXPECT_NEAR(dual_value(net, alg, s, supply), alg.flow_value(), tolerance);
}

// Bit for bit: the flags and the reuse of an object must not move a single
// rounding.
void expect_same_results(const network & net, const auto & a, const auto & b) {
    ASSERT_EQ(a.flow_value(), b.flow_value());
    ASSERT_EQ(a.remaining_supply(), b.remaining_supply());
    for(const auto v : vertices(net.graph))
        ASSERT_EQ(a.potential(v), b.potential(v)) << "vertex " << v;
    for(const auto e : arcs(net.graph))
        ASSERT_EQ(a.flow(e), b.flow(e)) << "arc " << e;
}

// the gain of a best path to t over the arcs of positive capacity
std::vector<double> best_gains_to(const network & net, const vertex t) {
    std::vector<double> labels;
    for(const auto v : vertices(net.graph))
        labels.push_back(v == t ? 1.0 : 0.0);
    for(std::size_t round = 0; round < labels.size(); ++round)
        for(const auto a : arcs(net.graph))
            if(net.capacities[a] > 0)
                labels[arc_source(net.graph, a)] =
                    std::max(labels[arc_source(net.graph, a)],
                             net.gains[a] * labels[arc_target(net.graph, a)]);
    return labels;
}

}  // namespace

// s = 0 sends 3 to t = 2 over s -> t (gain 0.8, capacity 1) and
// s -> 1 -> t (gains 0.5 and 1, capacities 1 and infinity): 0.8 + 0.5 reach
// t, 1 stays at s, and s reaches t through no residual arc. A flow is
// measured at its arc's source: 1 -> t carries the 0.5 that s -> 1 delivers.
GTEST_TEST(onaga, a_small_network_by_hand) {
    const graph_t graph(3, std::vector<vertex>{0, 0, 1},
                        std::vector<vertex>{1, 2, 2});
    const std::vector<double> gains{0.5, 0.8, 1.0};
    const std::vector<double> capacities{1.0, 1.0, unbounded};
    for_each_flag_combination([&](auto traits) {
        onaga alg(traits, graph, gains, capacities, 0u, 2u, 3.0);
        alg.run();
        EXPECT_TRUE(alg.finished());
        EXPECT_DOUBLE_EQ(alg.flow_value(), 1.3);
        EXPECT_DOUBLE_EQ(alg.remaining_supply(), 1.0);
        EXPECT_EQ(alg.flow(0u), 1.0);
        EXPECT_EQ(alg.flow(1u), 1.0);
        EXPECT_EQ(alg.flow(2u), 0.5);
        EXPECT_EQ(alg.potential(0u), 0.0);
        EXPECT_EQ(alg.potential(1u), 1.0);
        EXPECT_EQ(alg.potential(2u), 1.0);
        for(const auto a : arcs(graph))
            EXPECT_EQ(alg.flows_map()[a], alg.flow(a));
        for(const auto v : vertices(graph))
            EXPECT_EQ(alg.potentials_map()[v], alg.potential(v));
    });
}

GTEST_TEST(onaga, maximum_flow_and_optimal_dual_on_random_lossy_networks) {
    for_each_flag_combination([](auto traits) {
        std::mt19937_64 rng(7);
        int sent_in_part = 0;
        for(int instance = 0; instance < 300; ++instance) {
            const auto n = static_cast<unsigned>(2 + rng() % 39);
            const auto m = static_cast<unsigned>(n + rng() % (3 * n));
            const network net = random_network(rng, n, m, 0.5);
            const auto s = static_cast<vertex>(rng() % n);
            auto t = static_cast<vertex>(rng() % n);
            if(t == s) t = (s + 1) % n;
            const double supply = random_supply(rng);
            onaga alg(traits, net.graph, net.gains, net.capacities, s, t,
                      supply);
            alg.run();
            ASSERT_TRUE(alg.finished());
            expect_optimal(net, alg, s, t, supply);
            // a second run() is a no-op, the results still readable
            const double value = alg.flow_value(), rho_s = alg.potential(s);
            alg.run();
            EXPECT_EQ(alg.flow_value(), value);
            EXPECT_EQ(alg.potential(s), rho_s);
            if(alg.flow_value() > 0 && alg.remaining_supply() > 1e-9 * supply)
                ++sent_in_part;
        }
        // the capacities bind on enough instances for the dual to be tested
        EXPECT_GT(sent_in_part, 50);
    });
}

// The resets may be sparse and the statuses are epochs: a reused object must
// give a fresh object's results bit for bit, through the clearing of the
// statuses every 127 searches.
GTEST_TEST(onaga, a_reset_object_gives_a_fresh_objects_results) {
    for_each_flag_combination([](auto traits) {
        std::mt19937_64 rng(11);
        network net = random_network(rng, 40, 160, 0.5);
        onaga reused(traits, net.graph, net.gains, net.capacities);
        for(int run = 0; run < 400; ++run) {
            SCOPED_TRACE(run);
            const auto s = static_cast<vertex>(rng() % 40);
            const auto t = static_cast<vertex>((s + 1 + rng() % 39) % 40);
            const double supply = random_supply(rng);
            // the maps are read live: a change between two runs, then reset()
            const auto a = static_cast<arc>(rng() % 160);
            if(net.capacities[a] != unbounded)
                net.capacities[a] = static_cast<double>(rng() % 3) * 0.5;
            reused.reset().set_source(s, supply).set_target(t).run();
            onaga fresh(traits, net.graph, net.gains, net.capacities, s, t,
                        supply);
            fresh.run();
            expect_same_results(net, reused, fresh);
        }
    });
}

// Past a bound on the augmentations, the potentials are still feasible, and
// their dual value is that of the flow sent plus ρ_s times the supply left:
// an upper bound on the maximum.
GTEST_TEST(onaga, a_run_cut_short_bounds_the_maximum) {
    for_each_flag_combination([](auto traits) {
        std::mt19937_64 rng(13);
        int cut_short = 0;
        for(int instance = 0; instance < 200; ++instance) {
            const network net = random_network(rng, 30, 120, 0.6);
            const vertex s = 0, t = 29;
            const double supply = 5.0;
            onaga full(traits, net.graph, net.gains, net.capacities, s, t,
                       supply);
            full.run();
            for(const std::size_t limit :
                {std::size_t{0}, std::size_t{1}, std::size_t{2}}) {
                onaga alg(traits, net.graph, net.gains, net.capacities, s, t,
                          supply);
                for(std::size_t i = 0; i < limit && !alg.finished(); ++i)
                    alg.advance();
                alg.compute_potentials();
                if(!alg.finished()) ++cut_short;
                expect_feasible_dual(net, alg, t);
                const double bound = dual_value(net, alg, s, supply);
                EXPECT_NEAR(bound,
                            alg.flow_value() +
                                alg.potential(s) * alg.remaining_supply(),
                            1e-9 * supply);
                EXPECT_GE(bound, full.flow_value() * (1 - 1e-12));
            }
        }
        EXPECT_GT(cut_short, 100);
    });
}

// Start labels change the searches, not the maximum flow nor the potentials,
// up to rounding. A vector is a label map: set_start_labels reads it once.
GTEST_TEST(onaga, start_labels_leave_the_result) {
    for_each_flag_combination([](auto traits) {
        std::mt19937_64 rng(17);
        for(int instance = 0; instance < 200; ++instance) {
            const network net = random_network(rng, 30, 120, 0.5);
            const vertex s = 0, t = 29;
            const double supply = 3.0;
            onaga plain(traits, net.graph, net.gains, net.capacities, s, t,
                        supply);
            plain.run();
            onaga labelled(traits, net.graph, net.gains, net.capacities);
            labelled.set_start_labels(best_gains_to(net, t));
            labelled.set_source(s, supply).set_target(t).run();
            expect_optimal(net, labelled, s, t, supply);
            EXPECT_NEAR(labelled.flow_value(), plain.flow_value(),
                        1e-9 * supply);
            for(const auto v : vertices(net.graph))
                EXPECT_NEAR(labelled.potential(v), plain.potential(v), 1e-9);
        }
    });
}

// The docs' idiom: the potentials of a run with a supply of 0 are the best
// gains to t over the arcs of positive capacity, feasible exactly, since each
// is the very product its arc's test compares.
GTEST_TEST(onaga, a_run_without_supply_gives_feasible_start_labels) {
    for_each_flag_combination([](auto traits) {
        std::mt19937_64 rng(59);
        for(int instance = 0; instance < 100; ++instance) {
            const network net = random_network(rng, 30, 120, 0.5);
            const vertex s = 0, t = 29;
            onaga probe(traits, net.graph, net.gains, net.capacities, s, t,
                        0.0);
            probe.run();
            EXPECT_EQ(probe.flow_value(), 0.0);
            const auto labels = best_gains_to(net, t);
            for(const auto v : vertices(net.graph))
                EXPECT_NEAR(probe.potential(v), labels[v], 1e-12);
            for(const auto a : arcs(net.graph)) {
                if(net.capacities[a] > 0) {
                    EXPECT_GE(probe.potential(arc_source(net.graph, a)),
                              net.gains[a] *
                                  probe.potential(arc_target(net.graph, a)));
                }
            }
            onaga alg(traits, net.graph, net.gains, net.capacities);
            alg.set_start_labels(probe.potentials_map());
            alg.set_source(s, 3.0).set_target(t).run();
            expect_optimal(net, alg, s, t, 3.0);
        }
    });
}

// New start labels on an object that carries flow, then reset(), in either
// order: the labels of the next run are the new ones, not the old nor 1.
GTEST_TEST(onaga, start_labels_change_on_a_used_object) {
    for_each_flag_combination([](auto traits) {
        std::mt19937_64 rng(61);
        for(int instance = 0; instance < 100; ++instance) {
            const network net = random_network(rng, 30, 120, 0.5);
            const vertex s = 0, t = 29;
            const auto labels = best_gains_to(net, t);
            onaga fresh(traits, net.graph, net.gains, net.capacities);
            fresh.set_start_labels(labels).set_source(s, 3.0).set_target(t);
            fresh.run();
            onaga before(traits, net.graph, net.gains, net.capacities, s, t,
                         3.0);
            before.run();
            before.set_start_labels(labels).reset().run();
            expect_same_results(net, before, fresh);
            onaga after(traits, net.graph, net.gains, net.capacities, s, t,
                        3.0);
            after.run();
            after.reset().set_start_labels(labels).run();
            expect_same_results(net, after, fresh);
        }
    });
}

// Many sources to one target: start labels set once, then a reset() per
// source, which must restore them and not 1.
GTEST_TEST(onaga, a_reset_object_keeps_its_start_labels) {
    for_each_flag_combination([](auto traits) {
        std::mt19937_64 rng(19);
        const network net = random_network(rng, 40, 160, 0.5);
        const vertex t = 39;
        const auto labels = best_gains_to(net, t);
        const auto label_map =
            maps::function([&](const vertex v) { return labels[v]; });
        onaga reused(traits, net.graph, net.gains, net.capacities);
        reused.set_start_labels(label_map);
        for(int run = 0; run < 400; ++run) {
            SCOPED_TRACE(run);
            const auto s = static_cast<vertex>(rng() % 39);
            const double supply = random_supply(rng);
            // the setters, then reset(): the other order than the docs' loop
            reused.set_source(s, supply).set_target(t).reset().run();
            onaga fresh(traits, net.graph, net.gains, net.capacities);
            fresh.set_start_labels(label_map);
            fresh.set_source(s, supply).set_target(t).run();
            expect_same_results(net, reused, fresh);
        }
    });
}

// Tied gains give the searches ties, which a search that starts from the tree
// of the last one breaks in its own order: the flow must stay optimal, and a
// reused object must still give a fresh one's results.
GTEST_TEST(onaga, tied_gains_keep_the_flow_optimal_and_a_reused_object_fresh) {
    for_each_flag_combination([](auto traits) {
        std::mt19937_64 rng(23);
        for(int instance = 0; instance < 100; ++instance) {
            const network net = random_network(rng, 30, 120, 0.5, true);
            const vertex t = 29;
            const auto labels = best_gains_to(net, t);
            onaga reused(traits, net.graph, net.gains, net.capacities);
            reused.set_start_labels(labels);
            for(int run = 0; run < 10; ++run) {
                const auto s = static_cast<vertex>(rng() % 29);
                const double supply = random_supply(rng);
                reused.reset().set_source(s, supply).set_target(t).run();
                expect_optimal(net, reused, s, t, supply);
                onaga fresh(traits, net.graph, net.gains, net.capacities);
                fresh.set_start_labels(labels);
                fresh.set_source(s, supply).set_target(t).run();
                expect_same_results(net, reused, fresh);
            }
        }
    });
}

// compute_potentials() between two advance() calls must leave the search that
// follows unchanged: it starts from the tree of the one before.
GTEST_TEST(onaga, potentials_between_augmentations_change_no_flow) {
    for_each_flag_combination([](auto traits) {
        std::mt19937_64 rng(29);
        for(int instance = 0; instance < 200; ++instance) {
            const network net =
                random_network(rng, 30, 120, 0.6, instance % 2 == 1);
            const vertex s = 0, t = 29;
            const double supply = 5.0;
            onaga plain(traits, net.graph, net.gains, net.capacities, s, t,
                        supply);
            onaga probed(traits, net.graph, net.gains, net.capacities, s, t,
                         supply);
            while(!plain.finished()) plain.advance();
            while(!probed.finished()) {
                probed.advance();
                probed.compute_potentials();
            }
            plain.compute_potentials();
            expect_same_results(net, plain, probed);
        }
    });
}

// The unbounded capacity is infinity, and the largest finite value does the
// same: a test of the open arcs that infinity fails closes every such arc.
GTEST_TEST(onaga, the_largest_capacity_does_what_infinity_does) {
    for_each_flag_combination([](auto traits) {
        std::mt19937_64 rng(31);
        for(int instance = 0; instance < 200; ++instance) {
            const network net = random_network(rng, 30, 120, 0.5);
            auto largest = net.capacities;
            for(const auto a : arcs(net.graph))
                if(largest[a] == unbounded)
                    largest[a] = std::numeric_limits<double>::max();
            const vertex s = 0, t = 29;
            const double supply = 3.0;
            onaga infinite(traits, net.graph, net.gains, net.capacities, s, t,
                           supply);
            onaga finite(traits, net.graph, net.gains, largest, s, t, supply);
            infinite.run();
            finite.run();
            expect_optimal(net, infinite, s, t, supply);
            expect_same_results(net, infinite, finite);
        }
    });
}

// A run that sends its whole supply leaves the tree of its last search for
// the next search. reset() and set_source() must drop it: a new source that
// starts from it settles that tree at a gain that is not its own.
GTEST_TEST(onaga, a_new_source_never_starts_from_the_last_tree) {
    for_each_flag_combination([](auto traits) {
        std::mt19937_64 rng(37);
        for(int instance = 0; instance < 300; ++instance) {
            const network net = random_network(rng, 8, 24, 0.5, instance % 2);
            onaga reused(traits, net.graph, net.gains, net.capacities);
            for(int run = 0; run < 20; ++run) {
                const auto s = static_cast<vertex>(rng() % 8);
                const auto t = static_cast<vertex>((s + 1 + rng() % 7) % 8);
                const double supply = run % 2 ? 0.05 : random_supply(rng);
                reused.reset().set_source(s, supply).set_target(t).run();
                onaga fresh(traits, net.graph, net.gains, net.capacities, s, t,
                            supply);
                fresh.run();
                expect_same_results(net, reused, fresh);
                // again from the same source, after reset() alone
                reused.reset().run();
                expect_same_results(net, reused, fresh);
            }
            // the setters keep the flow: a second source sent on top of the
            // first, set_source() alone or after set_target()
            const auto s1 = static_cast<vertex>(rng() % 8),
                       s2 = static_cast<vertex>(rng() % 8);
            const auto t = static_cast<vertex>((s1 + 1 + rng() % 7) % 8);
            if(s2 == t) continue;
            onaga alone(traits, net.graph, net.gains, net.capacities, s1, t,
                        0.05);
            onaga after(traits, net.graph, net.gains, net.capacities, s1, t,
                        0.05);
            alone.run();
            after.run();
            alone.set_source(s2, 5.0).run();
            after.set_target(t).set_source(s2, 5.0).run();
            expect_same_results(net, alone, after);
        }
    });
}

// sparse_reset changes what reset() walks and never a rounding. The lists
// change the order of the scans, hence which of equal gains wins: the same
// optimum, up to rounding.
GTEST_TEST(onaga, the_flags_change_no_optimum) {
    std::mt19937_64 rng(41);
    for(int instance = 0; instance < 100; ++instance) {
        const network net =
            random_network(rng, 30, 120, 0.5, instance % 2 == 1);
        const vertex t = 29;
        onaga all(flag_traits<true, true>{}, net.graph, net.gains,
                  net.capacities);
        onaga lists(flag_traits<false, true>{}, net.graph, net.gains,
                    net.capacities);
        onaga sparse(flag_traits<true, false>{}, net.graph, net.gains,
                     net.capacities);
        onaga none(flag_traits<false, false>{}, net.graph, net.gains,
                   net.capacities);
        for(int run = 0; run < 10; ++run) {
            SCOPED_TRACE(run);
            const auto s = static_cast<vertex>(rng() % 29);
            const double supply = random_supply(rng);
            all.reset().set_source(s, supply).set_target(t).run();
            lists.reset().set_source(s, supply).set_target(t).run();
            sparse.reset().set_source(s, supply).set_target(t).run();
            none.reset().set_source(s, supply).set_target(t).run();
            expect_same_results(net, all, lists);
            expect_same_results(net, sparse, none);
            expect_optimal(net, none, s, t, supply);
            EXPECT_NEAR(all.flow_value(), none.flow_value(), 1e-9 * supply);
            for(const auto v : vertices(net.graph))
                EXPECT_NEAR(all.potential(v), none.potential(v), 1e-9);
        }
    }
}

// A move mid-run carries the tree that the next search starts from.
GTEST_TEST(onaga, a_move_between_augmentations_changes_nothing) {
    for_each_flag_combination([](auto traits) {
        std::mt19937_64 rng(43);
        for(int instance = 0; instance < 100; ++instance) {
            const network net = random_network(rng, 30, 120, 0.6);
            const vertex s = 0, t = 29;
            onaga plain(traits, net.graph, net.gains, net.capacities, s, t,
                        5.0);
            plain.run();
            onaga moved(traits, net.graph, net.gains, net.capacities, s, t,
                        5.0);
            if(!moved.finished()) moved.advance();
            auto target = std::move(moved);
            target.run();
            expect_same_results(net, plain, target);
        }
    });
}

// A traits heap is an alias template the algorithm instantiates with its own
// order: a binary heap in place of the default 4-ary one reaches the same
// optimum.
struct binary_heap_traits : onaga_default_traits<graph_t, double> {
    template <std::strict_weak_order<double, double> Compare>
    using heap = updatable_d_ary_heap<
        2, std::pair<vertex, double>, Compare,
        vertex_map_t<graph_t, std::size_t, onaga_roles::heap_index>,
        maps::element<1>, maps::element<0>>;
};

GTEST_TEST(onaga, a_traits_heap_of_another_arity_reaches_the_same_optimum) {
    std::mt19937_64 rng(71);
    for(int instance = 0; instance < 100; ++instance) {
        const network net = random_network(rng, 30, 120, 0.5);
        const vertex s = 0, t = 29;
        const double supply = random_supply(rng);
        onaga binary(binary_heap_traits{}, net.graph, net.gains, net.capacities,
                     s, t, supply);
        onaga plain(net.graph, net.gains, net.capacities, s, t, supply);
        binary.run();
        plain.run();
        expect_optimal(net, binary, s, t, supply);
        EXPECT_NEAR(binary.flow_value(), plain.flow_value(), 1e-9 * supply);
        for(const auto v : vertices(net.graph))
            EXPECT_NEAR(binary.potential(v), plain.potential(v), 1e-9);
    }
}

// The default tolerances are relative to the value type's precision: at
// float, a plain 1e-11 would be 0 and leave every filled arc open.
GTEST_TEST(onaga, a_float_network_converges_to_the_double_optimum) {
    std::mt19937_64 rng(47);
    for(int instance = 0; instance < 100; ++instance) {
        const network net = random_network(rng, 30, 120, 0.5);
        std::vector<float> gains, capacities;
        for(const auto a : arcs(net.graph)) {
            gains.push_back(static_cast<float>(net.gains[a]));
            capacities.push_back(static_cast<float>(net.capacities[a]));
        }
        const vertex s = 0, t = 29;
        onaga single(net.graph, gains, capacities, s, t, 3.0f);
        single.run();
        onaga exact(net.graph, net.gains, net.capacities, s, t, 3.0);
        exact.run();
        EXPECT_TRUE(single.finished());
        EXPECT_NEAR(single.flow_value(), exact.flow_value(),
                    1e-2 * std::max(1.0, exact.flow_value()));
    }
}

// The value type is the common type of the two maps, and each read converts
// exactly: an int capacity or a float gain gives the bits of the double run.
// An owned graph, a lambda map and a mutable_digraph change nothing either.
GTEST_TEST(onaga, other_value_types_graphs_and_maps_give_the_same_flow) {
    std::mt19937_64 rng(67);
    for(int instance = 0; instance < 100; ++instance) {
        network net = random_network(rng, 30, 120, 0.5);
        std::vector<int> int_capacities;
        std::vector<float> float_gains;
        for(const auto a : arcs(net.graph)) {
            if(net.capacities[a] != unbounded)
                net.capacities[a] = static_cast<double>(1 + rng() % 3);
            int_capacities.push_back(net.capacities[a] == unbounded
                                         ? std::numeric_limits<int>::max()
                                         : static_cast<int>(net.capacities[a]));
            float_gains.push_back(static_cast<float>(net.gains[a]));
        }
        const vertex s = 0, t = 29;
        const double supply = random_supply(rng);

        auto int_bounded = net.capacities;
        for(const auto a : arcs(net.graph))
            int_bounded[a] = static_cast<double>(int_capacities[a]);
        onaga with_ints(net.graph, net.gains, int_capacities, s, t, supply);
        onaga with_doubles(net.graph, net.gains, int_bounded, s, t, supply);
        with_ints.run();
        with_doubles.run();
        expect_same_results(net, with_ints, with_doubles);

        network rounded = net;
        for(const auto a : arcs(net.graph))
            rounded.gains[a] = static_cast<double>(float_gains[a]);
        onaga with_floats(net.graph, float_gains, net.capacities, s, t, supply);
        onaga with_rounded(rounded.graph, rounded.gains, rounded.capacities, s,
                           t, supply);
        with_floats.run();
        with_rounded.run();
        expect_same_results(net, with_floats, with_rounded);

        onaga owned(
            graph_t(net.graph), [&net](const arc & a) { return net.gains[a]; },
            net.capacities, s, t, supply);
        owned.run();
        onaga plain(net.graph, net.gains, net.capacities, s, t, supply);
        plain.run();
        expect_same_results(net, owned, plain);

        mutable_digraph mutable_graph;
        std::vector<vertex_t<mutable_digraph>> mutable_vertices;
        for(std::size_t i = 0; i < num_vertices(net.graph); ++i)
            mutable_vertices.push_back(mutable_graph.create_vertex());
        std::vector<arc_t<mutable_digraph>> mutable_arcs;
        for(const auto a : arcs(net.graph))
            mutable_arcs.push_back(mutable_graph.create_arc(
                mutable_vertices[arc_source(net.graph, a)],
                mutable_vertices[arc_target(net.graph, a)]));
        auto mutable_gains = create_arc_map<double>(mutable_graph);
        auto mutable_capacities = create_arc_map<double>(mutable_graph);
        for(const auto a : arcs(net.graph)) {
            mutable_gains[mutable_arcs[a]] = net.gains[a];
            mutable_capacities[mutable_arcs[a]] = net.capacities[a];
        }
        onaga on_mutable(mutable_graph, mutable_gains, mutable_capacities,
                         mutable_vertices[s], mutable_vertices[t], supply);
        on_mutable.run();
        EXPECT_NEAR(on_mutable.flow_value(), plain.flow_value(), 1e-9 * supply);
        for(const auto v : vertices(net.graph))
            EXPECT_NEAR(on_mutable.potential(mutable_vertices[v]),
                        plain.potential(v), 1e-9);
    }
}

GTEST_TEST(onaga, the_preconditions_are_asserted) {
    const graph_t graph(3, std::vector<vertex>{0, 0, 1},
                        std::vector<vertex>{1, 2, 2});
    const std::vector<double> gains{0.5, 0.8, 1.0};
    const std::vector<double> capacities{1.0, 1.0, unbounded};
    const std::vector<double> labels{0.8, 1.0, 1.0};

    onaga unset(graph, gains, capacities);
    EXPECT_DEATH((void)unset.run(), "");
    EXPECT_DEATH((void)unset.set_source(0u, unbounded), "");

    onaga alg(graph, gains, capacities, 0u, 2u, 3.0);
    EXPECT_DEATH((void)alg.potential(0u), "");
    alg.run();
    alg.set_start_labels(labels);
    EXPECT_DEATH((void)alg.compute_potentials(), "");
    alg.reset();
    EXPECT_DEATH((void)alg.potential(0u), "");
    alg.run();
    EXPECT_DOUBLE_EQ(alg.flow_value(), 1.3);
}

GTEST_TEST(onaga, expiring_map_accessors_outlive_the_algorithm) {
    std::mt19937_64 rng(53);
    const network net = random_network(rng, 30, 120, 0.5);
    const vertex s = 0, t = 29;
    using alg_t =
        decltype(onaga(net.graph, net.gains, net.capacities, s, t, 1.0));
    auto flows_source = std::make_unique<alg_t>(net.graph, net.gains,
                                                net.capacities, s, t, 3.0);
    auto potentials_source = std::make_unique<alg_t>(net.graph, net.gains,
                                                     net.capacities, s, t, 3.0);
    flows_source->run();
    potentials_source->run();
    std::vector<double> flows, potentials;
    for(const auto a : arcs(net.graph)) flows.push_back(flows_source->flow(a));
    for(const auto v : vertices(net.graph))
        potentials.push_back(potentials_source->potential(v));
    auto flows_map = std::move(*flows_source).flows_map();
    auto potentials_map = std::move(*potentials_source).potentials_map();
    flows_source.reset();
    potentials_source.reset();
    for(const auto a : arcs(net.graph)) EXPECT_EQ(flows_map[a], flows[a]);
    for(const auto v : vertices(net.graph))
        EXPECT_EQ(potentials_map[v], potentials[v]);
}

namespace {
template <typename Graph, typename GainMap, typename CapacityMap>
concept onaga_accepts =
    requires { typename onaga<Graph, GainMap, CapacityMap>; };

struct no_flags_traits {
    template <typename Compare>
    using heap = onaga_default_traits<graph_t, double>::heap<Compare>;
    static constexpr double arc_full = 1e-11;
    static constexpr double negligible_flow = 1e-12;
};
struct fixed_heap_traits : onaga_default_traits<graph_t, double> {
    using heap = onaga_default_traits<graph_t, double>::heap<std::less<double>>;
};
}  // namespace

GTEST_TEST(onaga, contract_pins) {
    using RG = views::graph_all_t<const graph_t &>;
    using DM = maps::mapping_all_t<const std::vector<double> &>;
    using IM = maps::mapping_all_t<const std::vector<int> &>;
    // Flows are fractional whatever the capacities: integral capacities pass
    // with floating-point gains, an integral common type does not.
    static_assert(onaga_accepts<RG, DM, DM>);
    static_assert(onaga_accepts<RG, DM, IM>);
    static_assert(!onaga_accepts<RG, IM, IM>);
    static_assert(!onaga_traits<no_flags_traits, double>);
    static_assert(onaga_traits<onaga_default_traits<graph_t, float>, float>);
    // The traits name the heap's structure, the algorithm its order: a heap
    // spelled whole, comparator included, is not a traits heap.
    static_assert(!onaga_traits<fixed_heap_traits, double>);

    // the double defaults are exactly the constants Gecot was measured with
    static_assert(onaga_default_traits<graph_t, double>::arc_full == 1e-11);
    static_assert(onaga_default_traits<graph_t, double>::negligible_flow ==
                  1e-12);
    using float_traits = onaga_default_traits<graph_t, float>;
    static_assert(1.0f - float_traits::arc_full < 1.0f);
    static_assert(float_traits::negligible_flow > 0.0f);

    const graph_t g;
    const std::vector<double> gains, capacities;
    static_assert(
        std::same_as<decltype(onaga(g, gains, capacities)), onaga<RG, DM, DM>>);
    static_assert(std::same_as<decltype(onaga(flag_traits<false, false>{}, g,
                                              gains, capacities, 0u, 1u, 1)),
                               onaga<RG, DM, DM, flag_traits<false, false>>>);
    SUCCEED();
}
