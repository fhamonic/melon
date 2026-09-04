#undef NDEBUG
#include <gtest/gtest.h>

#include <concepts>
#include <memory>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>

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

    builder.add_arc({3, 4});
    builder.add_arc({1, 7});
    builder.add_arc({5, 2});
    builder.add_arc({2, 4});
    builder.add_arc({5, 3});
    builder.add_arc({6, 5});
    builder.add_arc({1, 2});
    builder.add_arc({1, 6});
    builder.add_arc({2, 3});

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

    for(auto & [u, v] : pairs) builder.add_arc({u, v}, weight(u, v));

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

// regression: build() has an rvalue-qualified overload that moves the
// property vectors instead of copying -- and add_arc is ref-qualified so that
// value category survives a chain: returning `static_digraph_builder &`
// unconditionally makes std::move(b).add_arc(...) an *lvalue*, which sends
// the following .build() straight back to the copying overload.
namespace build_overloads {
using builder = static_digraph_builder<static_digraph, int>;

template <typename B>
using add_arc_result = decltype(std::declval<B>().add_arc({0u, 1u}, 0));

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

// An lvalue chain stays an lvalue, an rvalue chain stays an rvalue; without
// the ref-qualifiers both are `builder &`.
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
    b.add_arc({0, 1}, counted{7}).add_arc({1, 2}, counted{8});
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
            .add_arc({0, 1}, counted{7})
            .add_arc({1, 2}, counted{8})
            .build();

    ASSERT_EQ(num_vertices(graph), 3u);
    ASSERT_EQ(properties[0].v, 7);
    ASSERT_EQ(properties[1].v, 8);
}

// and the plain, unchained form is untouched
GTEST_TEST(static_digraph_builder, lvalue_chain_still_builds_normally) {
    static_digraph_builder<static_digraph, int> builder(3);
    builder.add_arc({0, 1}, 7).add_arc({1, 2}, 8);
    auto [graph, lengths] = builder.build();
    ASSERT_EQ(num_arcs(graph), 2u);
    ASSERT_TRUE(EQ_RANGES(lengths, {7, 8}));
}

////////////////////////////////////////////////////////////////////////////////
// add_arc takes the endpoints as one pair; the positional (u, v, ...)
// spelling survives only on a property-less builder, where nothing can
// follow the endpoints. add_arcs appends a range of pairs or of
// (pair, properties...) tuple-likes, in either value category
////////////////////////////////////////////////////////////////////////////////

namespace add_arcs_shapes {
using plain = static_digraph_builder<static_digraph>;
using weighted = static_digraph_builder<static_digraph, int>;
using two_props = static_digraph_builder<static_digraph, int, double>;
using owning = static_digraph_builder<static_digraph, std::unique_ptr<int>>;
using vertex = vertex_t<static_digraph>;
using endpoints = std::pair<vertex, vertex>;

template <typename B, typename... Args>
concept add_arc_callable =
    requires(B & b, Args... args) { b.add_arc(args...); };
template <typename B, typename R>
concept add_arcs_callable =
    requires(B & b, R && r) { b.add_arcs(std::forward<R>(r)); };
}  // namespace add_arcs_shapes

static_assert(add_arcs_shapes::add_arc_callable<
              add_arcs_shapes::weighted, add_arcs_shapes::endpoints, int>);
static_assert(!add_arcs_shapes::add_arc_callable<add_arcs_shapes::weighted,
                                                 unsigned, unsigned, int>);
static_assert(add_arcs_shapes::add_arc_callable<add_arcs_shapes::plain,
                                                unsigned, unsigned>);
static_assert(add_arcs_shapes::add_arc_callable<add_arcs_shapes::plain,
                                                add_arcs_shapes::endpoints>);
static_assert(!add_arcs_shapes::add_arc_callable<add_arcs_shapes::weighted,
                                                 unsigned, unsigned>);

// a property-less builder takes a range of pairs; a builder with properties
// takes tuple-likes of the matching arity, and rejects bare pairs or the
// wrong number of fields
static_assert(add_arcs_shapes::add_arcs_callable<
              add_arcs_shapes::plain, std::vector<add_arcs_shapes::endpoints>>);
static_assert(
    !add_arcs_shapes::add_arcs_callable<
        add_arcs_shapes::weighted, std::vector<add_arcs_shapes::endpoints>>);
static_assert(add_arcs_shapes::add_arcs_callable<
              add_arcs_shapes::weighted,
              std::vector<std::tuple<add_arcs_shapes::endpoints, int>>>);
static_assert(
    add_arcs_shapes::add_arcs_callable<
        add_arcs_shapes::two_props,
        std::vector<std::tuple<add_arcs_shapes::endpoints, int, double>>>);
static_assert(!add_arcs_shapes::add_arcs_callable<
              add_arcs_shapes::two_props,
              std::vector<std::tuple<add_arcs_shapes::endpoints, int>>>);

// properties are copied out of the entries, so a move-only property is
// rejected while the range yields lvalues -- an rvalue vector included, its
// elements are still lvalues -- and accepted once as_rvalue makes them rvalues
namespace add_arcs_shapes {
using owning_entries = std::vector<std::tuple<endpoints, std::unique_ptr<int>>>;
}  // namespace add_arcs_shapes
static_assert(!add_arcs_shapes::add_arcs_callable<
              add_arcs_shapes::owning, add_arcs_shapes::owning_entries &>);
static_assert(!add_arcs_shapes::add_arcs_callable<
              add_arcs_shapes::owning, add_arcs_shapes::owning_entries>);
