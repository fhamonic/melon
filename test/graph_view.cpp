#undef NDEBUG
#include <gtest/gtest.h>

#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include "melon/container/static_digraph.hpp"
#include "melon/graph.hpp"
#include "melon/views/graph_view.hpp"

#include "ranges_test_helper.hpp"

using G = melon::static_digraph;

////////////////////////////////////////////////////////////////////////////////
// graph_all wraps lvalues in ref views and rvalues in owning views, and passes
// an existing view through unchanged
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(graph_view, test) {
    static_assert(melon::graph<G>);

    static_assert(
        std::same_as<melon::views::graph_all_t<G &>, melon::graph_ref_view<G>>);
    static_assert(std::same_as<melon::views::graph_all_t<const G &>,
                               melon::graph_ref_view<const G>>);
    static_assert(std::same_as<melon::views::graph_all_t<G &&>,
                               melon::graph_owning_view<G>>);
    static_assert(std::same_as<melon::views::graph_all_t<G>,
                               melon::graph_owning_view<G>>);

    static_assert(melon::graph_view<melon::graph_ref_view<G>>);
    static_assert(melon::graph_view<melon::graph_ref_view<const G>>);
    static_assert(melon::graph_view<melon::graph_owning_view<G>>);

    static_assert(
        std::same_as<melon::views::graph_all_t<melon::graph_ref_view<G>>,
                     melon::graph_ref_view<G>>);
    static_assert(
        std::same_as<melon::views::graph_all_t<melon::graph_ref_view<const G>>,
                     melon::graph_ref_view<const G>>);
    static_assert(
        std::same_as<melon::views::graph_all_t<melon::graph_owning_view<G>>,
                     melon::graph_owning_view<G>>);
}

////////////////////////////////////////////////////////////////////////////////
// graph_ref_view binds to lvalues only, and copying a mutable lvalue view uses
// the copy constructor
////////////////////////////////////////////////////////////////////////////////

// regression: a guard spelled `!specialization_of<T, graph_ref_view>` over
// the deduced T sees `graph_ref_view<G> &` for a mutable lvalue -- a
// reference, therefore not a specialization. The guard passes, the template
// beats the copy constructor, and `static_cast<G &>` hard-errors.
static_assert(std::copy_constructible<melon::graph_ref_view<G>>);
static_assert(std::constructible_from<melon::graph_ref_view<G>, G &>);
static_assert(std::constructible_from<melon::graph_ref_view<G>,
                                      melon::graph_ref_view<G> &>);
static_assert(std::constructible_from<melon::graph_ref_view<const G>, G &>);

// The paired `convertible_to<T, G &>` constraint turns what would be a hard
// error inside the constructor body into an ordinary constraint failure.
static_assert(!std::constructible_from<melon::graph_ref_view<G>,
                                       melon::graph_ref_view<const G> &>);
static_assert(!std::constructible_from<melon::graph_ref_view<G>, int &>);

GTEST_TEST(graph_view, copying_a_mutable_lvalue_uses_the_copy_constructor) {
    G graph;
    melon::graph_ref_view<G> view(graph);

    melon::graph_ref_view<G> from_mutable_lvalue(view);
    ASSERT_EQ(std::addressof(from_mutable_lvalue.base()),
              std::addressof(graph));

    melon::graph_ref_view<G> from_const_lvalue(std::as_const(view));
    ASSERT_EQ(std::addressof(from_const_lvalue.base()), std::addressof(graph));
}

