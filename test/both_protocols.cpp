#undef NDEBUG
#include <gtest/gtest.h>

#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/algorithm/connected_components.hpp"
#include "melon/algorithm/dijkstra.hpp"
#include "melon/algorithm/kruskal.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/views/graph_view.hpp"
#include "melon/views/reverse.hpp"
#include "melon/views/subgraph.hpp"
#include "melon/views/undirect.hpp"
#include "melon/views/with_maps.hpp"

#include "both_protocols_triangle.hpp"
#include "ranges_test_helper.hpp"

using namespace melon;

using triangle = both_protocols::triangle;

////////////////////////////////////////////////////////////////////////////////
// a type modelling both protocols keeps both through every pass-through
// wrapper, and an algorithm's constraint picks the half it reads
////////////////////////////////////////////////////////////////////////////////

static_assert(graph<triangle>);
static_assert(undirected_graph<triangle>);
static_assert(outward_incidence_graph<triangle>);
static_assert(inward_incidence_graph<triangle>);
static_assert(has_incidence<triangle>);

using ref_view = graph_ref_view<triangle>;
using owning_view = graph_owning_view<triangle>;

static_assert(graph_view<ref_view> && undirected_graph_view<ref_view>);
static_assert(graph_view<owning_view> && undirected_graph_view<owning_view>);
static_assert(outward_incidence_graph<ref_view>);
static_assert(has_incidence<ref_view>);
static_assert(has_arc_map<ref_view, int>);
static_assert(has_edge_map<ref_view, int>);
static_assert(std::same_as<arc_t<ref_view>, both_protocols::arc>);
static_assert(std::same_as<edge_t<ref_view>, both_protocols::edge>);

// The forwarding layers are a single-inheritance chain of empty bases: the
// wrapper is one pointer, not a pointer plus one word per protocol half.
static_assert(sizeof(ref_view) == sizeof(triangle *));

GTEST_TEST(both_protocols, wrapper_forwards_both_halves) {
    triangle g;
    ref_view view(g);

    ASSERT_EQ(num_arcs(view), 6u);
    ASSERT_EQ(num_edges(view), 3u);
    ASSERT_TRUE(EQ_RANGES(arcs(view), {0, 1, 2, 3, 4, 5}));
    ASSERT_TRUE(EQ_RANGES(edges(view), {0, 1, 2}));
    ASSERT_EQ(arc_source(view, 1u), 1u);
    ASSERT_EQ(arc_target(view, 1u), 0u);
    ASSERT_EQ(edge_endpoints(view, 0u), both_protocols::endpoints(0, 1));
    ASSERT_TRUE(EQ_RANGES(out_arcs(view, 0u), {0, 5}));
    ASSERT_EQ(std::ranges::distance(incidence(view, 0u)), 2);
}

GTEST_TEST(both_protocols, algorithms_pick_their_half) {
    triangle g;
    std::vector<int> arc_length(6, 1);
    std::vector<int> edge_cost{5, 3, 4};

    int reached = 0;
    for(auto && [v, d] : dijkstra(g, arc_length, 0u)) {
        (void)v;
        (void)d;
        ++reached;
    }
    ASSERT_EQ(reached, 3);

    std::vector<unsigned int> tree;
    for(auto && e : kruskal(g, edge_cost)) tree.push_back(e);
    ASSERT_TRUE(EQ_RANGES(tree, {1, 2}));

    int components = 0;
    for(auto && c : connected_components(g)) {
        (void)c;
        ++components;
    }
    ASSERT_EQ(components, 1);
}

