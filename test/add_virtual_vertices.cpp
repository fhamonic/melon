#undef NDEBUG
#include <gtest/gtest.h>

#include <ranges>
#include <utility>
#include <vector>

#include "melon/container/mutable_digraph.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/experimental/add_virtual_vertices.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// add_virtual_vertices extends a graph's integral vertex id space with
// `count` fresh ids past the largest one, leaving arcs untouched; the virtual
// vertices have empty incidence and the vertex-map factory covers them
////////////////////////////////////////////////////////////////////////////////

namespace {
// vertices 0..3, arcs 0: 0>2, 1: 1>2, 2: 2>3
auto base_test_instance() {
    static_digraph_builder<static_digraph, int> builder(4);
    builder.add_arc({0u, 2u}, 3).add_arc({1u, 2u}, 5).add_arc({2u, 3u}, 4);
    return builder.build();
}
}  // namespace

using augmented_t = decltype(experimental::views::add_virtual_vertices(
    std::declval<static_digraph &>(), 2));
static_assert(graph_view<augmented_t>);
static_assert(outward_incidence_graph<augmented_t>);
static_assert(inward_incidence_graph<augmented_t>);
static_assert(outward_adjacency_graph<augmented_t>);
static_assert(inward_adjacency_graph<augmented_t>);
static_assert(has_arc_source<augmented_t> && has_arc_target<augmented_t>);
static_assert(has_out_degree<augmented_t> && has_in_degree<augmented_t>);
static_assert(has_num_vertices<augmented_t> && has_num_arcs<augmented_t>);
static_assert(has_vertex_map<augmented_t> && has_arc_map<augmented_t>);

GTEST_TEST(add_virtual_vertices, extended_vertex_id_space) {
    auto [graph, weight] = base_test_instance();
    auto view = experimental::views::add_virtual_vertices(graph, 2);

    ASSERT_EQ(view.num_virtual_vertices(), 2u);
    ASSERT_EQ(view.virtual_vertex(0), 4u);
    ASSERT_EQ(view.virtual_vertex(1), 5u);
    ASSERT_TRUE(std::ranges::equal(view.virtual_vertices(),
                                   std::vector<unsigned int>{4u, 5u}));
    ASSERT_EQ(view.num_vertices(), 6u);
    ASSERT_TRUE(EQ_MULTISETS(view.vertices(), {0u, 1u, 2u, 3u, 4u, 5u}));
}

GTEST_TEST(add_virtual_vertices, count_defaults_to_one) {
    auto [graph, weight] = base_test_instance();
    auto view = experimental::views::add_virtual_vertices(graph);
    ASSERT_EQ(view.num_virtual_vertices(), 1u);
    ASSERT_EQ(view.virtual_vertex(0), 4u);
    ASSERT_EQ(view.num_vertices(), 5u);
}

GTEST_TEST(add_virtual_vertices, arcs_are_untouched) {
    auto [graph, weight] = base_test_instance();
    auto view = experimental::views::add_virtual_vertices(graph, 2);

    ASSERT_EQ(view.num_arcs(), num_arcs(graph));
    ASSERT_TRUE(EQ_MULTISETS(view.arcs(), arcs(graph)));
    for(auto && a : arcs(graph)) {
        ASSERT_EQ(arc_source(view, a), arc_source(graph, a));
        ASSERT_EQ(arc_target(view, a), arc_target(graph, a));
    }
    auto entries = view.arcs_entries();
    ASSERT_TRUE(EQ_MULTISETS(entries, arcs_entries(graph)));

    auto arc_map = create_arc_map<int>(view);
    for(auto && a : arcs(view)) arc_map[a] = static_cast<int>(a) + 10;
    for(auto && a : arcs(view)) ASSERT_EQ(arc_map[a], static_cast<int>(a) + 10);
}

GTEST_TEST(add_virtual_vertices, virtual_vertices_have_empty_incidence) {
    auto [graph, weight] = base_test_instance();
    auto view = experimental::views::add_virtual_vertices(graph, 2);

    for(auto && v : view.virtual_vertices()) {
        ASSERT_TRUE(std::ranges::empty(out_arcs(view, v)));
        ASSERT_TRUE(std::ranges::empty(in_arcs(view, v)));
        ASSERT_TRUE(std::ranges::empty(out_neighbors(view, v)));
        ASSERT_TRUE(std::ranges::empty(in_neighbors(view, v)));
        ASSERT_EQ(out_degree(view, v), 0u);
        ASSERT_EQ(in_degree(view, v), 0u);
    }
    for(auto && u : vertices(graph)) {
        ASSERT_TRUE(std::ranges::equal(out_arcs(view, u), out_arcs(graph, u)));
        ASSERT_TRUE(std::ranges::equal(in_arcs(view, u), in_arcs(graph, u)));
        ASSERT_EQ(out_degree(view, u), out_degree(graph, u));
        ASSERT_EQ(in_degree(view, u), in_degree(graph, u));
    }
}

GTEST_TEST(add_virtual_vertices, vertex_maps_cover_the_virtual_vertices) {
    auto [graph, weight] = base_test_instance();
    auto view = experimental::views::add_virtual_vertices(graph, 2);

    auto map = create_vertex_map<int>(view, -1);
    for(auto && v : vertices(view)) ASSERT_EQ(map[v], -1);
    map[view.virtual_vertex(0)] = 7;
    map[view.virtual_vertex(1)] = 8;
    ASSERT_EQ(map[4u], 7);
    ASSERT_EQ(map[5u], 8);
}

GTEST_TEST(add_virtual_vertices, pipe_syntax) {
    auto [graph, weight] = base_test_instance();
    auto view = graph | experimental::views::add_virtual_vertices(3);
    ASSERT_EQ(view.num_virtual_vertices(), 3u);
    ASSERT_EQ(view.num_vertices(), 7u);
}

