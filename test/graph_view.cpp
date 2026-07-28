#undef NDEBUG
#include <gtest/gtest.h>

#include <iostream>
#include <ranges>
#include <vector>

#include "melon/container/static_digraph.hpp"
#include "melon/graph.hpp"
#include "melon/views/graph_view.hpp"

using G = melon::static_digraph;

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
// ############ regression: has_vertex_map on a wrapped graph ##################

// melon::create_vertex_map used to be a *function* template in namespace melon,
// so the __adl_create_vertex_map probe inside the CPO found it by ADL for any
// graph whose associated namespaces include melon -- i.e. every melon view.
// Asking has_vertex_map<> about a view wrapping a graph that has no vertex map
// then made the constraint depend on itself:
//   error: satisfaction of atomic constraint ... depends on itself
// It has to answer `false`, not fail to compile. The CPOs are now variable
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

// the wrapped forms are what used to be a hard error
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

// ############ the CPOs are still customizable by ADL #########################

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
static_assert(
    melon::has_vertex_map<melon::graph_ref_view<adl_maps::graph_with_adl_maps>>);

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