////////////////////////////////////////////////////////////////////////////////
// the map adaptors forward both halves too, so an undirected algorithm reads
// through them
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(both_protocols, with_maps_composes_with_both_kinds_of_algorithm) {
    triangle g;
    auto provided =
        views::with_vertex_maps(g, []<typename T>(auto, const auto & gg)
                                    requires std::default_initializable<T>
                                { return std::vector<T>(num_vertices(gg)); });
    static_assert(graph_view<decltype(provided)>);
    static_assert(undirected_graph_view<decltype(provided)>);

    std::vector<int> edge_cost{5, 3, 4};
    std::vector<unsigned int> tree;
    for(auto && e : kruskal(provided, edge_cost)) tree.push_back(e);
    ASSERT_TRUE(EQ_RANGES(tree, {1, 2}));

    std::vector<int> arc_length(6, 1);
    int reached = 0;
    for(auto && [v, d] : dijkstra(provided, arc_length, 0u)) {
        (void)v;
        (void)d;
        ++reached;
    }
    ASSERT_EQ(reached, 3);

    auto with_edges = views::with_edge_maps(
        g, []<typename T>(auto, const auto & gg, const T & d) {
            return std::vector<T>(num_edges(gg), d);
        });
    static_assert(graph_view<decltype(with_edges)>);
    auto emap = create_edge_map<int>(with_edges, 7);
    ASSERT_EQ(emap[2u], 7);
}

////////////////////////////////////////////////////////////////////////////////
// a transforming view forwards only what stays true through the transform
////////////////////////////////////////////////////////////////////////////////

// reverse crosses the arc directions and leaves the edges, which have none,
// untouched: both halves survive. subgraph filters vertices and arcs and has
// no edge filter, so forwarding the edge half would list edges whose endpoint
// the filter removed: it keeps the directed half only.
static_assert(graph<reverse_view<ref_view>>);
static_assert(undirected_graph<reverse_view<ref_view>>);
static_assert(graph<subgraph_view<ref_view, maps::true_map, maps::true_map>>);
static_assert(
    !undirected_graph<subgraph_view<ref_view, maps::true_map, maps::true_map>>);

GTEST_TEST(both_protocols, reverse_keeps_the_edges) {
    triangle g;
    auto r = views::reverse(g);
    ASSERT_EQ(arc_source(r, 1u), 0u);
    ASSERT_EQ(arc_target(r, 1u), 1u);
    ASSERT_TRUE(EQ_RANGES(edges(r), {0, 1, 2}));
    ASSERT_EQ(edge_endpoints(r, 1u), both_protocols::endpoints(1, 2));
}

////////////////////////////////////////////////////////////////////////////////
// a user adaptor on the public graph_view_interface forwards both halves and
// redeclares only what it changes
////////////////////////////////////////////////////////////////////////////////

// Renumbers nothing, just reports one more vertex than it has: the vertex half
// is overridden, the arc and edge halves come from the chain untouched.
struct padded_view : graph_view_interface<ref_view> {
    using base_type = graph_view_interface<ref_view>;
    explicit padded_view(triangle & g) : base_type(ref_view(g)) {}
    auto num_vertices() const { return melon::num_vertices(wrapped()) + 1; }
};

static_assert(graph_view<padded_view> && undirected_graph_view<padded_view>);
// Not one pointer: the stored ref view carries its own graph_view_base, which
// cannot share offset zero with the chain's -- the known cost of stacking
// views, unrelated to the layers.
static_assert(sizeof(padded_view) <= 2 * sizeof(triangle *));

GTEST_TEST(both_protocols, user_adaptor_on_the_public_chain) {
    triangle g;
    padded_view p(g);
    ASSERT_EQ(num_vertices(p), 4u);
    ASSERT_EQ(num_arcs(p), 6u);
    ASSERT_EQ(num_edges(p), 3u);
    ASSERT_EQ(edge_endpoints(p, 2u), both_protocols::endpoints(2, 0));
    static_assert(std::same_as<views::graph_all_t<padded_view &>, padded_view>);
}

////////////////////////////////////////////////////////////////////////////////
// as_directed / as_undirected restrict a triangle type to one half, and are the
// identity on a type that has nothing to hide
////////////////////////////////////////////////////////////////////////////////

using directed_only = decltype(views::as_directed(std::declval<triangle &>()));
using undirected_only =
    decltype(views::as_undirected(std::declval<triangle &>()));