////////////////////////////////////////////////////////////////////////////////
// ids need not be dense: over a mutable_digraph with removal holes the first
// virtual id is one past the largest LIVE id -- which may re-mint a freed id
// above every live one, since distinctness is only owed to live vertices --
// and the empty incidence at a virtual vertex exercises the at-end default
// construction of the intrusive iterators
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(add_virtual_vertices, non_dense_ids_extend_past_the_largest_live) {
    mutable_digraph graph;
    const auto v0 = graph.create_vertex();  // id 0
    const auto v1 = graph.create_vertex();  // id 1
    const auto v2 = graph.create_vertex();  // id 2
    const auto v3 = graph.create_vertex();  // id 3
    (void)graph.create_arc(v0, v2);
    graph.remove_vertex(v1);
    graph.remove_vertex(v3);
    // live ids {0, 2}; freed id 3 sits above every live id and is re-minted
    auto view = experimental::views::add_virtual_vertices(graph, 2);
    ASSERT_EQ(view.virtual_vertex(0), 3u);
    ASSERT_EQ(view.virtual_vertex(1), 4u);
    ASSERT_EQ(view.num_vertices(), 4u);
    ASSERT_TRUE(EQ_MULTISETS(view.vertices(), {0u, 2u, 3u, 4u}));

    ASSERT_TRUE(is_valid_vertex(view, 3u));
    ASSERT_TRUE(is_valid_vertex(view, 4u));
    ASSERT_FALSE(is_valid_vertex(view, 1u));

    for(auto && v : view.virtual_vertices()) {
        ASSERT_TRUE(std::ranges::empty(out_arcs(view, v)));
        ASSERT_TRUE(std::ranges::empty(in_arcs(view, v)));
    }
    ASSERT_TRUE(std::ranges::equal(out_arcs(view, v0), out_arcs(graph, v0)));

    auto map = create_vertex_map<int>(view, 0);
    map[view.virtual_vertex(1)] = 5;
    ASSERT_EQ(map[4u], 5);
}

GTEST_TEST(add_virtual_vertices, stacking_mints_past_the_inner_virtuals) {
    auto [graph, weight] = base_test_instance();
    auto inner = experimental::views::add_virtual_vertices(graph, 1);
    auto outer = experimental::views::add_virtual_vertices(inner, 2);
    ASSERT_EQ(inner.virtual_vertex(0), 4u);
    ASSERT_EQ(outer.virtual_vertex(0), 5u);
    ASSERT_EQ(outer.virtual_vertex(1), 6u);
    ASSERT_EQ(outer.num_vertices(), 7u);
    ASSERT_TRUE(EQ_MULTISETS(outer.vertices(), {0u, 1u, 2u, 3u, 4u, 5u, 6u}));
    // the outer map covers the inner's virtual vertex too
    auto map = create_vertex_map<int>(outer, -1);
    map[inner.virtual_vertex(0)] = 1;
    map[outer.virtual_vertex(1)] = 2;
    ASSERT_EQ(map[4u], 1);
    ASSERT_EQ(map[6u], 2);
}

////////////////////////////////////////////////////////////////////////////////
// the constraint boundary: vertex maps are granted even over a base with no
// factory of its own, and a graph with non-integral vertex ids is rejected --
// that half of the id contract is load-bearing (fresh-id minting, id-sized
// maps), unlike the arc half, which the view does not touch
////////////////////////////////////////////////////////////////////////////////

namespace {

struct bare_digraph {
    unsigned int n;
    std::vector<std::pair<unsigned int, unsigned int>> ends;

    auto vertices() const { return std::views::iota(0u, n); }
    auto arcs() const {
        return std::views::iota(0u, static_cast<unsigned int>(ends.size()));
    }
    auto arcs_entries() const {
        return arcs() | std::views::transform([this](unsigned int a) {
                   return std::make_pair(a, ends[a]);
               });
    }
};

struct opaque_vertex {
    unsigned int index;
    bool operator==(const opaque_vertex &) const = default;
};
struct opaque_vertex_digraph {
    auto vertices() const {
        return std::views::iota(0u, 2u) |
               std::views::transform(
                   [](const unsigned int i) { return opaque_vertex{i}; });
    }
    auto arcs() const { return std::views::iota(0u, 0u); }
    auto arcs_entries() const {
        return std::views::empty<
            std::pair<unsigned int, std::pair<opaque_vertex, opaque_vertex>>>;
    }
};

}  // namespace

static_assert(!has_vertex_map<bare_digraph>);
static_assert(has_vertex_map<decltype(experimental::views::add_virtual_vertices(
                  std::declval<bare_digraph &>()))>);

// A concept, not a bare requires-expression: outside a template the probed
// call is non-dependent, so its failure is a hard error instead of `false`.
template <typename G>
concept admits_virtual_vertices =
    requires(G & g) { experimental::views::add_virtual_vertices(g); };
static_assert(admits_virtual_vertices<static_digraph>);
static_assert(graph<opaque_vertex_digraph>);
static_assert(!admits_virtual_vertices<opaque_vertex_digraph>);

GTEST_TEST(add_virtual_vertices, grants_vertex_maps_over_a_bare_graph) {
    bare_digraph graph{3u, {{0u, 1u}, {1u, 2u}}};
    auto view = experimental::views::add_virtual_vertices(graph);
    auto map = create_vertex_map<int>(view, 0);
    map[view.virtual_vertex(0)] = 9;
    ASSERT_EQ(map[3u], 9);
    ASSERT_TRUE(EQ_MULTISETS(view.vertices(), {0u, 1u, 2u, 3u}));
}
