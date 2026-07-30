#undef NDEBUG
#include <gtest/gtest.h>

#include <concepts>
#include <utility>

#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// the builder accepts arcs in any order and build() sorts them by source,
// carrying the optional per-arc property maps along
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(static_digraph_builder, build_without_map) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(3, 4);
    builder.add_arc(1, 7);
    builder.add_arc(5, 2);
    builder.add_arc(2, 4);
    builder.add_arc(5, 3);
    builder.add_arc(6, 5);
    builder.add_arc(1, 2);
    builder.add_arc(1, 6);
    builder.add_arc(2, 3);

    auto [graph] = builder.build();

    ASSERT_TRUE(EQ_RANGES(
        arcs_entries(graph),
        std::vector<std::pair<
            arc_t<static_digraph>,
            std::pair<vertex_t<static_digraph>, vertex_t<static_digraph>>>>(
            {{0, {1, 2}},
             {1, {1, 6}},
             {2, {1, 7}},
             {3, {2, 3}},
             {4, {2, 4}},
             {5, {3, 4}},
             {6, {5, 2}},
             {7, {5, 3}},
             {8, {6, 5}}})));
}

GTEST_TEST(static_digraph_builder, build_with_map) {
    constexpr std::size_t n = 8;
    static_digraph_builder<static_digraph, int> builder(n);

    std::vector<std::pair<vertex_t<static_digraph>, vertex_t<static_digraph>>>
        pairs{{3, 4}, {1, 7}, {5, 2}, {2, 4}, {5, 3},
              {6, 5}, {1, 2}, {1, 6}, {2, 3}};

    auto weight = [](vertex_t<static_digraph> u, vertex_t<static_digraph> v) {
        return static_cast<int>(u * n + v);
    };

    for(auto & [u, v] : pairs) builder.add_arc(u, v, weight(u, v));

    auto [graph, map] = builder.build();

    ASSERT_TRUE(EQ_RANGES(
        arcs_entries(graph),
        std::vector<std::pair<
            arc_t<static_digraph>,
            std::pair<vertex_t<static_digraph>, vertex_t<static_digraph>>>>(
            {{0, {1, 2}},
             {1, {1, 6}},
             {2, {1, 7}},
             {3, {2, 3}},
             {4, {2, 4}},
             {5, {3, 4}},
             {6, {5, 2}},
             {7, {5, 3}},
             {8, {6, 5}}})));

    for(arc_t<static_digraph> a : arcs(graph)) {
        auto u = arc_source(graph, a);
        auto v = arc_target(graph, a);
        ASSERT_EQ(map[a], weight(u, v));
    }
}

////////////////////////////////////////////////////////////////////////////////
// build() on an rvalue builder moves the property vectors, and add_arc
// preserves the chain's value category
////////////////////////////////////////////////////////////////////////////////

// regression (2.8): build() copied the property vectors unconditionally. There
// is now an rvalue-qualified overload that moves them instead -- and add_arc
// is ref-qualified so that value category survives a chain: returning
// `static_digraph_builder &` unconditionally made std::move(b).add_arc(...) an
// *lvalue*, which sent the following .build() straight back to the copying
// overload.
namespace build_overloads {
using builder = static_digraph_builder<static_digraph, int>;

template <typename B>
using add_arc_result = decltype(std::declval<B>().add_arc(0u, 1u, 0));

// counts its own copy constructions, which is what tells the two build()
// overloads apart: they return the same *type*, so only the copying is visible
struct counted {
    static inline int copies = 0;
    int v = 0;
    counted() = default;
    counted(int x) : v(x) {}
    counted(const counted & o) : v(o.v) { ++copies; }
    counted(counted &&) = default;
    counted & operator=(const counted &) = default;
    counted & operator=(counted &&) = default;
};
}  // namespace build_overloads

// This is the fix: an lvalue chain stays an lvalue, an rvalue chain stays an
// rvalue. Without the ref-qualifiers both were `builder &`.
static_assert(
    std::same_as<build_overloads::add_arc_result<build_overloads::builder &>,
                 build_overloads::builder &>);
