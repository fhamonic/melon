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
// so the has_adl_create_vertex_map probe inside the CPO found it by ADL for any
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

// ####### regression: 2-arg create_*_map noexcept / arc_sources_map ##########

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

// both CPO overloads used to share the 0-argument probe, so the default-value
// call claimed noexcept while it could throw
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

namespace bad_endpoint_maps {
// arc_sources_map()'s return-type constraint had been commented out while its
// arc_targets_map twin kept it, so a void-returning member was accepted and the
// CPO handed back void.
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
