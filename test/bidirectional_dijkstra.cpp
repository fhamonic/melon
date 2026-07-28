#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/bidirectional_dijkstra.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

GTEST_TEST(bidirectional_dijkstra, test) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 7);
    builder.add_arc(0, 2, 9);
    builder.add_arc(0, 5, 14);
    builder.add_arc(1, 0, 7);
    builder.add_arc(1, 2, 10);
    builder.add_arc(1, 3, 15);
    builder.add_arc(2, 0, 9);
    builder.add_arc(2, 1, 10);
    builder.add_arc(2, 3, 12);
    builder.add_arc(2, 5, 2);
    builder.add_arc(3, 1, 15);
    builder.add_arc(3, 2, 12);
    builder.add_arc(3, 4, 6);
    builder.add_arc(4, 3, 6);
    builder.add_arc(4, 5, 9);
    builder.add_arc(5, 0, 14);
    builder.add_arc(5, 2, 2);
    builder.add_arc(5, 4, 9);

    auto [graph, length_map] = builder.build();

    bidirectional_dijkstra alg(graph, length_map, 0u, 3u);
    ASSERT_EQ(alg.run(), 21);
    ASSERT_TRUE(alg.path_found());
    ASSERT_TRUE(EQ_MULTISETS(alg.path(), {1, 8}));
    alg.reset();
}

// ##################### traits: store_path = false ###########################

// bidirectional_dijkstra is the one algorithm whose flag defaults to *true*,
// so the distance-only configuration was never instantiated: neither the
// vertex_map_if that drops both predecessor maps, nor the requires-clauses
// that are supposed to withdraw the path accessors along with them.
namespace {
struct bidirectional_dijkstra_pathless_traits
    : bidirectional_dijkstra_default_traits<static_digraph, int> {
    static constexpr bool store_path = false;
};

template <typename A>
concept has_path_found = requires(const A & a) { a.path_found(); };
template <typename A>
concept has_path = requires(const A & a) { a.path(); };
template <typename A>
concept has_pred_arc = requires(
    const A & a, const vertex_t<static_digraph> & u) { a.pred_arc(u); };
template <typename A>
concept has_succ_arc = requires(
    const A & a, const vertex_t<static_digraph> & u) { a.succ_arc(u); };
}  // namespace

static_assert(
    bidirectional_dijkstra_trait<bidirectional_dijkstra_pathless_traits>);

GTEST_TEST(bidirectional_dijkstra, store_path_false) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 7);
    builder.add_arc(0, 2, 9);
    builder.add_arc(0, 5, 14);
    builder.add_arc(1, 0, 7);
    builder.add_arc(1, 2, 10);
    builder.add_arc(1, 3, 15);
    builder.add_arc(2, 0, 9);
    builder.add_arc(2, 1, 10);
    builder.add_arc(2, 3, 12);
    builder.add_arc(2, 5, 2);
    builder.add_arc(3, 1, 15);
    builder.add_arc(3, 2, 12);
    builder.add_arc(3, 4, 6);
    builder.add_arc(4, 3, 6);
    builder.add_arc(4, 5, 9);
    builder.add_arc(5, 0, 14);
    builder.add_arc(5, 2, 2);
    builder.add_arc(5, 4, 9);

    auto [graph, length_map] = builder.build();

    bidirectional_dijkstra alg(bidirectional_dijkstra_pathless_traits{}, graph,
                               length_map, 0u, 3u);
    using pathless = decltype(alg);
    using with_path = decltype(bidirectional_dijkstra(graph, length_map));

    // the distance is still computed, and matches the store_path = true run
    ASSERT_EQ(alg.run(), 21);

    // ... while every path-only member is withdrawn
    static_assert(!has_path_found<pathless>);
    static_assert(!has_path<pathless>);
    static_assert(!has_pred_arc<pathless>);
    static_assert(!has_succ_arc<pathless>);

    // control: the same four are present under the default traits, so the
    // assertions above cannot pass vacuously through a typo in a member name
    static_assert(has_path_found<with_path>);
    static_assert(has_path<with_path>);
    static_assert(has_pred_arc<with_path>);
    static_assert(has_succ_arc<with_path>);

    // the predecessor maps are really gone, not merely inaccessible
    static_assert(sizeof(pathless) < sizeof(with_path));

    alg.reset();
    ASSERT_EQ(alg.add_source(0u).add_target(3u).run(), 21);
}