static_assert(
    std::same_as<build_overloads::add_arc_result<build_overloads::builder &&>,
                 build_overloads::builder &&>);

// both build() overloads exist, and neither is callable on a const builder
// (both sort in place). Spelled as concepts so the requires-expressions are
// dependent and answer instead of diagnosing.
namespace build_overloads {
template <typename B>
concept lvalue_buildable = requires(B & b) { b.build(); };
template <typename B>
concept rvalue_buildable = requires(B & b) { std::move(b).build(); };
template <typename B>
concept const_buildable = requires(const B & b) { b.build(); };
}  // namespace build_overloads

static_assert(build_overloads::lvalue_buildable<build_overloads::builder>);
static_assert(build_overloads::rvalue_buildable<build_overloads::builder>);
static_assert(!build_overloads::const_buildable<build_overloads::builder>);

namespace build_overloads {
inline auto make_counted_builder() {
    static_digraph_builder<static_digraph, counted> b(3);
    b.add_arc(0, 1, counted{7}).add_arc(1, 2, counted{8});
    return b;
}
}  // namespace build_overloads

// The two overloads return the same *type*, so only the copying tells them
// apart. Measured as a difference rather than against zero: sorting the zipped
// arc list copies each property once on libstdc++ (ranges::sort reaches the
// zip_view's proxy reference through _GLIBCXX_MOVE(*it), which binds the
// element as an lvalue), and that copy is common to both overloads. What
// build() itself adds is one copy per property vector element -- exactly what
// build() && drops.
GTEST_TEST(static_digraph_builder, rvalue_build_moves_the_property_vectors) {
    using counted = build_overloads::counted;

    auto lvalue_builder = build_overloads::make_counted_builder();
    counted::copies = 0;
    auto [graph1, properties1] = lvalue_builder.build();
    const int lvalue_copies = counted::copies;

    auto rvalue_builder = build_overloads::make_counted_builder();
    counted::copies = 0;
    auto [graph2, properties2] = std::move(rvalue_builder).build();
    const int rvalue_copies = counted::copies;

    ASSERT_EQ(lvalue_copies - rvalue_copies, 2);  // the two property elements

    // and both produce the same answer
    ASSERT_EQ(num_arcs(graph2), 2u);
    ASSERT_EQ(properties2.size(), 2u);
    ASSERT_EQ(properties2[0].v, properties1[0].v);
    ASSERT_EQ(properties2[1].v, properties1[1].v);
    ASSERT_EQ(properties2[0].v, 7);
    ASSERT_EQ(properties2[1].v, 8);
}

// the lvalue overload copies, so the builder survives it
GTEST_TEST(static_digraph_builder, lvalue_build_keeps_the_builder) {
    auto builder = build_overloads::make_counted_builder();

    auto [graph, properties] = builder.build();
    ASSERT_EQ(num_arcs(graph), 2u);
    ASSERT_EQ(properties[0].v, 7);

    // built from a copy, so the builder still holds its own arcs
    auto [graph2, properties2] = builder.build();
    ASSERT_EQ(num_arcs(graph2), 2u);
    ASSERT_EQ(properties2[0].v, 7);
    ASSERT_EQ(properties2[1].v, 8);
}

GTEST_TEST(static_digraph_builder, chaining_on_a_temporary_moves) {
    using counted = build_overloads::counted;
    counted::copies = 0;
    auto [graph, properties] =
        static_digraph_builder<static_digraph, counted>(3)
            .add_arc(0, 1, counted{7})
            .add_arc(1, 2, counted{8})
            .build();

    ASSERT_EQ(num_vertices(graph), 3u);
    ASSERT_EQ(properties[0].v, 7);
    ASSERT_EQ(properties[1].v, 8);
}

// and the plain, unchained form is untouched
GTEST_TEST(static_digraph_builder, lvalue_chain_still_builds_normally) {
    static_digraph_builder<static_digraph, int> builder(3);
    builder.add_arc(0, 1, 7).add_arc(1, 2, 8);
    auto [graph, lengths] = builder.build();
    ASSERT_EQ(num_arcs(graph), 2u);
    ASSERT_TRUE(EQ_RANGES(lengths, {7, 8}));
}
