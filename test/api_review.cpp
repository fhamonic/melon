// Regression tests from the second and third API-review passes, run after the
// TODO.md items were closed. Like api_consistency.cpp these are mostly
// family-wide invariants; every test here was checked to fail before the fix
// it guards and passes after. The file is organised by theme, not by review
// pass.
//
// The value-semantics tests deliberately destroy the source before reading the
// copy: a copy whose cursor still points into a *live* source reads plausible
// values, which is why the original defects survived a full suite.

#undef NDEBUG
#include <gtest/gtest.h>

#include <concepts>
#include <memory>
#include <ranges>
#include <type_traits>
#include <vector>

#include "melon/algorithm/biobjective_dijkstra.hpp"
#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/algorithm/competing_dijkstras.hpp"
#include "melon/algorithm/depth_first_search.hpp"
#include "melon/algorithm/dijkstra.hpp"
#include "melon/algorithm/knapsack_bnb.hpp"
#include "melon/algorithm/kruskal.hpp"
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
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/graph_view.hpp"
#include "melon/views/reverse.hpp"
#include "melon/views/subgraph.hpp"
#include "melon/views/undirect.hpp"
#include "melon/views/undirected_graph_view.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// a copied or assigned algorithm owns its state: no cursor may point into
// another object
////////////////////////////////////////////////////////////////////////////////

// kruskal kept its cursor as an iterator into its own _sorted_edges, so a
// memberwise copy compared it against the *copy's* end() -- never equal.
// Destroying the source first turns the old behaviour into an ASan report
// instead of a silently wrong traversal.
GTEST_TEST(api_review, kruskal_copy_owns_its_cursor) {
    static_digraph_builder<static_digraph, double> b(4);
    b.add_arc(0u, 1u, 1.0)
        .add_arc(1u, 2u, 2.0)
        .add_arc(2u, 3u, 3.0)
        .add_arc(0u, 3u, 9.0);
    auto [g, costs] = b.build();
    auto ug = views::undirect(g);

    auto source = std::make_unique<decltype(kruskal(ug, costs))>(ug, costs);
    auto copy = *source;
    source.reset();

    std::vector<unsigned> taken;
    for(; !copy.finished(); copy.advance()) taken.push_back(copy.current());
    EXPECT_EQ(taken.size(), 3u) << "a spanning tree of 4 vertices has 3 edges";
}

GTEST_TEST(api_review, kruskal_copy_assignment_owns_its_cursor) {
    static_digraph_builder<static_digraph, double> b(3);
    b.add_arc(0u, 1u, 1.0).add_arc(1u, 2u, 2.0);
    auto [g, costs] = b.build();
    auto ug = views::undirect(g);

    kruskal reference(ug, costs);
    auto source = std::make_unique<decltype(kruskal(ug, costs))>(ug, costs);
    reference = *source;
    source.reset();

    std::size_t n = 0;
    for(; !reference.finished(); reference.advance()) ++n;
    EXPECT_EQ(n, 2u);
}

// knapsack_bnb / unbounded_knapsack_bnb keep _best_sol as iterators into their
// own _value_cost_pairs.
GTEST_TEST(api_review, knapsack_copy_owns_its_solution) {
    std::vector<unsigned> items{0u, 1u, 2u, 3u};
    std::vector<double> value{10.0, 7.0, 5.0, 3.0};
    std::vector<double> cost{5.0, 4.0, 3.0, 2.0};

    using knapsack = decltype(knapsack_bnb(items, value, cost, 9.0));
    auto source = std::make_unique<knapsack>(items, value, cost, 9.0);
    source->run();
    const double expected = source->solution_value();

    auto copy = *source;
    source.reset();
    EXPECT_EQ(copy.solution_value(), expected);
    EXPECT_LE(copy.solution_cost(), 9.0);
    std::size_t n = 0;
    for([[maybe_unused]] auto && i : copy.solution_items()) ++n;
    EXPECT_GT(n, 0u);
}

GTEST_TEST(api_review, unbounded_knapsack_copy_owns_its_solution) {
    std::vector<unsigned> items{0u, 1u};
    std::vector<double> value{6.0, 5.0};
    std::vector<double> cost{4.0, 3.0};

    using knapsack = decltype(unbounded_knapsack_bnb(items, value, cost, 9.0));
    auto source = std::make_unique<knapsack>(items, value, cost, 9.0);
    source->run();
    const double expected = source->solution_value();

    auto copy = *source;
    source.reset();
    EXPECT_EQ(copy.solution_value(), expected);
}

// The generic breadth_first_search specialisation kept a defaulted copy while
// its cursor is an iterator into its own _queue. Destroy the source first: a
// cursor into a live source reads plausible values.
namespace {
struct distance_bfs_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = true;  // selects the generic one
    static constexpr bool store_traversal_range = false;
};
}  // namespace

