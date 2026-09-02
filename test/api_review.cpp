#undef NDEBUG
#include <gtest/gtest.h>

#include <concepts>
#include <memory>
#include <ranges>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "melon/algorithm/biobjective_dijkstra.hpp"
#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/algorithm/competing_dijkstras.hpp"
#include "melon/algorithm/depth_first_search.hpp"
#include "melon/algorithm/dijkstra.hpp"
#include "melon/algorithm/dinitz.hpp"
#include "melon/algorithm/edmonds_karp.hpp"
#include "melon/algorithm/knapsack_bnb.hpp"
#include "melon/algorithm/kruskal.hpp"
#include "melon/algorithm/network_simplex.hpp"
#include "melon/algorithm/network_voronoi.hpp"
#include "melon/algorithm/strongly_connected_components.hpp"
#include "melon/algorithm/topological_sort.hpp"
#include "melon/algorithm/traversal_forest.hpp"
#include "melon/algorithm/unbounded_knapsack_bnb.hpp"
#include "melon/container/mutable_digraph.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/container/static_filter_map.hpp"
#include "melon/container/static_forward_digraph.hpp"
#include "melon/detail/consumable_view.hpp"
#include "melon/maps/constant.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/complete_digraph.hpp"
#include "melon/views/graph_view.hpp"
#include "melon/views/reverse.hpp"
#include "melon/views/subgraph.hpp"
#include "melon/views/undirect.hpp"
#include "melon/views/undirected_graph_view.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// a relocated algorithm owns its state: no cursor may point into another object
////////////////////////////////////////////////////////////////////////////////

// kruskal keeps its cursor as an iterator into its own _sorted_edges.
// Destroying the source first turns a cursor left behind into an ASan report
// instead of a silently wrong traversal.
GTEST_TEST(api_review, kruskal_move_owns_its_cursor) {
    static_digraph_builder<static_digraph, double> b(4);
    b.add_arc(0u, 1u, 1.0)
        .add_arc(1u, 2u, 2.0)
        .add_arc(2u, 3u, 3.0)
        .add_arc(0u, 3u, 9.0);
    auto [g, costs] = b.build();
    auto ug = views::undirect(g);

    auto source = std::make_unique<decltype(kruskal(ug, costs))>(ug, costs);
    auto moved = std::move(*source);
    source.reset();

    std::vector<unsigned> taken;
    for(; !moved.finished(); moved.advance()) taken.push_back(moved.current());
    EXPECT_EQ(taken.size(), 3u) << "a spanning tree of 4 vertices has 3 edges";
}

GTEST_TEST(api_review, kruskal_move_assignment_owns_its_cursor) {
    static_digraph_builder<static_digraph, double> b(3);
    b.add_arc(0u, 1u, 1.0).add_arc(1u, 2u, 2.0);
    auto [g, costs] = b.build();
    auto ug = views::undirect(g);

    kruskal reference(ug, costs);
    auto source = std::make_unique<decltype(kruskal(ug, costs))>(ug, costs);
    reference = std::move(*source);
    source.reset();

    std::size_t n = 0;
    for(; !reference.finished(); reference.advance()) ++n;
    EXPECT_EQ(n, 2u);
}

// knapsack_bnb / unbounded_knapsack_bnb keep _best_sol as iterators into their
// own _value_cost_pairs.
GTEST_TEST(api_review, knapsack_move_owns_its_solution) {
    std::vector<unsigned> items{0u, 1u, 2u, 3u};
    std::vector<double> value{10.0, 7.0, 5.0, 3.0};
    std::vector<double> cost{5.0, 4.0, 3.0, 2.0};

    using knapsack = decltype(knapsack_bnb(items, value, cost, 9.0));
    auto source = std::make_unique<knapsack>(items, value, cost, 9.0);
    source->run();
    const double expected = source->solution_value();

    auto moved = std::move(*source);
    source.reset();
    EXPECT_EQ(moved.solution_value(), expected);
    EXPECT_LE(moved.solution_cost(), 9.0);
    std::size_t n = 0;
    for([[maybe_unused]] auto && i : moved.solution_items()) ++n;
    EXPECT_GT(n, 0u);
}

GTEST_TEST(api_review, unbounded_knapsack_move_owns_its_solution) {
    std::vector<unsigned> items{0u, 1u};
    std::vector<double> value{6.0, 5.0};
    std::vector<double> cost{4.0, 3.0};

    using knapsack = decltype(unbounded_knapsack_bnb(items, value, cost, 9.0));
    auto source = std::make_unique<knapsack>(items, value, cost, 9.0);
    source->run();
    const double expected = source->solution_value();

    auto moved = std::move(*source);
    source.reset();
    EXPECT_EQ(moved.solution_value(), expected);
}

// The generic breadth_first_search specialisation keeps its cursor as an
// iterator into its own _queue. Destroy the source first: a cursor into a live
// source reads plausible values.
namespace {
struct distance_bfs_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = true;  // selects the generic one
    static constexpr bool store_traversal_range = false;
};
}  // namespace

GTEST_TEST(api_review, generic_bfs_move_keeps_its_cursor) {
    static_digraph_builder<static_digraph> b(4);
    b.add_arc(0u, 1u).add_arc(0u, 2u).add_arc(1u, 3u).add_arc(2u, 3u);
    auto [g] = b.build();

    using BFS = breadth_first_search<graph_ref_view<static_digraph>,
                                     distance_bfs_traits>;
    static_assert(std::movable<BFS> && !std::copyable<BFS>);

    auto source = std::make_unique<BFS>(g);
    source->add_source(0u);
    source->advance();

    BFS moved = std::move(*source);
    source.reset();

    moved.run();
    for(const auto & v : melon::vertices(g)) EXPECT_TRUE(moved.reached(v));
    EXPECT_EQ(moved.dist(3u), 2);
}

// A cursor over a non-borrowed range keeps an iterator that refers back into
// the range it holds (a filter_view iterator holds a parent pointer), and these
// cursors live inside a std::vector. Relocating that vector without rebasing
// leaves every stack frame's cursor aimed at freed memory -- with no copy
// anywhere in the program. The branch at the root is what makes it reachable:
// without it the traversal never increments a relocated cursor.
//
// This one only *fails* under a sanitizer -- the read lands on freed memory
// that still holds the right bytes -- so it is written for the
// linux-gcc15-sanitize job (-DMELON_SANITIZE=address,undefined), which reports
// the heap-use-after-free in detail::consumable_input_view::current().
GTEST_TEST(api_review, dfs_cursors_survive_stack_reallocation) {
    constexpr unsigned n = 12;
    static_digraph_builder<static_digraph> b(n);
    b.add_arc(0u, 1u);     // long branch, forces regrowth
    b.add_arc(0u, n - 1);  // taken only after backtracking
    for(unsigned i = 1; i + 2 < n; ++i) b.add_arc(i, i + 1);
    auto [g] = b.build();

    auto vertex_filter = g.create_vertex_map<bool>(true);
    auto sub = views::subgraph(g, vertex_filter);
    ASSERT_FALSE(has_num_vertices<decltype(sub)>)
        << "no reserve, so the stack really does reallocate";

    depth_first_search dfs(sub, 0u);
    std::size_t visited = 0;
    for(; !dfs.finished(); dfs.advance()) ++visited;
    EXPECT_EQ(visited, n);
}

