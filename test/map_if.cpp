#undef NDEBUG
#include <gtest/gtest.h>

#include <concepts>
#include <type_traits>
#include <utility>

#include "melon/container/static_digraph.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/utility/static_digraph_builder.hpp"

using namespace melon;

using enabled_vertex_map = detail::vertex_map_if<true, static_digraph, int>;
using disabled_vertex_map = detail::vertex_map_if<false, static_digraph, int>;
using enabled_arc_map = detail::arc_map_if<true, static_digraph, int>;
using disabled_arc_map = detail::arc_map_if<false, static_digraph, int>;

////////////////////////////////////////////////////////////////////////////////
// the disabled variant mirrors the enabled constructor signatures exactly, so
// a malformed construction fails whatever the condition -- not only for the
// first user whose traits turn the map on
////////////////////////////////////////////////////////////////////////////////

static_assert(
    std::constructible_from<enabled_vertex_map, const static_digraph &>);
static_assert(std::constructible_from<enabled_vertex_map,
                                      const static_digraph &, const int &>);
static_assert(
    std::constructible_from<disabled_vertex_map, const static_digraph &>);
static_assert(std::constructible_from<disabled_vertex_map,
                                      const static_digraph &, const int &>);
static_assert(!std::constructible_from<disabled_vertex_map, int>);
static_assert(!std::constructible_from<disabled_vertex_map,
                                       const static_digraph &, const char *>);
static_assert(
    !std::constructible_from<disabled_vertex_map, double, const char *, int>);
static_assert(
    !std::constructible_from<enabled_vertex_map, double, const char *, int>);

// the arc twin must not drift from the vertex one
static_assert(std::constructible_from<enabled_arc_map, const static_digraph &,
                                      const int &>);
static_assert(std::constructible_from<disabled_arc_map, const static_digraph &,
                                      const int &>);
static_assert(!std::constructible_from<disabled_arc_map, const static_digraph &,
                                       const char *>);
static_assert(
    !std::constructible_from<disabled_arc_map, double, const char *, int>);

////////////////////////////////////////////////////////////////////////////////
// a holder's defaulted default constructor survives its map condition turning
// on: the enabled variant is default-constructible whenever the underlying
// map type is, handing out the map's valid moved-from empty state
////////////////////////////////////////////////////////////////////////////////

static_assert(std::default_initializable<enabled_vertex_map>);
static_assert(std::default_initializable<disabled_vertex_map>);
static_assert(std::default_initializable<enabled_arc_map>);
static_assert(std::default_initializable<disabled_arc_map>);

namespace {
struct enabled_holder {
    [[no_unique_address]] enabled_vertex_map map;
    enabled_holder() = default;
};
}  // namespace
static_assert(std::default_initializable<enabled_holder>);

////////////////////////////////////////////////////////////////////////////////
// the disabled variant is an empty type: distinct DiscriminatingT lets two
// [[no_unique_address]] members overlap completely, and sharing the default
// tag costs the documented byte of padding. MSVC ignores the standard
// attribute spelling, so the size pins are for the compilers that honor it
////////////////////////////////////////////////////////////////////////////////

static_assert(std::is_empty_v<disabled_vertex_map>);
static_assert(std::is_empty_v<disabled_arc_map>);

namespace {
struct first_tag;
struct second_tag;
struct two_distinct_tags {
    [[no_unique_address]] detail::vertex_map_if<false, static_digraph, int,
                                                first_tag> a;
    [[no_unique_address]] detail::vertex_map_if<false, static_digraph, int,
                                                second_tag> b;
};
struct two_shared_tags {
    [[no_unique_address]] disabled_vertex_map a;
    [[no_unique_address]] disabled_vertex_map b;
};
}  // namespace
#if !defined(_MSC_VER)
static_assert(sizeof(two_distinct_tags) == 1);
static_assert(sizeof(two_shared_tags) == 2);
#endif

////////////////////////////////////////////////////////////////////////////////
// the enabled variant forwards construction defaults, subscripts and fill
// into the underlying map, yielding references rather than copies, and is
// constructible from a const graph
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(map_if, forwards_into_the_underlying_map) {
    static_digraph_builder<static_digraph, int> builder(3);
    builder.add_arc(0u, 1u, 0).add_arc(1u, 2u, 0);
    auto [graph, unused] = builder.build();
    const static_digraph & const_graph = graph;

    detail::vertex_map_if<true, static_digraph, int> vertex_map(const_graph, 7);
    static_assert(std::is_lvalue_reference_v<decltype(vertex_map[0u])>);
    static_assert(
        std::is_lvalue_reference_v<decltype(std::as_const(vertex_map)[0u])>);
    static_assert(noexcept(vertex_map[0u]));
    for(auto && v : vertices(graph)) ASSERT_EQ(vertex_map[v], 7);
    vertex_map[1u] = 3;
    ASSERT_EQ(vertex_map[1u], 3);
    vertex_map.fill(1);
    for(auto && v : vertices(graph)) ASSERT_EQ(vertex_map[v], 1);

    detail::arc_map_if<true, static_digraph, int> arc_map(const_graph, 5);
    static_assert(std::is_lvalue_reference_v<decltype(arc_map[0u])>);
    static_assert(noexcept(arc_map[0u]));
    for(auto && a : arcs(graph)) ASSERT_EQ(arc_map[a], 5);
    arc_map[0u] = 2;
    ASSERT_EQ(arc_map[0u], 2);
    arc_map.fill(4);
    for(auto && a : arcs(graph)) ASSERT_EQ(arc_map[a], 4);
}