GTEST_TEST(api_review, generic_bfs_copy_rebases_its_cursor) {
    static_digraph_builder<static_digraph> b(4);
    b.add_arc(0u, 1u).add_arc(0u, 2u).add_arc(1u, 3u).add_arc(2u, 3u);
    auto [g] = b.build();

    using BFS = breadth_first_search<graph_ref_view<static_digraph>,
                                     distance_bfs_traits>;
    static_assert(std::copyable<BFS>);

    auto source = std::make_unique<BFS>(g);
    source->add_source(0u);
    source->advance();

    BFS copy = *source;
    source.reset();

    copy.run();
    for(const auto & v : melon::vertices(g)) EXPECT_TRUE(copy.reached(v));
    EXPECT_EQ(copy.dist(3u), 2);
}

// A cursor over a non-borrowed range keeps an iterator that refers back into
// the range it holds (a filter_view iterator holds a parent pointer), and these
// cursors live inside a std::vector. Relocating that vector therefore used to
// leave every stack frame's cursor aimed at freed memory -- with no copy
// anywhere in the program. The branch at the root is what makes it reachable:
// without it the traversal never increments a relocated cursor.
//
// This one only *fails* under a sanitizer -- the read lands on freed memory
// that still holds the right bytes -- so it is written for the
// linux-gcc15-sanitize job (-DMELON_SANITIZE=address,undefined), where the old
// code reported a heap-use-after-free in consumable_input_view::current().
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
// value* points back at that member, and a memberwise copy or move aims the
// new object's cursors at the old object's graph. Every cached cursor is
// keyed by the vertex it walks from, so relocation *re-asks the new graph*
// for each frame's range and the consumed counter restores the position --
// which is what makes a DFS over an owned, filtered subgraph honestly
// copyable, where it used to be constrained away, and what makes its move
// sound, which the defaulted move it replaces was not (the old pin here
// claimed soundness the ASan trace refuted).
GTEST_TEST(api_review, algorithms_caching_ranges_rebase_on_relocation) {
    static_digraph static_g;
    mutable_digraph mutable_g;

    // Copyable as before: every range these hand out references storage that
    // lives outside the graph view the algorithm stores.
    static_assert(std::copyable<decltype(depth_first_search(static_g, 0u))>);
    static_assert(std::copyable<decltype(depth_first_search(mutable_g, 0u))>);
    static_assert(std::copyable<decltype(depth_first_search(
                      views::reverse(static_g), 0u))>);
    static_assert(
        std::copyable<decltype(strongly_connected_components(static_g))>);

    // Newly copyable: the cached ranges point back at the stored subgraph,
    // and the copy re-aims them at its own subgraph member.
    using sub = subgraph_view<graph_ref_view<static_digraph>,
                              mapping_ref_view<static_map<unsigned, bool>>,
                              maps::true_map>;
    using sub_dfs = decltype(depth_first_search(std::declval<sub &>(), 0u));
    static_assert(std::copyable<sub_dfs>);
    static_assert(std::movable<sub_dfs>);

    // And the relocations are sound mid-run: interleave a move and a copy
    // into a traversal and it yields exactly what an undisturbed one does.
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
    auto moved = std::move(dfs);           // relocate mid-run
    visited.push_back(moved.current());
    moved.advance();
    auto copied = moved;                   // duplicate mid-run
    while(!copied.finished()) {
        visited.push_back(copied.current());
        copied.advance();
    }
    EXPECT_EQ(visited, expected);
    // The copy's source is unharmed and finishes on its own.
    std::size_t remaining = 0;
    while(!moved.finished()) {
        ++remaining;
        moved.advance();
    }
    EXPECT_EQ(remaining, expected.size() - 2);
}

// views::undirect promised its ranges survive the view being relocated while
// its incidence lambdas captured `this`.
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
    held.reset();         // the object the lambdas used to point at

    std::vector<std::pair<unsigned, unsigned>> got;
    for(auto && e : melon::incidence(relocated, 1u))
        got.emplace_back(e.first, e.second);
    EXPECT_EQ(got, expected);
}

////////////////////////////////////////////////////////////////////////////////
// owned (rvalue) graphs and ranges are accepted wherever references are
////////////////////////////////////////////////////////////////////////////////

// induced_subgraph over an *rvalue* range: std::views::all_t of a temporary
// container is a move-only owning_view, so a vertices() returning by value was
// ill-formed and the whole type silently stopped modelling graph.
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

// traversal_forest used to store the graph view twice -- once itself, once
// inside its breadth_first_search -- and built the second from the first as an
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