// The other half of the cursor problem: views::subgraph's filtered ranges
// capture `this`, so a range obtained from a subgraph an algorithm stores *by
// value* points back at that member, and a memberwise move aims the new
// object's cursors at the old object's graph. Every cached cursor is keyed by
// the vertex it walks from, so relocation *re-asks the new graph* for each
// frame's range and the consumed counter restores the position -- which is
// what makes the move sound where a defaulted memberwise move is not.
GTEST_TEST(api_review, algorithms_caching_ranges_rebase_on_relocation) {
    static_digraph static_g;
    mutable_digraph mutable_g;

    // Move-only, and uniformly so: left to the members, copyability would
    // depend on the algorithm, on whether a view sat in between, and on
    // whether the container underneath hands out std-borrowed incidence
    // ranges -- depth_first_search over a subgraph of a static_digraph not
    // copyable while the same over a mutable_digraph is. One answer for every
    // combination.
    using dfs_static = decltype(depth_first_search(static_g, 0u));
    using dfs_mutable = decltype(depth_first_search(mutable_g, 0u));
    using dfs_reverse =
        decltype(depth_first_search(views::reverse(static_g), 0u));
    using scc_static = decltype(strongly_connected_components(static_g));
    static_assert(std::movable<dfs_static> && !std::copyable<dfs_static>);
    static_assert(std::movable<dfs_mutable> && !std::copyable<dfs_mutable>);
    static_assert(std::movable<dfs_reverse> && !std::copyable<dfs_reverse>);
    static_assert(std::movable<scc_static> && !std::copyable<scc_static>);

    using sub = subgraph_view<graph_ref_view<static_digraph>,
                              mapping_ref_view<static_map<unsigned, bool>>,
                              maps::true_map>;
    using sub_dfs = decltype(depth_first_search(std::declval<sub &>(), 0u));
    static_assert(std::movable<sub_dfs> && !std::copyable<sub_dfs>);

    // And the relocations are sound mid-run: interleave a move construction
    // and a move assignment into a traversal and it yields exactly what an
    // undisturbed one does.
    auto builder = static_digraph_builder<static_digraph>(8);
    builder.add_arc(0u, 1u)
        .add_arc(0u, 2u)
        .add_arc(1u, 3u)
        .add_arc(2u, 3u)
        .add_arc(3u, 4u)
        .add_arc(4u, 5u)
        .add_arc(5u, 6u)
        .add_arc(6u, 7u);
    auto [g] = std::move(builder).build();
    auto filter = static_map<unsigned, bool>(8u, true);

    std::vector<unsigned> expected;
    for(const auto & v : depth_first_search(g, 0u)) expected.push_back(v);

    auto dfs = depth_first_search(
        views::subgraph(g, mapping_ref_view(filter), maps::true_map{}), 0u);
    std::vector<unsigned> visited;
    visited.push_back(dfs.current());
    dfs.advance();
    auto moved = std::move(dfs);  // relocate mid-run: move construction
    visited.push_back(moved.current());
    moved.advance();
    // relocate mid-run again, this time onto a live object: move assignment
    auto reassigned = depth_first_search(
        views::subgraph(g, mapping_ref_view(filter), maps::true_map{}), 7u);
    reassigned = std::move(moved);
    while(!reassigned.finished()) {
        visited.push_back(reassigned.current());
        reassigned.advance();
    }
    EXPECT_EQ(visited, expected);
}

// views::undirect promises its ranges survive the view being relocated, so
// its incidence lambdas must not capture `this`.
GTEST_TEST(api_review, undirect_ranges_do_not_capture_the_view) {
    using RV = graph_ref_view<static_digraph>;
    using OWN = graph_owning_view<static_digraph>;
    static_assert(melon::enable_borrowed_graph<undirect_view<RV>>);
    // Owning its graph, it cannot hand out a copy, and says so.
    static_assert(!melon::enable_borrowed_graph<undirect_view<OWN>>);
    static_assert(melon::undirected_graph<undirect_view<OWN>>);

    static_digraph_builder<static_digraph> b(3);
    b.add_arc(0u, 1u).add_arc(1u, 2u);
    auto [g] = b.build();

    using U = undirect_view<RV>;
    auto held = std::make_unique<U>(views::undirect(g));
    auto range = melon::incidence(*held, 1u);

    std::vector<std::pair<unsigned, unsigned>> expected;
    for(auto && e : range) expected.emplace_back(e.first, e.second);

    U relocated = *held;  // a copy the trait licenses
    held.reset();         // the object this-capturing lambdas would point at

    std::vector<std::pair<unsigned, unsigned>> got;
    for(auto && e : melon::incidence(relocated, 1u))
        got.emplace_back(e.first, e.second);
    EXPECT_EQ(got, expected);
}

////////////////////////////////////////////////////////////////////////////////
// owned (rvalue) graphs and ranges are accepted wherever references are
////////////////////////////////////////////////////////////////////////////////

// induced_subgraph over an *rvalue* range: std::views::all_t of a temporary
// container is a move-only owning_view, so a vertices() that returns the
// member by value is ill-formed and the whole type silently stops modelling
// graph.
GTEST_TEST(api_review, induced_subgraph_accepts_an_owned_vertex_range) {
    static_digraph_builder<static_digraph> b(4);
    b.add_arc(0u, 1u).add_arc(1u, 2u).add_arc(2u, 3u);
    auto [g] = b.build();

    auto sub = views::induced_subgraph(g, std::vector<unsigned>{0u, 1u, 2u});
    static_assert(graph<decltype(sub)>,
                  "induced_subgraph over an owned range must still be a graph");

    std::vector<unsigned> seen;
    for(auto && v : melon::vertices(sub)) seen.push_back(v);
    EXPECT_EQ(seen, (std::vector<unsigned>{0u, 1u, 2u}));
}

// Storing the graph view twice -- once in traversal_forest itself, once
// inside its breadth_first_search -- builds the second from the first as an
// lvalue, which needs a copy graph_owning_view does not have.
GTEST_TEST(api_review, traversal_forest_accepts_an_owned_graph) {
    static_digraph_builder<static_digraph> b(4);
    b.add_arc(0u, 1u).add_arc(2u, 3u);
    auto [g] = b.build();

    traversal_forest tf(std::move(g));
    std::size_t trees = 0;
    for(; !tf.finished(); tf.advance()) ++trees;
    EXPECT_EQ(trees, 2u) << "two components, two trees";
}

// The re-seeding idiom detail::consumable_input_view exists for --
// `_remaining_out_arcs[u] = out_arcs(_graph, u)` -- is a prvalue, which an
// `operator=(R &)` can never bind.
GTEST_TEST(api_review, consumable_cursor_is_assignable_from_a_prvalue_range) {
    static_digraph_builder<static_digraph> b(3);
    b.add_arc(0u, 1u).add_arc(0u, 2u);
    auto [g] = b.build();
    auto arc_filter = g.create_arc_map<bool>(true);
    auto sub = views::subgraph(g, maps::true_map{}, arc_filter);

    using range = out_arcs_range_t<decltype(sub)>;
    static_assert(!std::ranges::borrowed_range<range>,
                  "a filtered incidence range lands on the owning cursor");

    detail::consumable_input_view<range> cursor(melon::out_arcs(sub, 0u));
    cursor.advance();
    ASSERT_FALSE(cursor.empty());
    cursor = melon::out_arcs(sub, 0u);  // the prvalue assignment is the pin
    EXPECT_FALSE(cursor.empty());
}