static_assert(std::same_as<directed_only, as_directed_view<ref_view>>);
static_assert(std::same_as<undirected_only, as_undirected_view<ref_view>>);
static_assert(graph_view<directed_only> && !undirected_graph<directed_only>);
static_assert(undirected_graph_view<undirected_only> &&
              !graph<undirected_only>);
static_assert(outward_incidence_graph<directed_only>);
static_assert(has_incidence<undirected_only>);
static_assert(has_degree<undirected_only>);
static_assert(has_vertex_map<directed_only> && has_vertex_map<undirected_only>);

// nothing to hide: the wrapper is graph_all's, one layer, not two
static_assert(
    std::same_as<decltype(views::as_directed(std::declval<static_digraph &>())),
                 graph_ref_view<static_digraph>>);
static_assert(std::same_as<decltype(views::as_directed(
                               std::declval<as_directed_view<ref_view>>())),
                           as_directed_view<ref_view>>);
static_assert(std::same_as<decltype(views::as_undirected(
                               std::declval<undirect_view<ref_view> &>())),
                           undirect_view<ref_view>>);

// what the two restrictions reject: the half is not there to keep
template <typename G>
concept as_directed_accepts = requires(G && g) { views::as_directed(g); };
template <typename G>
concept as_undirected_accepts = requires(G && g) { views::as_undirected(g); };
static_assert(!as_directed_accepts<undirected_only &>);
static_assert(!as_undirected_accepts<directed_only &>);
static_assert(!as_undirected_accepts<static_digraph &>);

// borrowed exactly when the wrapped view is
static_assert(enable_borrowed_graph<directed_only>);
static_assert(enable_borrowed_graph<undirected_only>);
static_assert(!enable_borrowed_graph<as_directed_view<owning_view>>);

GTEST_TEST(both_protocols, restrictions_forward_their_half_and_pipe) {
    triangle g;
    auto d = g | views::as_directed;
    auto u = g | views::as_undirected;
    static_assert(std::same_as<decltype(d), directed_only>);
    static_assert(std::same_as<decltype(u), undirected_only>);

    ASSERT_EQ(num_arcs(d), 6u);
    ASSERT_EQ(num_vertices(d), 3u);
    ASSERT_TRUE(EQ_RANGES(in_arcs(d, 0u), {1, 4}));
    ASSERT_EQ(num_edges(u), 3u);
    ASSERT_EQ(degree(u, 2u), 2u);
    ASSERT_EQ(&d.base().base(), &g);
    ASSERT_EQ(&u.base().base(), &g);

    std::vector<int> edge_cost{5, 3, 4};
    std::vector<unsigned int> tree;
    for(auto && e : kruskal(u, edge_cost)) tree.push_back(e);
    ASSERT_TRUE(EQ_RANGES(tree, {1, 2}));

    // undirect over the directed restriction makes edges out of the *arcs*:
    // a different, twice-as-large edge set than the type's own.
    auto ud = views::undirect(d);
    ASSERT_EQ(num_edges(ud), 6u);
}

// An overload set split on the two view concepts is ambiguous for a wrapped
// triangle type; the restrictions are how a caller picks.
namespace overloaded {
template <graph_view G>
int which(const G &) {
    return 1;
}
template <undirected_graph_view G>
int which(const G &) {
    return 2;
}
template <typename G>
concept unambiguous = requires(const G & g) { which(g); };
}  // namespace overloaded

static_assert(!overloaded::unambiguous<ref_view>);
static_assert(overloaded::unambiguous<directed_only>);
static_assert(overloaded::unambiguous<undirected_only>);

GTEST_TEST(both_protocols, restrictions_disambiguate_an_overload_set) {
    triangle g;
    ASSERT_EQ(overloaded::which(views::as_directed(g)), 1);
    ASSERT_EQ(overloaded::which(views::as_undirected(g)), 2);
}