// The re-seeding idiom consumable_input_view exists for --
// `_remaining_out_arcs[u] = out_arcs(_graph, u)` -- is a prvalue, which never
// bound to the old `operator=(R &)`.
GTEST_TEST(api_review, consumable_cursor_is_assignable_from_a_prvalue_range) {
    static_digraph_builder<static_digraph> b(3);
    b.add_arc(0u, 1u).add_arc(0u, 2u);
    auto [g] = b.build();
    auto arc_filter = g.create_arc_map<bool>(true);
    auto sub = views::subgraph(g, maps::true_map{}, arc_filter);

    using range = out_arcs_range_t<decltype(sub)>;
    static_assert(!std::ranges::borrowed_range<range>,
                  "a filtered incidence range lands on the owning cursor");

    consumable_input_view<range> cursor(melon::out_arcs(sub, 0u));
    cursor.advance();
    ASSERT_FALSE(cursor.empty());
    cursor = melon::out_arcs(sub, 0u);  // did not compile before
    EXPECT_FALSE(cursor.empty());
}

// views::graph_all had no constructible_from guard, so asking the question at
// all hard-errored for an lvalue move-only view instead of picking a branch.
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

// graph_forwarding_interface forwarded sixteen accessors and not arcs_entries,
// so views::graph_all -- which every algorithm and every view calls -- turned a
// graph of the shape above into a non-graph.
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
// views::subgraph can ask it. It used to ask has_vertex_removal, which no view
// satisfies, leaving the branch dead: a vertex removed from the graph came back
// valid from its subgraph while vertices() had already stopped listing it.
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

////////////////////////////////////////////////////////////////////////////////
// noexcept specifications are honest in both directions
////////////////////////////////////////////////////////////////////////////////

// subgraph.hpp was missed entirely by the sweep the other views got.
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

// num_vertices was the one member of every view that carried no specification
// at all, while its num_arcs / num_edges sibling carried a conditional one.
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

// Each CPO used to carry a private is_noexcept() that re-ran the same
// `if constexpr` its operator() ran, in a different place -- and in
// undirected_graph.hpp the two drifted, so the specification came from an
// overload the operator never called. Twenty-three of the twenty-six are now
// one constrained overload per protocol, with the noexcept beside the
// expression it measures. This pins both directions.
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

////////////////////////////////////////////////////////////////////////////////
// CTAD deduces exactly the type the user would have spelled out
////////////////////////////////////////////////////////////////////////////////

// A deduction guide that defaults Traits computes it over the *deduced* graph
// type -- a reference -- while the class computes it over graph_all_t, so the
// two named different specialisations of <algo>_default_traits.
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

// run() answered void in ten algorithms and Algo & in four; reset() already
// returned Algo & everywhere. Returning the reference is the additive fix.
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
// be absent. It used to be unconditional in the branchless one.
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
// the adaptors (a copy of the adapted view). There used to be three
// *different* conventions and no base() at all on subgraph or undirect.
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
    // Both of these had none before.
    static_assert(
        has_const_base<subgraph_view<RV, maps::true_map, maps::true_map>>);
    static_assert(has_const_base<undirect_view<RV>>);
    SUCCEED();
}

// store_traversal_range was silently ignored by the branchless specialisation,
// and the two returned different types for the same member.
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

// reached_map() sat on two of the eight algorithms that answer reached().
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
    auto ts = topological_sort(g).run();
    for(const auto & v : melon::vertices(g))
        EXPECT_EQ(ts.reached_map()[v], ts.reached(v));
}

// vertices/edges were the only CPOs returning decltype(auto), so their range
// aliases were the only ones that could name a reference type.
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

// out_arcs() built its iota from a ternary mixing `arc` with num_arcs()'s
// std::size_t, so its common type was size_t: a non-common range twice the
// size of the one arcs() produced from explicitly cast ends. The size is
// multiplied by where these cursors live -- one per depth_first_search stack
// frame, one per vertex in each of dinitz's two maps.
GTEST_TEST(api_review, incidence_ranges_are_common_and_narrow) {
    using SD = static_digraph;
    using OA = out_arcs_range_t<SD>;
    using AR = arcs_range_t<SD>;

    static_assert(std::ranges::common_range<OA>);
    static_assert(std::same_as<std::ranges::range_value_t<OA>, arc_t<SD>>);
    static_assert(sizeof(OA) == sizeof(AR));
    static_assert(sizeof(consumable_input_view<OA>) ==
                  sizeof(consumable_input_view<AR>));

    using SFD = static_forward_digraph;
    static_assert(std::ranges::common_range<out_arcs_range_t<SFD>>);
    static_assert(
        std::same_as<std::ranges::range_value_t<out_arcs_range_t<SFD>>,
                     arc_t<SFD>>);
    SUCCEED();
}

// The container interface every standard container has and these two did not.
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