// The std::ranges::ref_view bindable-test: a temporary whose conversion
// materialises a graph must be rejected -- accepting it left the ref view
// pointing at an object dying at the end of the full-expression.
namespace {
struct materializing_graph_handle {
    G g;
    operator G &() { return g; }
    operator G() { return g; }
};
template <typename V, typename T>
concept ref_view_accepts = requires(T && t) { V{static_cast<T &&>(t)}; };
static_assert(
    !ref_view_accepts<melon::graph_ref_view<G>, materializing_graph_handle>);
static_assert(ref_view_accepts<melon::graph_ref_view<G>, G &>);
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// asking a wrapped graph for maps it does not have answers false instead of
// failing to compile
////////////////////////////////////////////////////////////////////////////////

// regression: with melon::create_vertex_map a *function* template in
// namespace melon, the has_adl_create_vertex_map probe inside the CPO finds
// it by ADL for any graph whose associated namespaces include melon -- i.e.
// every melon view. Asking has_vertex_map<> about a view wrapping a graph
// that has no vertex map then makes the constraint depend on itself:
//   error: satisfaction of atomic constraint ... depends on itself
// It has to answer `false`, not fail to compile. The CPOs are variable
// templates, which ADL cannot find.
namespace no_maps {
struct graph_without_maps {
    auto vertices() const { return std::views::iota(0u, 3u); }
    auto out_arcs(unsigned v) const { return std::views::iota(v, v + 1); }
    unsigned arc_target(unsigned a) const { return (a + 1) % 3; }
    unsigned arc_source(unsigned a) const { return a; }
};
}  // namespace no_maps

static_assert(melon::graph<no_maps::graph_without_maps>);
static_assert(!melon::has_vertex_map<no_maps::graph_without_maps>);
static_assert(!melon::has_arc_map<no_maps::graph_without_maps>);

// the wrapped forms are where the self-dependency strikes
static_assert(
    !melon::has_vertex_map<melon::graph_ref_view<no_maps::graph_without_maps>>);
static_assert(
    !melon::has_arc_map<melon::graph_ref_view<no_maps::graph_without_maps>>);
static_assert(!melon::has_vertex_map<
              melon::graph_owning_view<no_maps::graph_without_maps>>);

// a graph that does have them keeps answering true, wrapped or not
static_assert(melon::has_vertex_map<G>);
static_assert(melon::has_arc_map<G>);
static_assert(melon::has_vertex_map<melon::graph_ref_view<G>>);
static_assert(melon::has_arc_map<melon::graph_ref_view<G>>);
static_assert(melon::has_vertex_map<melon::graph_ref_view<const G>>);

////////////////////////////////////////////////////////////////////////////////
// the create-map CPOs remain customizable by ADL
////////////////////////////////////////////////////////////////////////////////

namespace adl_maps {
struct graph_with_adl_maps {
    auto vertices() const { return std::views::iota(0u, 3u); }
    auto out_arcs(unsigned v) const { return std::views::iota(v, v + 1); }
    unsigned arc_target(unsigned a) const { return (a + 1) % 3; }
    unsigned arc_source(unsigned a) const { return a; }
};
template <typename T>
std::vector<T> create_vertex_map(const graph_with_adl_maps &) {
    return std::vector<T>(3);
}
template <typename T>
std::vector<T> create_vertex_map(const graph_with_adl_maps &, const T & d) {
    return std::vector<T>(3, d);
}
}  // namespace adl_maps

static_assert(melon::has_vertex_map<adl_maps::graph_with_adl_maps>);
static_assert(melon::has_vertex_map<
              melon::graph_ref_view<adl_maps::graph_with_adl_maps>>);

GTEST_TEST(graph_view, adl_create_vertex_map_still_dispatches) {
    adl_maps::graph_with_adl_maps g;
    auto map = melon::create_vertex_map<int>(g, 42);
    ASSERT_EQ(map.size(), 3u);
    ASSERT_EQ(map[1], 42);

    // and through a view, which forwards to the same CPO
    auto view = melon::views::graph_all(g);
    auto view_map = melon::create_vertex_map<int>(view, 7);
    ASSERT_EQ(view_map.size(), 3u);
    ASSERT_EQ(view_map[1], 7);
}

////////////////////////////////////////////////////////////////////////////////
// create_*_map with a default value is noexcept only when that overload is
////////////////////////////////////////////////////////////////////////////////

namespace noexcept_probe {
// 0-argument overloads are noexcept, the default-value ones are not
struct graph_with_throwing_defaults {
    auto vertices() const { return std::views::iota(0u, 3u); }
    auto arcs() const { return std::views::iota(0u, 2u); }
    unsigned arc_source(unsigned a) const { return a; }
    unsigned arc_target(unsigned a) const { return a + 1; }
    template <typename T>
    auto create_vertex_map() const noexcept {
        return std::vector<T>(3);
    }
    template <typename T>
    auto create_vertex_map(const T & d) const {
        return std::vector<T>(3, d);
    }
    template <typename T>
    auto create_arc_map() const noexcept {
        return std::vector<T>(2);
    }
    template <typename T>
    auto create_arc_map(const T & d) const {
        return std::vector<T>(2, d);
    }
};
}  // namespace noexcept_probe

// regression: two CPO overloads sharing the 0-argument probe make the
// default-value call claim noexcept while it can throw
static_assert(noexcept(melon::create_vertex_map<int>(
    std::declval<const noexcept_probe::graph_with_throwing_defaults &>())));
static_assert(!noexcept(melon::create_vertex_map<int>(
    std::declval<const noexcept_probe::graph_with_throwing_defaults &>(),
    std::declval<const int &>())));
static_assert(noexcept(melon::create_arc_map<int>(
    std::declval<const noexcept_probe::graph_with_throwing_defaults &>())));
static_assert(!noexcept(melon::create_arc_map<int>(
    std::declval<const noexcept_probe::graph_with_throwing_defaults &>(),
    std::declval<const int &>())));

////////////////////////////////////////////////////////////////////////////////
// a malformed endpoint-map member is rejected and the synthesised map serves
// instead
////////////////////////////////////////////////////////////////////////////////

namespace bad_endpoint_maps {
// regression: without arc_sources_map()'s return-type constraint -- which its
// arc_targets_map twin carries -- a void-returning member is accepted and the
// CPO hands back void.
struct graph_with_void_maps {
    auto vertices() const { return std::views::iota(0u, 3u); }
    auto arcs() const { return std::views::iota(0u, 2u); }
    unsigned arc_source(unsigned a) const { return a; }
    unsigned arc_target(unsigned a) const { return a + 1; }
    void arc_sources_map() const {}
    void arc_targets_map() const {}
};
template <typename G>
concept sources_map_is_usable =
    requires(const G & g, unsigned a) { melon::arc_sources_map(g)[a]; };
template <typename G>
concept targets_map_is_usable =
    requires(const G & g, unsigned a) { melon::arc_targets_map(g)[a]; };
}  // namespace bad_endpoint_maps

static_assert(melon::graph<bad_endpoint_maps::graph_with_void_maps>);
// the bad members are rejected symmetrically, so both fall back
static_assert(!melon::cpo::has_member_arc_sources_map<
              bad_endpoint_maps::graph_with_void_maps>);
static_assert(!melon::cpo::has_member_arc_targets_map<
              bad_endpoint_maps::graph_with_void_maps>);
static_assert(bad_endpoint_maps::sources_map_is_usable<
              bad_endpoint_maps::graph_with_void_maps>);
static_assert(bad_endpoint_maps::targets_map_is_usable<
              bad_endpoint_maps::graph_with_void_maps>);
// a graph with real endpoint maps still uses its own members
static_assert(melon::cpo::has_member_arc_sources_map<G>);
static_assert(melon::cpo::has_member_arc_targets_map<G>);

GTEST_TEST(graph_view, void_endpoint_maps_fall_back_to_synthesised_ones) {
    bad_endpoint_maps::graph_with_void_maps g;
    auto sources = melon::arc_sources_map(g);
    auto targets = melon::arc_targets_map(g);
    for(const unsigned a : melon::arcs(g)) {
        ASSERT_EQ(sources[a], melon::arc_source(g, a));
        ASSERT_EQ(targets[a], melon::arc_target(g, a));
    }
}

////////////////////////////////////////////////////////////////////////////////
// a view forwards every member the CPOs probe, answering exactly as the
// wrapped graph does
////////////////////////////////////////////////////////////////////////////////

// regression: endpoint maps spelled `sources_map()` / `targets_map()` are
// names the CPO -- which looks for `arc_sources_map()` / `arc_targets_map()`
// -- never reaches. Nothing refers to such members, so every graph wrapped in
// a view silently loses its container's endpoint maps and gets the CPO's
// `maps::map(lambda)` fallback -- one indirect call per lookup where the
// container has a flat array. The tests below pin every
// member of both views against the graph they wrap, so a name that no CPO
// reaches shows up as a changed return *type*, not just as a still-correct
// value.

namespace passthrough {

// 3 vertices, arcs 0..4 = (0,1) (0,2) (1,2) (2,0) (2,1); sources sorted, as
// static_digraph's constructor asserts.
inline G make_graph() {
    const std::vector<std::pair<unsigned int, unsigned int>> arc_pairs(
        {{0u, 1u}, {0u, 2u}, {1u, 2u}, {2u, 0u}, {2u, 1u}});
    return G(3, std::views::keys(arc_pairs), std::views::values(arc_pairs));
}

// Everything the CPOs can ask of a view over `g` must answer as `g` does.
template <typename View>
void expect_same_as(const G & g, const View & view) {
    ASSERT_EQ(melon::num_vertices(view), melon::num_vertices(g));
    ASSERT_EQ(melon::num_arcs(view), melon::num_arcs(g));

    ASSERT_TRUE(EQ_RANGES(melon::vertices(view), melon::vertices(g)));
    ASSERT_TRUE(EQ_RANGES(melon::arcs(view), melon::arcs(g)));

    const auto sources = melon::arc_sources_map(view);
    const auto targets = melon::arc_targets_map(view);
    for(const auto & a : melon::arcs(g)) {
        ASSERT_EQ(melon::arc_source(view, a), melon::arc_source(g, a));
        ASSERT_EQ(melon::arc_target(view, a), melon::arc_target(g, a));
        ASSERT_EQ(sources[a], melon::arc_source(g, a));
        ASSERT_EQ(targets[a], melon::arc_target(g, a));
    }

    for(const auto & u : melon::vertices(g)) {
        ASSERT_TRUE(EQ_RANGES(melon::out_arcs(view, u), melon::out_arcs(g, u)));
        ASSERT_TRUE(EQ_RANGES(melon::in_arcs(view, u), melon::in_arcs(g, u)));
        ASSERT_TRUE(EQ_RANGES(melon::out_neighbors(view, u),
                              melon::out_neighbors(g, u)));
    }

    const auto vertex_map = melon::create_vertex_map<int>(view);
    ASSERT_EQ(vertex_map.size(), melon::num_vertices(g));
    const auto filled_vertex_map = melon::create_vertex_map<int>(view, 7);
    for(const auto & u : melon::vertices(g)) ASSERT_EQ(filled_vertex_map[u], 7);

    const auto arc_map = melon::create_arc_map<int>(view);
    ASSERT_EQ(arc_map.size(), melon::num_arcs(g));
    const auto filled_arc_map = melon::create_arc_map<int>(view, 9);
    for(const auto & a : melon::arcs(g)) ASSERT_EQ(filled_arc_map[a], 9);
}

}  // namespace passthrough

GTEST_TEST(graph_view, ref_view_forwards_every_member) {
    const G graph = passthrough::make_graph();
    passthrough::expect_same_as(graph, melon::graph_ref_view<const G>(graph));
}

GTEST_TEST(graph_view, owning_view_forwards_every_member) {
    const G graph = passthrough::make_graph();
    passthrough::expect_same_as(
        graph, melon::graph_owning_view<G>(passthrough::make_graph()));
}

// The two views must forward the *members* the CPOs look for, so wrapping a
// graph never degrades a lookup to the synthesised fallback. Type equality is
// what says so: the fallback is a `maps::map` over a lambda, never a
// `mapping_ref_view` over the container's own array.
template <typename View>
concept keeps_endpoint_maps =
    melon::cpo::has_member_arc_sources_map<View> &&
    melon::cpo::has_member_arc_targets_map<View> &&
    std::same_as<decltype(melon::arc_sources_map(std::declval<const View &>())),
                 decltype(melon::arc_sources_map(std::declval<const G &>()))> &&
    std::same_as<decltype(melon::arc_targets_map(std::declval<const View &>())),
                 decltype(melon::arc_targets_map(std::declval<const G &>()))>;

static_assert(keeps_endpoint_maps<melon::graph_ref_view<G>>);
static_assert(keeps_endpoint_maps<melon::graph_ref_view<const G>>);
static_assert(keeps_endpoint_maps<melon::graph_owning_view<G>>);

// and the wrapping is transparent to the concepts, at every nesting depth
static_assert(melon::has_arc_sources_map<melon::graph_ref_view<G>>);
static_assert(melon::has_arc_targets_map<melon::graph_ref_view<G>>);
static_assert(keeps_endpoint_maps<
              melon::graph_ref_view<const melon::graph_ref_view<const G>>>);

// A graph with no endpoint map of its own is served by the CPO's synthesised
// one. The view's member is guarded by has_arc_*_map, which that fallback
// satisfies, so the view does grow the member -- it forwards to the CPO, which
// synthesises the map over the *inner* graph. The point is that the answer
// comes from one layer either way, never from a lambda wrapping a lambda.
static_assert(
    !melon::cpo::has_member_arc_sources_map<no_maps::graph_without_maps>);
static_assert(
    melon::has_arc_sources_map<no_maps::graph_without_maps>);  // via fallback
static_assert(melon::has_arc_sources_map<
              melon::graph_ref_view<no_maps::graph_without_maps>>);
static_assert(
    std::same_as<decltype(melon::arc_sources_map(
                     std::declval<const melon::graph_ref_view<
                         no_maps::graph_without_maps> &>())),
                 decltype(melon::arc_sources_map(
                     std::declval<const no_maps::graph_without_maps &>()))>);

GTEST_TEST(graph_view, wrapping_a_mapless_graph_still_yields_usable_maps) {
    no_maps::graph_without_maps graph;
    const melon::graph_ref_view<no_maps::graph_without_maps> view(graph);
    const auto sources = melon::arc_sources_map(view);
    const auto targets = melon::arc_targets_map(view);
    for(const unsigned int a : {0u, 1u, 2u}) {
        ASSERT_EQ(sources[a], melon::arc_source(graph, a));
        ASSERT_EQ(targets[a], melon::arc_target(graph, a));
    }
}

////////////////////////////////////////////////////////////////////////////////
// a view has exactly the capabilities of the graph it wraps -- no more, no
// less
////////////////////////////////////////////////////////////////////////////////

// Capabilities the wrapped graph does not have must not appear on the view
// either -- each view member is guarded by the concept for what it forwards
// to. graph_without_maps has out_arcs but no in_arcs, so neither it nor a view
// over it can list in-arcs, and in_neighbors is not synthesisable either.
static_assert(!melon::has_in_arcs<no_maps::graph_without_maps>);
static_assert(
    !melon::has_in_arcs<melon::graph_ref_view<no_maps::graph_without_maps>>);
static_assert(!melon::inward_adjacency_graph<no_maps::graph_without_maps>);
static_assert(!melon::inward_adjacency_graph<
              melon::graph_ref_view<no_maps::graph_without_maps>>);
static_assert(
    melon::has_out_arcs<melon::graph_ref_view<no_maps::graph_without_maps>>);

// static_digraph has all four, so a view over it does too
static_assert(melon::outward_incidence_graph<melon::graph_ref_view<G>>);
static_assert(melon::inward_incidence_graph<melon::graph_ref_view<G>>);
static_assert(melon::outward_adjacency_graph<melon::graph_ref_view<G>>);
static_assert(melon::inward_adjacency_graph<melon::graph_ref_view<G>>);
