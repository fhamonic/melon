#undef NDEBUG
#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <set>
#include <vector>

#include "melon/container/mutable_digraph.hpp"
#include "melon/graph.hpp"

#include "dumb_digraph.hpp"
#include "random_ranges_helper.hpp"
#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// mutable_digraph models the graph concepts with full vertex/arc creation,
// removal and re-wiring, and its ranges are borrowed forward ranges
////////////////////////////////////////////////////////////////////////////////

static_assert(melon::graph<mutable_digraph>);
static_assert(melon::outward_incidence_graph<mutable_digraph>);
static_assert(melon::outward_adjacency_graph<mutable_digraph>);
static_assert(melon::has_vertex_map<mutable_digraph>);
static_assert(melon::has_vertex_creation<mutable_digraph>);
static_assert(melon::has_vertex_removal<mutable_digraph>);
static_assert(melon::has_arc_creation<mutable_digraph>);
static_assert(melon::has_arc_removal<mutable_digraph>);
static_assert(melon::has_change_arc_source<mutable_digraph>);
static_assert(melon::has_change_arc_target<mutable_digraph>);

static_assert(std::ranges::forward_range<vertices_range_t<mutable_digraph>>);
static_assert(std::ranges::borrowed_range<vertices_range_t<mutable_digraph>>);
static_assert(std::ranges::forward_range<out_arcs_range_t<mutable_digraph>>);
static_assert(std::ranges::borrowed_range<out_arcs_range_t<mutable_digraph>>);
static_assert(std::ranges::forward_range<in_arcs_range_t<mutable_digraph>>);
static_assert(std::ranges::borrowed_range<in_arcs_range_t<mutable_digraph>>);

using Graph = mutable_digraph;
using arc_entries_list = std::initializer_list<
    std::pair<arc_t<Graph>, std::pair<vertex_t<Graph>, vertex_t<Graph>>>>;

////////////////////////////////////////////////////////////////////////////////
// vertices and arcs can be created, queried and removed; invalid ids are
// rejected and asserting accessors die on them
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(mutable_digraph, empty_constructor) {
    Graph graph;
    ASSERT_TRUE(EMPTY(vertices(graph)));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(arcs_entries(graph)));

    ASSERT_FALSE(is_valid_vertex(graph, 0));
    EXPECT_DEATH((void)out_arcs(graph, 0), "");
    EXPECT_DEATH((void)in_arcs(graph, 0), "");
    EXPECT_DEATH((void)out_neighbors(graph, 0), "");
    EXPECT_DEATH((void)in_neighbors(graph, 0), "");
}

GTEST_TEST(mutable_digraph, create_vertices) {
    Graph graph;

    auto a = create_vertex(graph);
    auto b = create_vertex(graph);
    auto c = create_vertex(graph);

    ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {a, b, c}));
    ASSERT_TRUE(EMPTY(arcs(graph)));
    ASSERT_TRUE(EMPTY(out_arcs(graph, 0)));
    ASSERT_TRUE(EMPTY(out_arcs(graph, 1)));
    ASSERT_TRUE(EMPTY(out_arcs(graph, 2)));
    ASSERT_TRUE(is_valid_vertex(graph, 2));
    ASSERT_FALSE(is_valid_vertex(graph, 3));
    EXPECT_DEATH((void)out_arcs(graph, 3), "");
}

GTEST_TEST(mutable_digraph, create_arcs) {
    Graph graph;

    auto a = create_vertex(graph);
    auto b = create_vertex(graph);
    auto c = create_vertex(graph);

    auto ab = create_arc(graph, a, b);
    auto ac = create_arc(graph, a, c);
    auto cb = create_arc(graph, c, b);
    auto ca = create_arc(graph, c, a);

    ASSERT_TRUE(EQ_MULTISETS(vertices(graph), {a, b, c}));
    ASSERT_TRUE(EQ_MULTISETS(arcs(graph), {ab, ac, cb, ca}));

    ASSERT_EQ(arc_source(graph, ab), a);
    ASSERT_EQ(arc_source(graph, ac), a);
    ASSERT_EQ(arc_source(graph, cb), c);
    ASSERT_EQ(arc_target(graph, ab), b);
    ASSERT_EQ(arc_target(graph, ac), c);
    ASSERT_EQ(arc_target(graph, cb), b);

    ASSERT_TRUE(EQ_MULTISETS(
        arcs_entries(graph),
        arc_entries_list{
            {ab, {a, b}}, {ac, {a, c}}, {cb, {c, b}}, {ca, {c, a}}}));

    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, a), {b, c}));
    ASSERT_TRUE(EMPTY(out_neighbors(graph, b)));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, c), {a, b}));
}