static_assert(add_arcs_shapes::add_arcs_callable<
              add_arcs_shapes::owning,
              decltype(std::declval<add_arcs_shapes::owning_entries &>() |
                       std::views::as_rvalue)>);

// copy vs move is observed on the source entries: a moved-from `drained`
// reads -1, a copied one keeps its value. Copyable on purpose -- build()
// sorts through libstdc++'s std::sort, which copies the zip proxy, so a
// move-only property cannot reach the built map to be checked there
namespace add_arcs_shapes {
struct drained {
    int value;
    drained(int v) : value(v) {}
    drained(const drained & o) : value(o.value) {}
    drained(drained && o) noexcept : value(std::exchange(o.value, -1)) {}
    drained & operator=(const drained &) = default;
    drained & operator=(drained && o) noexcept {
        value = std::exchange(o.value, -1);
        return *this;
    }
};
using draining = static_digraph_builder<static_digraph, drained>;
using drained_entries = std::vector<std::tuple<endpoints, drained>>;
}  // namespace add_arcs_shapes
GTEST_TEST(static_digraph_builder, add_arcs_copies_unless_as_rvalue) {
    using namespace add_arcs_shapes;
    drained_entries entries{{{2, 0}, 20}, {{0, 1}, 1}, {{1, 2}, 12}};

    draining copying(3);
    copying.add_arcs(entries);
    for(const auto & entry : entries) ASSERT_NE(std::get<1>(entry).value, -1);
    auto [copied_graph, copied] = std::move(copying).build();
    ASSERT_EQ(num_arcs(copied_graph), 3u);
    // sorted by source: (0,1) (1,2) (2,0)
    ASSERT_TRUE(EQ_RANGES(copied | std::views::transform(&drained::value),
                          {1, 12, 20}));

    draining moving(3);
    moving.add_arcs(entries | std::views::as_rvalue);
    for(const auto & entry : entries) ASSERT_EQ(std::get<1>(entry).value, -1);
    auto [moved_graph, moved] = std::move(moving).build();
    ASSERT_EQ(num_arcs(moved_graph), 3u);
    ASSERT_TRUE(
        EQ_RANGES(moved | std::views::transform(&drained::value), {1, 12, 20}));
}

GTEST_TEST(static_digraph_builder, add_arcs_from_a_range_of_pairs) {
    using namespace add_arcs_shapes;
    std::vector<endpoints> pairs{{3, 4}, {1, 7}, {5, 2}};
    plain builder(8);
    builder.add_arcs(pairs).add_arc({1, 2}).add_arc(1, 3);
    auto [graph] = builder.build();
    ASSERT_EQ(num_arcs(graph), 5u);
    ASSERT_TRUE(EQ_RANGES(out_arcs(graph, 1u), {0u, 1u, 2u}));
    ASSERT_EQ(arc_target(graph, 0u), 2u);
    ASSERT_EQ(arc_target(graph, 1u), 3u);
    ASSERT_EQ(arc_target(graph, 2u), 7u);
}

// the zip of an endpoints range with the property ranges is the shape the
// bulk form is built for; an input range without a size takes the same path
// without the reserve
GTEST_TEST(static_digraph_builder, add_arcs_from_zipped_properties) {
    using namespace add_arcs_shapes;
    std::vector<endpoints> pairs{{2, 0}, {0, 1}, {1, 2}};
    std::vector<int> weights{20, 1, 12};
    std::vector<double> lengths{2.0, 0.1, 1.2};

    two_props builder(3);
    builder.add_arcs(std::views::zip(pairs, weights, lengths));
    auto [graph, weight, length] = builder.build();

    ASSERT_EQ(num_arcs(graph), 3u);
    // sorted by source: (0,1) (1,2) (2,0)
    ASSERT_TRUE(EQ_RANGES(weight, {1, 12, 20}));
    ASSERT_TRUE(EQ_RANGES(length, {0.1, 1.2, 2.0}));
}

// arcs_entries of one graph feed the builder of another: values() strips the
// arc id, and the arc map is read through the id range zipped alongside
GTEST_TEST(static_digraph_builder, add_arcs_copies_another_graph) {
    using namespace add_arcs_shapes;
    auto [source, source_length] = weighted(4)
                                       .add_arc({0, 1}, 7)
                                       .add_arc({1, 2}, 8)
                                       .add_arc({0, 3}, 9)
                                       .build();
    weighted copy(num_vertices(source));
    copy.add_arcs(std::views::zip(
        arcs_entries(source) | std::views::values,
        arcs(source) |
            std::views::transform([&](auto a) { return source_length[a]; })));
    auto [graph, length] = copy.build();

    ASSERT_TRUE(EQ_RANGES(arcs_entries(graph), arcs_entries(source)));
    ASSERT_TRUE(EQ_RANGES(length, source_length));
}

GTEST_TEST(static_digraph_builder, add_arcs_keeps_the_chain_value_category) {
    using namespace add_arcs_shapes;
    std::vector<std::tuple<endpoints, int>> entries{{{0, 1}, 7}, {{1, 2}, 8}};
    static_assert(
        std::same_as<decltype(std::declval<weighted &>().add_arcs(entries)),
                     weighted &>);
    static_assert(
        std::same_as<decltype(std::declval<weighted &&>().add_arcs(entries)),
                     weighted &&>);
    auto [graph, length] = weighted(3).add_arcs(entries).build();
    ASSERT_EQ(num_arcs(graph), 2u);
    ASSERT_TRUE(EQ_RANGES(length, {7, 8}));
}
