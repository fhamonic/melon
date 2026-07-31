#undef NDEBUG
#include <gtest/gtest.h>

#include <functional>
#include <tuple>
#include <vector>

#include "melon/container/mutable_digraph.hpp"
#include "melon/utility/make_static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

using namespace melon;

namespace {
struct not_a_graph {};
template <typename T>
concept can_make_static_digraph =
    requires(const T & t) { make_static_digraph(t); };
}  // namespace

// The overload participates only for graphs it can actually rebuild: it walks
// out_arcs and needs a vertex map for the renumbering.
static_assert(can_make_static_digraph<static_digraph>);
static_assert(can_make_static_digraph<mutable_digraph>);
static_assert(!can_make_static_digraph<not_a_graph>);

////////////////////////////////////////////////////////////////////////////////
// vertices are renumbered in comparator order, arcs and maps follow
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(make_static_digraph, reorders_vertices_arcs_and_maps) {
    static_digraph_builder<static_digraph, int> b(3);
    b.add_arc(0u, 1u, 100).add_arc(0u, 2u, 101).add_arc(1u, 2u, 102);
    auto [g, arc_prop] = std::move(b).build();
    auto vertex_prop = g.create_vertex_map<int>();
    vertex_prop[0u] = 10;
    vertex_prop[1u] = 11;
    vertex_prop[2u] = 12;

    // Descending order: new 0 is old 2, new 1 is old 1, new 2 is old 0. The
    // maps ride along as a reference tuple -- nothing is copied on the way in.
    auto [sg, new_vprop, new_aprop] = make_static_digraph(
        g, std::greater<unsigned>{}, std::tie(vertex_prop), std::tie(arc_prop));
    static_assert(std::same_as<decltype(sg), static_digraph>);

    EXPECT_EQ(melon::num_vertices(sg), 3u);
    EXPECT_EQ(melon::num_arcs(sg), 3u);
    std::vector<std::pair<unsigned, std::pair<unsigned, unsigned>>> entries;
    for(auto && [a, uv] : melon::arcs_entries(sg)) entries.emplace_back(a, uv);
    EXPECT_EQ(entries,
              (std::vector<std::pair<unsigned, std::pair<unsigned, unsigned>>>{
                  {0u, {1u, 0u}}, {1u, {2u, 1u}}, {2u, {2u, 0u}}}));

    EXPECT_EQ(new_vprop[0u], 12);
    EXPECT_EQ(new_vprop[1u], 11);
    EXPECT_EQ(new_vprop[2u], 10);
    // New arcs are emitted grouped by new source: old arc (1,2) first, then
    // old (0,1) and old (0,2).
    EXPECT_EQ(new_aprop[0u], 102);
    EXPECT_EQ(new_aprop[1u], 100);
    EXPECT_EQ(new_aprop[2u], 101);
}

////////////////////////////////////////////////////////////////////////////////
// every argument after the graph is optional
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(make_static_digraph, defaults_to_ascending_order_and_no_maps) {
    static_digraph_builder<static_digraph> b(3);
    b.add_arc(0u, 1u).add_arc(1u, 2u);
    auto [g] = std::move(b).build();

    auto [sg] = make_static_digraph(g);
    EXPECT_EQ(melon::num_vertices(sg), 3u);
    EXPECT_EQ(melon::num_arcs(sg), 2u);
    std::size_t count = 0;
    for(auto && [a, uv] : melon::arcs_entries(sg)) {
        auto && [s, t] = uv;
        EXPECT_EQ(melon::arc_source(g, a), s);
        EXPECT_EQ(melon::arc_target(g, a), t);
        ++count;
    }
    EXPECT_EQ(count, 2u);
}

////////////////////////////////////////////////////////////////////////////////
// a mutable_digraph with holes compacts to a dense static_digraph
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(make_static_digraph, compacts_a_graph_with_holes) {
    mutable_digraph g;
    const auto a = g.create_vertex();
    const auto b = g.create_vertex();
    const auto c = g.create_vertex();
    (void)g.create_arc(a, b);
    const auto dead = g.create_arc(b, c);
    (void)g.create_arc(a, c);
    g.remove_arc(dead);
    g.remove_vertex(c);  // also removes (a, c)

    auto [sg] = make_static_digraph(g);
    EXPECT_EQ(melon::num_vertices(sg), 2u);
    EXPECT_EQ(melon::num_arcs(sg), 1u);
    for(auto && [arc, uv] : melon::arcs_entries(sg)) {
        EXPECT_EQ(uv.first, 0u);
        EXPECT_EQ(uv.second, 1u);
    }
}