GTEST_TEST(mutable_digraph, remove_arcs) {
    Graph graph;
    auto a = create_vertex(graph);
    auto b = create_vertex(graph);
    auto c = create_vertex(graph);
    auto ab = create_arc(graph, a, b);
    auto ac = create_arc(graph, a, c);
    auto cb = create_arc(graph, c, b);

    remove_arc(graph, ac);

    ASSERT_FALSE(is_valid_arc(graph, ac));
    ASSERT_EQ(arc_source(graph, ab), a);
    EXPECT_DEATH((void)arc_source(graph, ac), "");
    ASSERT_EQ(arc_source(graph, cb), c);
    ASSERT_EQ(arc_target(graph, ab), b);
    EXPECT_DEATH((void)arc_target(graph, ac), "");
    ASSERT_EQ(arc_target(graph, cb), b);

    ASSERT_TRUE(EQ_MULTISETS(arcs_entries(graph),
                             arc_entries_list{{cb, {c, b}}, {ab, {a, b}}}));

    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, a), {b}));
    ASSERT_TRUE(EMPTY(out_neighbors(graph, b)));
    ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, c), {b}));
}

////////////////////////////////////////////////////////////////////////////////
// any sequence of mutations stays consistent with the dumb_digraph reference
// implementation
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(mutable_digraph, fuzzy_test) {
    enum Operation {
        CREATE_VERTEX,
        REMOVE_VERTEX,
        CREATE_ARC,
        REMOVE_ARC,
        CHANGE_SOURCE,
        CHANGE_TARGET
    };
    std::vector<Operation> operations = {
        CREATE_VERTEX, CREATE_VERTEX, REMOVE_VERTEX, CREATE_ARC,   CREATE_ARC,
        CREATE_ARC,    REMOVE_ARC,    CHANGE_SOURCE, CHANGE_TARGET};

    for(std::size_t j = 0; j < 10; ++j) {
        mutable_digraph graph;
        dumb_digraph dummy_graph;

        for(std::size_t i = 0; i < 200; ++i) {
            Operation op;
            for(;;) {
                op = random_element(operations);
                auto num_vertices_ =
                    std::ranges::distance(dummy_graph.vertices());
                auto num_arcs_ = std::ranges::distance(dummy_graph.arcs());
                if(op == REMOVE_VERTEX && num_vertices_ == 0) continue;
                if(op == CREATE_ARC && num_vertices_ < 2) continue;
                if(op == REMOVE_ARC && num_arcs_ == 0) continue;
                if((op == CHANGE_SOURCE || op == CHANGE_TARGET) &&
                   (num_arcs_ == 0 || num_vertices_ < 2))
                    continue;
                break;
            }

            if(op == CREATE_VERTEX) {
                auto u = create_vertex(graph);
                // std::cout << "create_vertex() -> " << u << std::endl;
                dummy_graph.create_vertex(u);
            }
            if(op == REMOVE_VERTEX) {
                auto u = random_element(dummy_graph.vertices());
                // std::cout << "remove_vertex(" << u << ")" << std::endl;
                remove_vertex(graph, u);
                dummy_graph.remove_vertex(u);
            }
            if(op == CREATE_ARC) {
                auto s = random_element(dummy_graph.vertices());
                auto t = random_element(dummy_graph.vertices());
                auto a = create_arc(graph, s, t);
                dummy_graph.create_arc(a, s, t);
                // std::cout << "create_arc(" << s << ", " << t << ") -> " << a
                //           << std::endl;
            }
            if(op == REMOVE_ARC) {
                auto a = random_element(dummy_graph.arcs());
                // std::cout << "remove_arc(" << a << ")" << std::endl;
                remove_arc(graph, a);
                dummy_graph.remove_arc(a);
            }
            if(op == CHANGE_SOURCE) {
                auto a = random_element(dummy_graph.arcs());
                auto s = random_element(dummy_graph.vertices());
                // std::cout << "change_arc_source(" << a << ", " << s << ")" <<
                // std::endl;
                change_arc_source(graph, a, s);
                dummy_graph.change_arc_source(a, s);
            }
            if(op == CHANGE_TARGET) {
                auto a = random_element(dummy_graph.arcs());
                auto t = random_element(dummy_graph.vertices());
                // std::cout << "change_arc_target(" << a << ", " << t << ")" <<
                // std::endl;
                change_arc_target(graph, a, t);
                dummy_graph.change_arc_target(a, t);
            }

            ASSERT_TRUE(EQ_MULTISETS(vertices(graph), dummy_graph.vertices()));
            ASSERT_TRUE(EQ_MULTISETS(arcs(graph), dummy_graph.arcs()));
            ASSERT_EQ(num_vertices(graph),
                      static_cast<std::size_t>(dummy_graph.num_vertices()));
            ASSERT_EQ(num_arcs(graph),
                      static_cast<std::size_t>(dummy_graph.num_arcs()));
            ASSERT_TRUE(
                EQ_MULTISETS(arcs_entries(graph), dummy_graph.arcs_entries()));

            // std::cout << "v=";
            for(auto && v : vertices(graph)) {
                // std::cout << v << ",";
                ASSERT_TRUE(is_valid_vertex(graph, v));
                ASSERT_TRUE(dummy_graph.is_valid_vertex(v));
                ASSERT_TRUE(
                    EQ_MULTISETS(in_arcs(graph, v), dummy_graph.in_arcs(v)));
                ASSERT_TRUE(
                    EQ_MULTISETS(out_arcs(graph, v), dummy_graph.out_arcs(v)));
                ASSERT_TRUE(EQ_MULTISETS(in_neighbors(graph, v),
                                         dummy_graph.in_neighbors(v)));
                ASSERT_TRUE(EQ_MULTISETS(out_neighbors(graph, v),
                                         dummy_graph.out_neighbors(v)));
            }
            // std::cout << std::endl;
            for(auto && a : arcs(graph)) {
                ASSERT_TRUE(is_valid_arc(graph, a));
                ASSERT_TRUE(dummy_graph.is_valid_arc(a));
                ASSERT_EQ(arc_target(graph, a), dummy_graph.arc_target(a));
                ASSERT_EQ(arc_source(graph, a), dummy_graph.arc_source(a));
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// removing a vertex returns every one of its arc ids to the free list
////////////////////////////////////////////////////////////////////////////////

// regression: remove_vertex() relinks each incidence chain through
// .next_in_arc and splices it onto the free list. Publishing the chain's
// *tail* as the new head instead of its first arc brings only one arc per
// chain back; the rest are unreachable forever, so _arcs grows without bound
// under churn -- and with it every create_arc_map, which sizes on
// _arcs.size() rather than num_arcs(). Nothing above catches it: the
// structure stays perfectly consistent, only the ids leak.
GTEST_TEST(mutable_digraph, remove_vertex_frees_every_incident_arc) {
    mutable_digraph graph;
    const auto hub = create_vertex(graph);
    std::vector<vertex_t<Graph>> others;
    for(std::size_t i = 0; i < 4; ++i) others.push_back(create_vertex(graph));

    std::set<arc_t<Graph>> freed;
    for(auto && v : others) freed.insert(create_arc(graph, v, hub));
    for(auto && v : others) freed.insert(create_arc(graph, hub, v));
    // the self-loop is the interesting one: it sits in both incidence lists,
    // so a splice that mishandles it either strands it or frees it twice
    freed.insert(create_arc(graph, hub, hub));
    ASSERT_EQ(freed.size(), 9u);

    remove_vertex(graph, hub);
    ASSERT_EQ(num_arcs(graph), 0u);

    // every freed id comes back, and each exactly once -- a double free would
    // hand the same id out twice and shrink the set
    const auto hub2 = create_vertex(graph);
    std::set<arc_t<Graph>> reused;
    for(auto && v : others) reused.insert(create_arc(graph, v, hub2));
    for(auto && v : others) reused.insert(create_arc(graph, hub2, v));
    reused.insert(create_arc(graph, hub2, hub2));
    ASSERT_EQ(reused, freed);
}

// The same property without leaning on an allocation order: over an arbitrary
// mutation sequence, an id is only ever minted when the free list is empty, so
// the highest one handed out stays below the peak number of live arcs.
GTEST_TEST(mutable_digraph, arc_ids_stay_bounded_by_the_peak_arc_count) {
    for(std::size_t j = 0; j < 10; ++j) {
        mutable_digraph graph;
        dumb_digraph reference;
        std::size_t peak_arcs = 0;
        std::optional<arc_t<Graph>> highest_id;

        for(std::size_t i = 0; i < 200; ++i) {
            const auto num_vertices_ =
                std::ranges::distance(reference.vertices());
            if(num_vertices_ < 3 || i % 3 == 0) {
                const auto u = create_vertex(graph);
                reference.create_vertex(u);
            } else if(i % 3 == 1) {
                const auto s = random_element(reference.vertices());
                const auto t = random_element(reference.vertices());
                const auto a = create_arc(graph, s, t);
                reference.create_arc(a, s, t);
                highest_id = highest_id ? std::max(*highest_id, a) : a;
            } else {
                const auto u = random_element(reference.vertices());
                remove_vertex(graph, u);
                reference.remove_vertex(u);
            }
            peak_arcs = std::max(peak_arcs, num_arcs(graph));
        }

        if(highest_id.has_value()) {
            ASSERT_LT(*highest_id, peak_arcs)
                << "arc ids outgrew the peak live-arc count: the free list is "
                   "losing entries";
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// a moved-from graph is a valid empty graph. The vectors empty on move, but a
// defaulted member-wise move keeps the counts and list heads, so the source
// claims vertices its vectors no longer hold.
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(mutable_digraph, moved_from_is_a_valid_empty_graph) {
    mutable_digraph graph;
    const auto u = create_vertex(graph);
    const auto v = create_vertex(graph);
    [[maybe_unused]] const auto a = create_arc(graph, u, v);

    mutable_digraph target(std::move(graph));
    ASSERT_EQ(target.num_vertices(), 2u);
    ASSERT_EQ(target.num_arcs(), 1u);
    ASSERT_EQ(graph.num_vertices(), 0u);
    ASSERT_EQ(graph.num_arcs(), 0u);
    ASSERT_TRUE(std::ranges::empty(vertices(graph)));
    auto moved_from_arcs = arcs(graph);
    ASSERT_TRUE(moved_from_arcs.begin() == moved_from_arcs.end());

    // the empty state is fully usable: creation restarts from scratch
    const auto w = create_vertex(graph);
    ASSERT_EQ(graph.num_vertices(), 1u);
    ASSERT_TRUE(graph.is_valid_vertex(w));

    // move assignment empties the source the same way
    graph = std::move(target);
    ASSERT_EQ(graph.num_vertices(), 2u);
    ASSERT_EQ(graph.num_arcs(), 1u);
    ASSERT_EQ(target.num_vertices(), 0u);
    ASSERT_EQ(target.num_arcs(), 0u);
    ASSERT_TRUE(std::ranges::empty(vertices(target)));
}

////////////////////////////////////////////////////////////////////////////////
// clear() has the std container shape: everything removed, the graph reusable
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(mutable_digraph, clear) {
    mutable_digraph graph;
    auto u = graph.create_vertex();
    auto v = graph.create_vertex();
    (void)graph.create_arc(u, v);
    graph.clear();
    ASSERT_EQ(graph.num_vertices(), 0u);
    ASSERT_EQ(graph.num_arcs(), 0u);
    ASSERT_TRUE(std::ranges::empty(vertices(graph)));
    // and the graph is usable afterwards
    auto w = graph.create_vertex();
    ASSERT_EQ(graph.num_vertices(), 1u);
    ASSERT_TRUE(is_valid_vertex(graph, w));
}

////////////////////////////////////////////////////////////////////////////////
// iterator honesty: the intrusive iterators return prvalues, so they are
// C++20 forward_iterators but only Cpp17 *input* iterators -- the
// std::vector<bool> / zip_view split
////////////////////////////////////////////////////////////////////////////////

using vertices_it = std::ranges::iterator_t<decltype(vertices(
    std::declval<const mutable_digraph &>()))>;
static_assert(std::forward_iterator<vertices_it>);
static_assert(std::same_as<std::iterator_traits<vertices_it>::iterator_category,
                           std::input_iterator_tag>);
static_assert(
    std::same_as<vertices_it::iterator_concept, std::forward_iterator_tag>);

////////////////////////////////////////////////////////////////////////////////
// vertex and arc handles share one id type, so iterator equality is defined
// per derived iterator: cursors from unrelated lists must not compare at all
// rather than compare equal on equal ids
////////////////////////////////////////////////////////////////////////////////

static_assert(!std::equality_comparable_with<
              std::ranges::iterator_t<decltype(out_arcs(
                  std::declval<const mutable_digraph &>(),
                  std::declval<vertex_t<mutable_digraph>>()))>,
              std::ranges::iterator_t<decltype(in_arcs(
                  std::declval<const mutable_digraph &>(),
                  std::declval<vertex_t<mutable_digraph>>()))>>);
static_assert(!std::equality_comparable_with<
              std::ranges::iterator_t<
                  decltype(vertices(std::declval<const mutable_digraph &>()))>,
              std::ranges::iterator_t<decltype(out_arcs(
                  std::declval<const mutable_digraph &>(),
                  std::declval<vertex_t<mutable_digraph>>()))>>);