// Without its constructible_from guard, views::graph_all hard-errors for an
// lvalue move-only view instead of picking a branch.
GTEST_TEST(api_review, graph_all_is_sfinae_friendly) {
    using OWN = graph_owning_view<static_digraph>;
    static_assert(requires(OWN & v) { views::graph_all(v); });
    // Ref for lvalues, ownership for rvalues -- the rule the library follows.
    static_assert(std::same_as<views::graph_all_t<OWN &>, graph_ref_view<OWN>>);
    static_assert(std::same_as<views::graph_all_t<OWN>, OWN>);
    static_assert(std::same_as<views::graph_all_t<static_digraph &>,
                               graph_ref_view<static_digraph>>);
    SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// views forward the whole graph protocol, not just the easy accessors
////////////////////////////////////////////////////////////////////////////////

namespace {
// A graph whose *only* arc protocol is arcs_entries -- the protocol `graph` is
// defined in terms of. It has no arc_source / arc_target, so nothing can
// synthesise the entries back for it.
struct entries_only_digraph {
    std::vector<std::pair<unsigned, unsigned>> ends;

    auto vertices() const { return std::views::iota(0u, 3u); }
    auto num_vertices() const { return 3u; }
    auto arcs() const {
        return std::views::iota(0u, static_cast<unsigned>(ends.size()));
    }
    auto arcs_entries() const {
        return std::views::transform(
            arcs(), [this](unsigned a) { return std::make_pair(a, ends[a]); });
    }
    template <typename T>
    auto create_vertex_map() const {
        return melon::static_map<unsigned, T>(3);
    }
    template <typename T>
    auto create_vertex_map(const T & d) const {
        return melon::static_map<unsigned, T>(3, d);
    }
};
}  // namespace

// If graph_forwarding_interface forwards the other accessors but not
// arcs_entries, views::graph_all -- which every algorithm and every view
// calls -- turns a graph of the shape above into a non-graph.
GTEST_TEST(api_review, views_do_not_drop_arcs_entries) {
    static_assert(melon::graph<entries_only_digraph>);
    static_assert(melon::graph<graph_ref_view<entries_only_digraph>>);
    static_assert(melon::graph<views::graph_all_t<entries_only_digraph &>>);

    // And where the container has its own, the view keeps it rather than
    // falling back to a transform over arc_source / arc_target.
    using MD = mutable_digraph;
    static_assert(
        std::same_as<decltype(melon::arcs_entries(std::declval<const MD &>())),
                     decltype(melon::arcs_entries(
                         std::declval<const graph_ref_view<MD> &>()))>);
    SUCCEED();
}

namespace {
// arcs_entries with the wrong shape: flat (arc, source, target) triples
// instead of an (arc, (source, target)) entry.
struct flat_entries_digraph {
    auto vertices() const { return std::views::iota(0u, 3u); }
    auto arcs() const { return std::views::iota(0u, 2u); }
    auto arcs_entries() const {
        return std::views::transform(
            arcs(), [](unsigned a) { return std::make_tuple(a, a, a + 1u); });
    }
};
// The same wrong shape, plus the accessors the CPO can synthesise correct
// entries from.
struct flat_entries_with_endpoints_digraph : flat_entries_digraph {
    auto arc_source(unsigned a) const { return a; }
    auto arc_target(unsigned a) const { return a + 1u; }
};
// The right shape carrying foreign element types.
struct string_entries_digraph {
    auto vertices() const { return std::views::iota(0u, 3u); }
    auto arcs() const { return std::views::iota(0u, 2u); }
    auto arcs_entries() const {
        return std::views::transform(arcs(), [](unsigned a) {
            return std::make_pair(
                a, std::make_pair(std::string("u"), std::string("v")));
        });
    }
};
}  // namespace

// A wrong-shaped arcs_entries member is not the protocol -- the way a member
// begin() returning a non-iterator is invisible to std::ranges::begin. Without
// this pin the type above satisfies `graph` and the shape mismatch surfaces as
// a template avalanche inside the transform lambda of whichever view or
// algorithm first touches an entry.
GTEST_TEST(api_review, arcs_entries_entry_shape_is_pinned) {
    static_assert(!melon::graph<flat_entries_digraph>);
    static_assert(!melon::cpo::has_own_arcs_entries<flat_entries_digraph>);

    // With endpoint accessors present the misshapen member is bypassed, not
    // forwarded: the graph stays a `graph` through the synthesised entries.
    static_assert(melon::graph<flat_entries_with_endpoints_digraph>);
    static_assert(
        !melon::cpo::has_own_arcs_entries<flat_entries_with_endpoints_digraph>);
    static_assert(
        std::same_as<
            std::ranges::range_value_t<decltype(melon::arcs_entries(
                std::declval<const flat_entries_with_endpoints_digraph &>()))>,
            std::pair<unsigned, std::pair<unsigned, unsigned>>>);

    // Detection is shape-only, so entries of foreign element types are still
    // the graph's own protocol -- a vertex-filtered subgraph of an
    // entries-only graph must keep its member detectable with no arcs() route
    // (subgraph_keeps_arcs_entries below). Type coherence with arc_t /
    // vertex_t is `graph`'s half of the check.
    static_assert(melon::cpo::has_own_arcs_entries<string_entries_digraph>);
    static_assert(!melon::graph<string_entries_digraph>);
    SUCCEED();
}

// reverse crosses the endpoints over, so it must not inherit the base's
// straight-through arcs_entries.
GTEST_TEST(api_review, reverse_crosses_over_arcs_entries) {
    mutable_digraph g;
    const auto a = g.create_vertex();
    const auto b = g.create_vertex();
    const auto c = g.create_vertex();
    (void)g.create_arc(a, b);
    (void)g.create_arc(b, c);

    auto r = views::reverse(g);
    for(auto && [arc, ends] : melon::arcs_entries(r)) {
        EXPECT_EQ(ends.first, melon::arc_source(r, arc));
        EXPECT_EQ(ends.second, melon::arc_target(r, arc));
    }
    // Really reversed with respect to the graph underneath.
    for(auto && [arc, ends] : melon::arcs_entries(g)) {
        EXPECT_EQ(ends.second, melon::arc_source(r, arc));
        EXPECT_EQ(ends.first, melon::arc_target(r, arc));
    }
}

// A view forwards the validity *question* without forwarding the mutation, so
// views::subgraph can ask it. Gating the branch on has_vertex_removal, which
// no view satisfies, leaves it dead: a vertex removed from the graph comes
// back valid from its subgraph while vertices() has already stopped listing
// it.
GTEST_TEST(api_review, subgraph_sees_removals_in_the_graph_underneath) {
    using MD = mutable_digraph;
    using RV = graph_ref_view<MD>;
    static_assert(melon::has_is_valid_vertex<MD> &&
                  melon::has_is_valid_vertex<RV>);
    static_assert(melon::has_is_valid_arc<MD> && melon::has_is_valid_arc<RV>);
    static_assert(melon::has_vertex_removal<MD> &&
                  !melon::has_vertex_removal<RV>);

    MD g;
    const auto a = g.create_vertex();
    const auto b = g.create_vertex();
    const auto c = g.create_vertex();
    (void)g.create_arc(a, b);

    auto sg = views::subgraph(g);
    g.remove_vertex(c);

    EXPECT_FALSE(melon::is_valid_vertex(g, c));
    EXPECT_FALSE(sg.is_valid_vertex(c))
        << "subgraph must not call a removed vertex valid";
    EXPECT_TRUE(sg.is_valid_vertex(a));
    EXPECT_TRUE(sg.is_valid_vertex(b));
}

// subgraph can drop arcs_entries the same way graph_forwarding_interface can
// (views_do_not_drop_arcs_entries above): a filterless subgraph of an
// entries-only graph then stops modeling `graph` at all, and a graph with its
// own arcs_entries gets the synthesized fallback. And since an entry names the
// arc and both endpoints -- everything the filters need -- a *filtered*
// subgraph filters the base's own entries too, which is the only possible
// protocol for a graph with no endpoint accessors. (An arc-filtered subgraph
// of such a graph is therefore a full `graph` again; a vertex-filtered one
// still is not, because `arcs` itself has no entries-based fallback -- with
// no incidence to join, has_arcs cannot hold.)
GTEST_TEST(api_review, subgraph_keeps_arcs_entries) {
    using EOSub =
        decltype(views::subgraph(std::declval<entries_only_digraph &>()));
    static_assert(melon::graph<EOSub>);
    static_assert(melon::cpo::has_own_arcs_entries<EOSub>);
    using EOArcFiltered = decltype(views::subgraph(
        std::declval<entries_only_digraph &>(), std::declval<maps::true_map>(),
        std::declval<static_map<unsigned, bool> &>()));
    static_assert(melon::graph<EOArcFiltered>);

    using MD = mutable_digraph;
    using MDSub = decltype(views::subgraph(std::declval<MD &>()));
    static_assert(
        std::same_as<decltype(melon::arcs_entries(std::declval<const MD &>())),
                     decltype(melon::arcs_entries(
                         std::declval<const MDSub &>()))>);

    entries_only_digraph g{{{0u, 1u}, {1u, 2u}, {2u, 0u}}};
    auto vfilter = static_map<unsigned, bool>(3u, true);
    auto afilter = static_map<unsigned, bool>(3u, true);
    auto sub = views::subgraph(g, vfilter, afilter);

    afilter[0u] = false;  // drops arc 0
    vfilter[2u] = false;  // drops arcs 1 and 2, both touching vertex 2
    EXPECT_TRUE(std::ranges::empty(melon::arcs_entries(sub)));

    vfilter[2u] = true;
    std::vector<unsigned> kept;
    for(auto && [a, ends] : melon::arcs_entries(sub)) kept.push_back(a);
    EXPECT_EQ(kept, (std::vector<unsigned>{1u, 2u}));
}

// The counts a filter cannot change forward with the base's noexcept; the
// moment a filter could change the answer, the member must vanish rather than
// lie (and mutable_digraph's non-sized arcs range means no fallback resurrects
// it).
GTEST_TEST(api_review, subgraph_forwards_what_no_filter_can_change) {
    using MD = mutable_digraph;
    using MDSub = decltype(views::subgraph(std::declval<MD &>()));
    static_assert(melon::has_num_arcs<MD>);
    static_assert(melon::has_num_arcs<MDSub>);

    using MDFiltered = decltype(views::subgraph(
        std::declval<MD &>(), std::declval<maps::true_map>(),
        std::declval<arc_map_t<MD, bool> &>()));
    static_assert(!melon::has_num_arcs<MDFiltered>);

    using SD = static_digraph;
    using SDSub = decltype(views::subgraph(std::declval<SD &>()));
    static_assert(melon::has_out_degree<SDSub> && melon::has_in_degree<SDSub>);
    static_assert(noexcept(std::declval<const SDSub &>().num_arcs()));
    SUCCEED();
}

// borrowed_graph.hpp names subgraph's captured-`this` filters as the reason it
// cannot be borrowed -- but with no filters every range forwards straight
// through, so the view is borrowed exactly when the wrapped one is. What that
// buys downstream: a traversal over subgraph(g) relocates memberwise and
// nothrow again, like a traversal over g itself, instead of running the
// cursor-rebase loop.
GTEST_TEST(api_review, filterless_subgraph_is_borrowed) {
    using SD = static_digraph;
    using RefSub = decltype(views::subgraph(std::declval<SD &>()));
    static_assert(melon::borrowed_graph<RefSub>);
    // An owning subgraph embeds the graph: its ranges point into the view.
    using OwnSub = decltype(views::subgraph(std::declval<SD>()));
    static_assert(!melon::borrowed_graph<OwnSub>);
    // A filter puts `this` back into every range.
    using Filtered = decltype(views::subgraph(
        std::declval<SD &>(), std::declval<maps::true_map>(),
        std::declval<arc_map_t<SD, bool> &>()));
    static_assert(!melon::borrowed_graph<Filtered>);

    using MD = mutable_digraph;
    using DFSDirect = decltype(depth_first_search(std::declval<MD &>()));
    using DFSSub =
        decltype(depth_first_search(views::subgraph(std::declval<MD &>())));
    static_assert(std::is_nothrow_move_constructible_v<DFSDirect>);
    static_assert(std::is_nothrow_move_constructible_v<DFSSub>);
    SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// noexcept specifications are honest in both directions
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(api_review, subgraph_does_not_lie_about_noexcept) {
    static_digraph g;
    auto sub = views::subgraph(g);

    static_assert(!noexcept(sub.create_vertex_map<double>()),
                  "it allocates a static_map");
    static_assert(!noexcept(sub.create_arc_map<double>()),
                  "it allocates a static_map");
    static_assert(!noexcept(melon::create_vertex_map<double>(sub)),
                  "and the CPO must not launder the lie");
    static_assert(!noexcept(sub.vertices()), "it builds a filter_view");

    // Positive control: the operations that really are nothrow still say so.
    static_assert(noexcept(melon::vertices(g)));
    SUCCEED();
}

// num_vertices carries the same conditional specification as its num_arcs /
// num_edges siblings on every view.
GTEST_TEST(api_review, views_keep_the_num_vertices_guarantee) {
    static_digraph g;
    auto ref = graph_ref_view(g);
    auto rev = views::reverse(g);
    auto und = views::undirect(g);

    static_assert(noexcept(ref.num_vertices()) && noexcept(ref.num_arcs()));
    static_assert(noexcept(rev.num_vertices()) && noexcept(rev.num_arcs()));
    static_assert(noexcept(und.num_vertices()) && noexcept(und.num_edges()));
    SUCCEED();
}

// A private is_noexcept() that re-runs the operator()'s `if constexpr` in a
// different place drifts from it -- in undirected_graph.hpp the specification
// then comes from an overload the operator never calls. The CPOs keep the
// noexcept beside the expression it measures, one constrained overload per
// protocol. This pins both directions.
namespace {
struct throwing_graph {
    std::vector<unsigned> _v{0u, 1u};
    std::vector<std::pair<unsigned, unsigned>> _a{{0u, 1u}};

    auto vertices() const noexcept(false) { return std::views::all(_v); }
    auto arcs() const noexcept(false) {
        return std::views::iota(0u, static_cast<unsigned>(_a.size()));
    }
    unsigned arc_source(unsigned a) const noexcept(false) {
        return _a[a].first;
    }
    unsigned arc_target(unsigned a) const noexcept(false) {
        return _a[a].second;
    }
};
}  // namespace

GTEST_TEST(api_review, cpo_noexcept_follows_the_branch_it_takes) {
    static_digraph honest;
    throwing_graph thrower;

    // static_digraph's accessors are nothrow, and the CPO must not lose that.
    static_assert(noexcept(melon::vertices(honest)));
    static_assert(noexcept(melon::arc_source(honest, 0u)));
    static_assert(noexcept(melon::num_vertices(honest)));

    // The same CPOs over a graph whose members can throw must not claim it.
    static_assert(!noexcept(melon::vertices(thrower)));
    static_assert(!noexcept(melon::arc_source(thrower, 0u)));
    static_assert(!noexcept(melon::arc_target(thrower, 0u)));

    // num_vertices has no member here, so it takes the sized-range fallback --
    // whose specification must come from that expression, not from a member
    // that does not exist.
    static_assert(!noexcept(melon::num_vertices(thrower)));

    // And a view over each must report the same as what it wraps.
    auto honest_view = graph_ref_view(honest);
    auto throwing_view = graph_ref_view(thrower);
    static_assert(noexcept(honest_view.vertices()));
    static_assert(!noexcept(throwing_view.vertices()));
    SUCCEED();
}

// current() returns the vertex by value, and the copy that return performs is
// part of what its noexcept must measure: with a throwing-copy vertex type a
// nothrow claim turns the first throw into std::terminate. Measuring only the
// reach of the element, or carrying no specification at all, both make that
// claim.
namespace {
struct throwing_copy_vertex {
    unsigned id = 0;
    throwing_copy_vertex() = default;
    throwing_copy_vertex(unsigned i) : id(i) {}
    throwing_copy_vertex(const throwing_copy_vertex & o) noexcept(false)
        : id(o.id) {}
    throwing_copy_vertex(throwing_copy_vertex &&) noexcept = default;
    throwing_copy_vertex & operator=(const throwing_copy_vertex &) noexcept(
        false) {
        return *this;
    }
    throwing_copy_vertex & operator=(throwing_copy_vertex &&) noexcept =
        default;
    bool operator==(const throwing_copy_vertex &) const = default;
};

template <typename T>
struct throwing_vertex_map {
    std::vector<T> data;
    decltype(auto) operator[](const throwing_copy_vertex & v) {
        return data[v.id];
    }
    decltype(auto) operator[](const throwing_copy_vertex & v) const {
        return data[v.id];
    }
};

struct throwing_vertex_graph {
    auto vertices() const noexcept {
        return std::views::transform(
            std::views::iota(0u, 2u),
            [](unsigned i) noexcept { return throwing_copy_vertex{i}; });
    }
    auto arcs() const noexcept { return std::views::iota(0u, 1u); }
    throwing_copy_vertex arc_source(unsigned) const noexcept { return {0u}; }
    throwing_copy_vertex arc_target(unsigned) const noexcept { return {1u}; }
    auto out_arcs(const throwing_copy_vertex & v) const noexcept {
        return std::views::iota(0u, v.id == 0u ? 1u : 0u);
    }
    auto in_arcs(const throwing_copy_vertex & v) const noexcept {
        return std::views::iota(0u, v.id == 1u ? 1u : 0u);
    }
    template <typename T>
    auto create_vertex_map() const {
        return throwing_vertex_map<T>{std::vector<T>(2)};
    }
    template <typename T>
    auto create_vertex_map(const T & d) const {
        return throwing_vertex_map<T>{std::vector<T>(2, d)};
    }
};
}  // namespace

GTEST_TEST(api_review, current_noexcept_measures_the_returned_copy) {
    using TG = views::graph_all_t<throwing_vertex_graph &>;
    static_assert(!std::is_nothrow_copy_constructible_v<throwing_copy_vertex>);
    static_assert(
        !noexcept(std::declval<const depth_first_search<TG> &>().current()));
    static_assert(
        !noexcept(std::declval<const breadth_first_search<TG> &>().current()));
    static_assert(
        !noexcept(std::declval<const topological_sort<TG> &>().current()));

    // Positive control: a nothrow-copy vertex keeps the guarantee, and
    // dijkstra reports it like the rest.
    using G = views::graph_all_t<static_digraph &>;
    using L =
        maps::mapping_all_t<static_map<arc_t<static_digraph>, unsigned> &>;
    static_assert(
        noexcept(std::declval<const depth_first_search<G> &>().current()));
    static_assert(
        noexcept(std::declval<const breadth_first_search<G> &>().current()));
    static_assert(
        noexcept(std::declval<const topological_sort<G> &>().current()));
    static_assert(noexcept(std::declval<const dijkstra<G, L> &>().current()));
    SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// CTAD deduces exactly the type the user would have spelled out
////////////////////////////////////////////////////////////////////////////////

// A deduction guide that defaults Traits computes it over the *deduced* graph
// type -- a reference -- while the class computes it over graph_all_t, so the
// two would name different specialisations of <algo>_default_traits.
GTEST_TEST(api_review, deduced_type_equals_the_spelled_out_one) {
    static_digraph g;
    std::vector<double> length_map;

    using graph_view = views::graph_all_t<static_digraph &>;
    using map_view = maps::mapping_all_t<std::vector<double> &>;

    static_assert(std::same_as<decltype(dijkstra(g, length_map)),
                               dijkstra<graph_view, map_view>>);
    static_assert(std::same_as<decltype(network_voronoi(g, length_map)),
                               network_voronoi<graph_view, map_view>>);
    static_assert(
        std::same_as<decltype(competing_dijkstras(g, length_map, length_map)),
                     competing_dijkstras<graph_view, map_view, map_view>>);
    static_assert(
        std::same_as<decltype(biobjective_dijkstra(g, length_map, length_map)),
                     biobjective_dijkstra<graph_view, map_view, map_view>>);
    SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// the whole family shares one API shape, modelled on std::ranges
////////////////////////////////////////////////////////////////////////////////

namespace {
template <typename A>
concept run_returns_self = requires(A a) {
    { a.run() } -> std::same_as<A &>;
};
}  // namespace

// run() returns Algo & family-wide, matching reset() -- a void run() breaks
// the `alg.run().dist()` idiom, and only for some family members.
GTEST_TEST(api_review, run_returns_the_algorithm) {
    static_digraph g;
    std::vector<double> length_map;

    static_assert(run_returns_self<decltype(dijkstra(g, length_map))>);
    static_assert(run_returns_self<decltype(network_voronoi(g, length_map))>);
    static_assert(run_returns_self<decltype(competing_dijkstras(g, length_map,
                                                                length_map))>);
    static_assert(run_returns_self<decltype(traversal_forest(g))>);
    SUCCEED();
}

// The default value of a create_*_map goes in by const reference everywhere,
// like the CPO that forwards it -- four view members took it by value.
namespace {
struct counted {
    static inline int copies = 0;
    counted() = default;
    counted(const counted &) { ++copies; }
    counted & operator=(const counted &) {
        ++copies;
        return *this;
    }
};
}  // namespace

GTEST_TEST(api_review, create_map_defaults_are_not_copied_twice) {
    static_digraph_builder<static_digraph> b(2);
    b.add_arc(0u, 1u);
    auto [g] = b.build();

    const counted seed;
    counted::copies = 0;
    auto direct = melon::create_vertex_map<counted>(g, seed);
    const int by_container = counted::copies;

    auto ref = graph_ref_view(g);
    counted::copies = 0;
    auto through_view = melon::create_vertex_map<counted>(ref, seed);
    const int by_view = counted::copies;

    EXPECT_EQ(by_view, by_container)
        << "the view must not add a copy of the default value";
}

namespace {
template <typename T>
concept has_const_base = requires(const T & t) { t.base(); };
template <typename T>
concept has_rvalue_base = requires(T && t) { std::move(t).base(); };
template <typename T>
concept has_traversal = requires(const T & t) { t.traversal(); };

// Both flags off: whichever specialisation a graph selects, traversal() must
// be absent.
struct plain_bfs_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
    static constexpr bool store_traversal_range = false;
};
struct branchless_traversal_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
    static constexpr bool store_traversal_range = true;
};
struct generic_traversal_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = true;  // forces the generic one
    static constexpr bool store_traversal_range = true;
};

template <typename A>
concept reports_reached =
    requires(const A & a, melon::vertex_t<static_digraph> v) {
        { a.reached(v) } -> std::convertible_to<bool>;
        { a.reached_map()[v] } -> std::convertible_to<bool>;
    };
}  // namespace

// Three shapes, each matching the std::ranges view it corresponds to:
// ref_view (one overload, reference), owning_view (four, ref-qualified) and
// the adaptors (a copy of the adapted view).
GTEST_TEST(api_review, base_follows_the_std_ranges_shapes) {
    using SD = static_digraph;
    using RV = graph_ref_view<SD>;
    using OWN = graph_owning_view<SD>;

    // std::ranges::ref_view
    static_assert(
        std::same_as<decltype(std::declval<const RV &>().base()), SD &>);
    // std::ranges::owning_view
    static_assert(std::same_as<decltype(std::declval<OWN &>().base()), SD &>);
    static_assert(
        std::same_as<decltype(std::declval<const OWN &>().base()), const SD &>);
    static_assert(std::same_as<decltype(std::declval<OWN>().base()), SD &&>);
    static_assert(
        std::same_as<decltype(std::declval<const OWN>().base()), const SD &&>);
    // std::ranges::filter_view: a copy of the adapted view, and no const &
    // overload when that copy is impossible.
    using REV = reverse_view<RV>;
    static_assert(
        std::same_as<decltype(std::declval<const REV &>().base()), RV>);
    static_assert(std::same_as<decltype(std::declval<REV>().base()), RV>);
    static_assert(!has_const_base<reverse_view<OWN>>);
    static_assert(has_rvalue_base<reverse_view<OWN>>);
    static_assert(
        has_const_base<subgraph_view<RV, maps::true_map, maps::true_map>>);
    static_assert(has_const_base<undirect_view<RV>>);
    SUCCEED();
}

// Both specialisations honour store_traversal_range and return the same type
// for the same member -- a flag silently ignored by one of them drifts the
// two APIs apart.
GTEST_TEST(api_review, both_bfs_specialisations_expose_the_same_api) {
    using RV = graph_ref_view<static_digraph>;
    using vertex = melon::vertex_t<static_digraph>;

    static_assert(!has_traversal<breadth_first_search<RV, plain_bfs_traits>>);
    static_assert(
        has_traversal<breadth_first_search<RV, branchless_traversal_traits>>);
    static_assert(
        has_traversal<breadth_first_search<RV, generic_traversal_traits>>);

    // One return type, and read-only: the window is rewritten by advance().
    static_assert(
        std::same_as<decltype(std::declval<const breadth_first_search<
                                  RV, branchless_traversal_traits> &>()
                                  .traversal()),
                     std::span<const vertex>>);
    static_assert(std::same_as<decltype(std::declval<const breadth_first_search<
                                            RV, generic_traversal_traits> &>()
                                            .traversal()),
                               std::span<const vertex>>);

    // current() by value in both, like every other algorithm's.
    static_assert(std::same_as<decltype(std::declval<const breadth_first_search<
                                            RV, plain_bfs_traits> &>()
                                            .current()),
                               vertex>);
    static_assert(std::same_as<decltype(std::declval<const breadth_first_search<
                                            RV, generic_traversal_traits> &>()
                                            .current()),
                               vertex>);
    SUCCEED();
}

// reached_map() accompanies reached() on every algorithm that answers it.
GTEST_TEST(api_review, reached_map_accompanies_reached) {
    using RV = graph_ref_view<static_digraph>;
    using LM = maps::mapping_all_t<std::vector<double> &>;

    static_assert(reports_reached<breadth_first_search<RV, plain_bfs_traits>>);
    static_assert(reports_reached<depth_first_search<RV>>);
    static_assert(reports_reached<topological_sort<RV>>);
    static_assert(reports_reached<dijkstra<RV, LM>>);
    static_assert(reports_reached<network_voronoi<RV, LM>>);
    static_assert(reports_reached<strongly_connected_components<RV>>);
    static_assert(reports_reached<
                  traversal_forest<RV, vertices_range_t<static_digraph>>>);

    // And it agrees with reached() at runtime.
    static_digraph_builder<static_digraph> b(4);
    b.add_arc(0u, 1u).add_arc(1u, 2u);
    auto [g] = b.build();
    // run() returns *this by reference; algorithms are move-only, so the
    // result is bound, not copied out.
    auto ts = topological_sort(g);
    ts.run();
    for(const auto & v : melon::vertices(g))
        EXPECT_EQ(ts.reached_map()[v], ts.reached(v));
}

// The reached()/reached_map() rule generalised: every per-key accessor over a
// stored map is accompanied by a pluralised *s_map() view of that map --
// flow()/flows_map() on the flow algorithms, dist()/dists_map(),
// cluster()/clusters_map(), component_id()/component_ids_map(), and the
// pred/depth accessors on the traversals. dijkstra's pred maps stay
// deliberately unexposed: they store std::optional<arc>, not the arc that
// pred_arc() answers, and path_to() already covers that use.
namespace {
struct full_bfs_traits {
    static constexpr bool store_pred_vertices = true;
    static constexpr bool store_pred_arcs = true;
    static constexpr bool store_distances = true;
    static constexpr bool store_traversal_range = false;
};
struct full_dfs_traits {
    static constexpr bool store_pred_vertices = true;
    static constexpr bool store_pred_arcs = true;
    static constexpr bool store_depth = true;
};
struct distance_dijkstra_traits
    : dijkstra_default_traits<graph_ref_view<static_digraph>, double> {
    static constexpr bool store_distances = true;
};
struct full_voronoi_traits
    : network_voronoi_default_traits<graph_ref_view<static_digraph>, double> {
    static constexpr bool store_distances = true;
    static constexpr bool store_clusters = true;
};
struct ids_scc_traits {
    static constexpr bool store_component_ids = true;
};
}  // namespace

GTEST_TEST(api_review, stored_maps_are_exposed_as_map_views) {
    using RV = graph_ref_view<static_digraph>;
    using LM = maps::mapping_all_t<std::vector<double> &>;
    using CM = maps::mapping_all_t<std::vector<int> &>;
    using V = vertex_t<static_digraph>;
    using A = arc_t<static_digraph>;

    static_assert(requires(const dinitz<RV, CM> & alg, const A & a) {
        { alg.flow(a) } -> std::convertible_to<int>;
        { alg.flows_map()[a] } -> std::convertible_to<int>;
    });
    static_assert(requires(const edmonds_karp<RV, CM> & alg, const A & a) {
        { alg.flow(a) } -> std::convertible_to<int>;
        { alg.flows_map()[a] } -> std::convertible_to<int>;
    });
    static_assert(requires(const network_simplex<RV, CM, CM, CM> & alg,
                           const A & a, const V & v) {
        { alg.flow(a) } -> std::convertible_to<int>;
        { alg.flows_map()[a] } -> std::convertible_to<int>;
        { alg.potential(v) } -> std::convertible_to<int>;
        { alg.potentials_map()[v] } -> std::convertible_to<int>;
    });
    static_assert(requires(
        const dijkstra<RV, LM, distance_dijkstra_traits> & alg, const V & v) {
        { alg.dists_map()[v] } -> std::convertible_to<double>;
    });
    static_assert(requires(
        const network_voronoi<RV, LM, full_voronoi_traits> & alg, const V & v) {
        { alg.dists_map()[v] } -> std::convertible_to<double>;
        { alg.clusters_map()[v] } -> std::convertible_to<V>;
    });
    static_assert(requires(
        const breadth_first_search<RV, full_bfs_traits> & alg, const V & v) {
        { alg.pred_vertices_map()[v] } -> std::convertible_to<V>;
        { alg.pred_arcs_map()[v] } -> std::convertible_to<A>;
        { alg.dists_map()[v] } -> std::convertible_to<int>;
    });
    static_assert(requires(const depth_first_search<RV, full_dfs_traits> & alg,
                           const V & v) {
        { alg.pred_vertices_map()[v] } -> std::convertible_to<V>;
        { alg.pred_arcs_map()[v] } -> std::convertible_to<A>;
        { alg.depths_map()[v] } -> std::convertible_to<int>;
    });
    static_assert(
        requires(const strongly_connected_components<RV, ids_scc_traits> & alg,
                 const V & v) {
            { alg.component_ids_map()[v] } -> std::convertible_to<V>;
        });

    // And the view agrees with the accessor at runtime.
    static_digraph_builder<static_digraph, int> b(4);
    b.add_arc(0u, 1u, 3).add_arc(0u, 2u, 2).add_arc(1u, 3u, 2).add_arc(2u, 3u,
                                                                       3);
    auto [g, capacity] = b.build();
    auto alg = dinitz(g, capacity, 0u, 3u);
    alg.run();
    for(const auto & a : melon::arcs(g))
        EXPECT_EQ(alg.flows_map()[a], alg.flow(a));
}

// Map accessors follow std::views::all's ref-or-owning split: an lvalue
// algorithm hands out a mapping_ref_view, an expiring one moves the stored
// map into a mapping_owning_view -- so `std::move(alg).flows_map()` extracts
// the result and outlives the algorithm. Computed reached_map()s (dijkstra,
// network_voronoi, strongly_connected_components, biobjective_dijkstra) have
// no stored bool map; their expiring overload instead moves the backing map
// (status enums, component indices, Pareto fronts) into the returned lambda
// map, so extraction works uniformly across the family.
namespace {
// A concept, not an inline requires-expression: template substitution is
// what turns an invalid body into false -- a non-dependent
// requires-expression checks its body as plain code and hard-errors.
template <typename A>
concept extractable_reached_map =
    requires(A && alg) { std::move(alg).reached_map(); };
}  // namespace

GTEST_TEST(api_review, expiring_map_accessors_extract_the_stored_map) {
    using RV = graph_ref_view<static_digraph>;
    using LM = maps::mapping_all_t<std::vector<double> &>;
    using CM = maps::mapping_all_t<std::vector<int> &>;

    // The type split, on dinitz's flows_map.
    static_assert(std::same_as<
                  decltype(std::declval<const dinitz<RV, CM> &>().flows_map()),
                  mapping_ref_view<const arc_map_t<RV, int>>>);
    static_assert(
        std::same_as<decltype(std::declval<dinitz<RV, CM> &&>().flows_map()),
                     mapping_owning_view<arc_map_t<RV, int>>>);

    // Every reached_map() supports extraction -- the stored ones by moving
    // the map into a mapping_owning_view, the computed ones by moving their
    // backing map into the returned lambda map.
    static_assert(extractable_reached_map<dijkstra<RV, LM>>);
    static_assert(extractable_reached_map<network_voronoi<RV, LM>>);
    static_assert(extractable_reached_map<strongly_connected_components<RV>>);
    static_assert(extractable_reached_map<breadth_first_search<RV>>);
    static_assert(extractable_reached_map<depth_first_search<RV>>);
    static_assert(extractable_reached_map<topological_sort<RV>>);

    // And the owning view survives the algorithm it was extracted from.
    static_digraph_builder<static_digraph, int> b(4);
    b.add_arc(0u, 1u, 3).add_arc(0u, 2u, 2).add_arc(1u, 3u, 2).add_arc(2u, 3u,
                                                                       3);
    auto [g, capacity] = b.build();
    using alg_t = decltype(dinitz(g, capacity, 0u, 3u));
    auto alg = std::make_unique<alg_t>(g, capacity, 0u, 3u);
    alg->run();
    std::vector<int> expected;
    for(const auto & a : melon::arcs(g)) expected.push_back(alg->flow(a));
    auto owned = std::move(*alg).flows_map();
    alg.reset();
    for(const auto & a : melon::arcs(g))
        EXPECT_EQ(owned[a], expected[a]) << "arc " << a;

    // A computed extraction survives its algorithm too: dijkstra's expiring
    // reached_map() owns the moved status map through its lambda.
    std::vector<double> lengths(melon::num_arcs(g), 1.0);
    using dij_t = decltype(dijkstra(g, lengths, 0u));
    auto dij = std::make_unique<dij_t>(g, lengths, 0u);
    dij->run();
    std::vector<bool> expected_reached;
    for(const auto & v : melon::vertices(g))
        expected_reached.push_back(dij->reached(v));
    auto owned_reached = std::move(*dij).reached_map();
    dij.reset();
    for(const auto & v : melon::vertices(g))
        EXPECT_EQ(owned_reached[v], expected_reached[v]) << "vertex " << v;
}

// No CPO returns decltype(auto), so no range alias can name a reference type.
GTEST_TEST(api_review, range_aliases_are_never_references) {
    static_assert(!std::is_reference_v<vertices_range_t<static_digraph>>);
    static_assert(
        !std::is_reference_v<vertices_range_t<graph_ref_view<static_digraph>>>);
    static_assert(!std::is_reference_v<arcs_range_t<static_digraph>>);
    SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// melon's ranges and containers conform to what std expects of them
////////////////////////////////////////////////////////////////////////////////

// An iota built from a ternary mixing `arc` with num_arcs()'s std::size_t
// widens its common type to size_t: a non-common range twice the size of the
// one arcs() produces from explicitly cast ends. The size is
// multiplied by where these cursors live -- one per depth_first_search stack
// frame, one per vertex in each of dinitz's two maps.
GTEST_TEST(api_review, incidence_ranges_are_common_and_narrow) {
    using SD = static_digraph;
    using OA = out_arcs_range_t<SD>;
    using AR = arcs_range_t<SD>;

    static_assert(std::ranges::common_range<OA>);
    static_assert(std::same_as<std::ranges::range_value_t<OA>, arc_t<SD>>);
    static_assert(sizeof(OA) == sizeof(AR));
    static_assert(sizeof(detail::consumable_input_view<OA>) ==
                  sizeof(detail::consumable_input_view<AR>));

    using SFD = static_forward_digraph;
    static_assert(std::ranges::common_range<out_arcs_range_t<SFD>>);
    static_assert(
        std::same_as<std::ranges::range_value_t<out_arcs_range_t<SFD>>,
                     arc_t<SFD>>);
    SUCCEED();
}

// The container interface every standard container has; both maps offer it.
GTEST_TEST(api_review, maps_offer_the_standard_container_interface) {
    using SM = static_map<unsigned, int>;
    using SFM = static_filter_map<unsigned>;

    static_assert(std::ranges::contiguous_range<SM>);
    static_assert(std::ranges::sized_range<SM>);
    static_assert(std::swappable<SM>);
    static_assert(std::ranges::random_access_range<SFM>);
    static_assert(std::swappable<SFM>);

    EXPECT_TRUE(SM().empty());
    EXPECT_TRUE(SFM().empty());

    SM a(3, 1), b(5, 2);
    swap(a, b);  // found by ADL, like every standard container's
    EXPECT_EQ(a.size(), 5u);
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(a[0u], 2);
    EXPECT_EQ(b[0u], 1);
    EXPECT_EQ(std::distance(a.cbegin(), a.cend()), 5);

    // at() is available for writing too, on both.
    SM m(2, 0);
    m.at(1u) = 7;
    EXPECT_EQ(m.at(1u), 7);
    SFM f(2, false);
    f.at(1u) = true;
    EXPECT_TRUE(f.at(1u));
    EXPECT_THROW((void)m.at(9u), std::out_of_range);
    EXPECT_THROW((void)f.at(9u), std::out_of_range);
}

////////////////////////////////////////////////////////////////////////////////
// the range- and closure-returning CPOs reject rvalue graphs whose result
// would dangle: a temporary is admitted only where the graph's borrowed
// promise covers its own handed-out ranges, and never for a synthesized
// result, which captures the graph object's address
////////////////////////////////////////////////////////////////////////////////

namespace rvalue_cpos {
template <typename G>
concept rvalue_vertices = requires { melon::vertices(G{}); };
template <typename G>
concept rvalue_arcs = requires { melon::arcs(G{}); };
template <typename G>
concept rvalue_arcs_entries = requires { melon::arcs_entries(G{}); };
template <typename G>
concept rvalue_out_arcs =
    requires(melon::vertex_t<G> v) { melon::out_arcs(G{}, v); };
template <typename G>
concept rvalue_out_neighbors =
    requires(melon::vertex_t<G> v) { melon::out_neighbors(G{}, v); };
template <typename G>
concept rvalue_arc_targets_map = requires { melon::arc_targets_map(G{}); };
}  // namespace rvalue_cpos

static_assert(!rvalue_cpos::rvalue_vertices<static_digraph>);
static_assert(!rvalue_cpos::rvalue_arcs<static_digraph>);
static_assert(!rvalue_cpos::rvalue_arcs_entries<static_digraph>);
static_assert(!rvalue_cpos::rvalue_out_arcs<static_digraph>);
static_assert(!rvalue_cpos::rvalue_out_neighbors<static_digraph>);
static_assert(!rvalue_cpos::rvalue_arc_targets_map<static_digraph>);
// the borrowed graph with its own members keeps rvalue support
static_assert(rvalue_cpos::rvalue_vertices<views::complete_digraph<>>);
static_assert(rvalue_cpos::rvalue_arcs_entries<views::complete_digraph<>>);
static_assert(rvalue_cpos::rvalue_out_neighbors<views::complete_digraph<>>);

////////////////////////////////////////////////////////////////////////////////
// views::graph_all rejects const rvalues instead of silently deep-copying
// into a graph_owning_view<const G> that is not even a graph_view --
// std::views::all's precedent
////////////////////////////////////////////////////////////////////////////////

namespace graph_all_categories {
template <typename T>
concept accepted = requires(T && t) { views::graph_all(std::forward<T>(t)); };
}  // namespace graph_all_categories
static_assert(graph_all_categories::accepted<static_digraph &>);
static_assert(graph_all_categories::accepted<const static_digraph &>);
static_assert(graph_all_categories::accepted<static_digraph>);
static_assert(!graph_all_categories::accepted<const static_digraph>);

////////////////////////////////////////////////////////////////////////////////
// on an equal-rank incidence tie the synthesized arcs() and arcs_entries()
// join the same direction, so the two CPOs enumerate one order
////////////////////////////////////////////////////////////////////////////////

namespace tiebreak {
struct two_arc_graph {
    auto vertices() const { return std::views::iota(0u, 2u); }
    auto out_arcs(unsigned int u) const {
        return std::views::single(u == 0u ? 0u : 1u);
    }
    auto in_arcs(unsigned int u) const {
        return std::views::single(u == 0u ? 1u : 0u);
    }
    unsigned int arc_source(unsigned int a) const { return a; }
    unsigned int arc_target(unsigned int a) const { return 1u - a; }
};
}  // namespace tiebreak

GTEST_TEST(api_review, synthesized_arcs_and_entries_agree_on_the_tiebreak) {
    static_assert(melon::graph<tiebreak::two_arc_graph>);
    tiebreak::two_arc_graph g;
    ASSERT_TRUE(EQ_RANGES(melon::arcs(g), {0u, 1u}));
    std::vector<unsigned int> entry_arcs;
    for(auto && e : melon::arcs_entries(g))
        entry_arcs.push_back(std::get<0>(e));
    ASSERT_TRUE(EQ_RANGES(entry_arcs, {0u, 1u}));
}

////////////////////////////////////////////////////////////////////////////////
// the creation capabilities accept convertible handle returns, like the
// arc_entries_range_of precedent: a member the CPO dispatches to is not
// invisible to the capability concept
////////////////////////////////////////////////////////////////////////////////

namespace convertible_creation {
struct widening_creation_graph : mutable_digraph {
    unsigned long create_vertex() { return mutable_digraph::create_vertex(); }
};
}  // namespace convertible_creation
static_assert(
    has_vertex_creation<convertible_creation::widening_creation_graph>);

////////////////////////////////////////////////////////////////////////////////
// neighbor member detection probes the element type through
// range_reference_t: a self-contained but non-borrowed member range (a
// filter/concat view) is a member all the same
////////////////////////////////////////////////////////////////////////////////

namespace nonborrowed_neighbors {
struct filtering_graph : static_digraph {
    using static_digraph::static_digraph;
    auto out_neighbors(const unsigned int & u) const {
        return std::views::filter(
            melon::out_neighbors(static_cast<const static_digraph &>(*this), u),
            [](unsigned int) { return true; });
    }
};
}  // namespace nonborrowed_neighbors
static_assert(melon::cpo::has_member_out_neighbors<
              nonborrowed_neighbors::filtering_graph>);

////////////////////////////////////////////////////////////////////////////////
// concept probes stay probes at the boundaries: a graph outside a concept
// answers false instead of hard-erroring during instantiation
////////////////////////////////////////////////////////////////////////////////

namespace probe_safety {
// Move-only arc handles: the synthesized arcs_entries would copy them into
// pairs, so graph<G> is false -- and, the actual pin, *compilably* false: the
// synthesizers' copy_constructible constraints keep the failure out of their
// deduced return types, where no requires-expression could catch it.
struct move_only_arc {
    move_only_arc() = default;
    move_only_arc(move_only_arc &&) = default;
    move_only_arc & operator=(move_only_arc &&) = default;
};
struct move_only_arc_graph {
    auto vertices() const { return std::views::iota(0, 1); }
    auto arcs() const { return std::views::empty<move_only_arc>; }
    int arc_source(const move_only_arc &) const;
    int arc_target(const move_only_arc &) const;
};
static_assert(!melon::graph<move_only_arc_graph>);

// A graph with no map factories: has_vertex_map answers false, and because
// that constraint sits on each algorithm *class* (and its default traits) --
// not on a constructor consulted only after the members hard-error -- CTAD
// and construction probes answer false too.
struct no_factory_graph {
    auto vertices() const { return std::views::iota(0u, 2u); }
    unsigned int num_vertices() const { return 2u; }
    auto out_arcs(unsigned int) const { return std::views::empty<unsigned>; }
    unsigned int arc_target(unsigned int) const;
};
static_assert(melon::outward_incidence_graph<no_factory_graph>);
static_assert(!melon::has_vertex_map<no_factory_graph>);
template <typename G, typename M>
concept dijkstra_deducible = requires(G & g, M & m) { melon::dijkstra(g, m); };
template <typename G>
concept bfs_deducible = requires(G & g) { melon::breadth_first_search(g); };
static_assert(!dijkstra_deducible<no_factory_graph, std::vector<int>>);
static_assert(!bfs_deducible<no_factory_graph>);
static_assert(dijkstra_deducible<static_digraph, std::vector<int>>);
static_assert(bfs_deducible<static_digraph>);
}  // namespace probe_safety

////////////////////////////////////////////////////////////////////////////////
// factory maps only promise output_mapping: construction and reset() fill
// through detail::fill, so a conforming map without a member fill runs every
// algorithm all the same
////////////////////////////////////////////////////////////////////////////////

namespace fill_less_maps {
template <typename V>
struct plain_map {
    std::vector<V> d;
    // decltype(auto): vector<bool>'s subscript yields a proxy, not a bool &.
    decltype(auto) operator[](unsigned int k) { return d[k]; }
    decltype(auto) operator[](unsigned int k) const { return d[k]; }
};
struct graph_with_plain_maps {
    unsigned int n = 3;
    std::vector<std::pair<unsigned, unsigned>> ends{{0, 1}, {1, 2}};

    auto vertices() const { return std::views::iota(0u, n); }
    unsigned int num_vertices() const { return n; }
    auto out_arcs(unsigned int u) const {
        return std::views::iota(0u, static_cast<unsigned>(ends.size())) |
               std::views::filter(
                   [this, u](unsigned int a) { return ends[a].first == u; });
    }
    unsigned int arc_target(unsigned int a) const { return ends[a].second; }
    template <typename V>
    auto create_vertex_map() const {
        return plain_map<V>{std::vector<V>(n)};
    }
    template <typename V>
    auto create_vertex_map(const V & v) const {
        return plain_map<V>{std::vector<V>(n, v)};
    }
    template <typename V>
    auto create_arc_map() const {
        return plain_map<V>{std::vector<V>(ends.size())};
    }
    template <typename V>
    auto create_arc_map(const V & v) const {
        return plain_map<V>{std::vector<V>(ends.size(), v)};
    }
};
static_assert(melon::has_vertex_map<graph_with_plain_maps>);
static_assert(melon::has_arc_map<graph_with_plain_maps>);
}  // namespace fill_less_maps

GTEST_TEST(api_review, algorithms_run_and_reset_on_fill_less_factory_maps) {
    fill_less_maps::graph_with_plain_maps g;
    std::vector<int> lengths{1, 1};

    auto dij = melon::dijkstra(g, lengths);
    dij.add_source(0u);
    for([[maybe_unused]] auto && entry : dij) {
    }
    // reset() refills the status map -- through the per-key fallback here.
    dij.reset();
    dij.add_source(0u);
    for([[maybe_unused]] auto && entry : dij) {
    }

    // topological_sort fills at construction, the case that decides whether
    // the object can even be built on such maps.
    auto topo = melon::topological_sort(g);
    for([[maybe_unused]] auto && v : topo) {
    }
}
