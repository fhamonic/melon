#include <gtest/gtest.h>

#include "melon/algorithm/dijkstra.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(dijkstra, test) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 7)
        .add_arc(0, 2, 9)
        .add_arc(0, 5, 14)
        .add_arc(1, 0, 7)
        .add_arc(1, 2, 10)
        .add_arc(1, 3, 15)
        .add_arc(2, 0, 9)
        .add_arc(2, 1, 10)
        .add_arc(2, 3, 12)
        .add_arc(2, 5, 2)
        .add_arc(3, 1, 15)
        .add_arc(3, 2, 12)
        .add_arc(3, 4, 6)
        .add_arc(4, 3, 6)
        .add_arc(4, 5, 9)
        .add_arc(5, 0, 14)
        .add_arc(5, 2, 2)
        .add_arc(5, 4, 9);

    auto [graph, length_map] = builder.build();

    dijkstra alg(graph, length_map);

    static_assert(std::copyable<decltype(alg)>);

    alg.add_source(0);
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(0u, 0));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(1u, 7));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(2u, 9));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(5u, 11));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(4u, 20));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(3u, 21));
    alg.advance();
    ASSERT_TRUE(alg.finished());
    alg.reset();
}

GTEST_TEST(dijkstra, traversal_iterator) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 7)
        .add_arc(0, 2, 9)
        .add_arc(0, 5, 14)
        .add_arc(1, 0, 7)
        .add_arc(1, 2, 10)
        .add_arc(1, 3, 15)
        .add_arc(2, 0, 9)
        .add_arc(2, 1, 10)
        .add_arc(2, 3, 12)
        .add_arc(2, 5, 2)
        .add_arc(3, 1, 15)
        .add_arc(3, 2, 12)
        .add_arc(3, 4, 6)
        .add_arc(4, 3, 6)
        .add_arc(4, 5, 9)
        .add_arc(5, 0, 14)
        .add_arc(5, 2, 2)
        .add_arc(5, 4, 9);

    auto [graph, length_map] = builder.build();

    dijkstra alg(graph, length_map);
    alg.add_source(0u);

    static_assert(std::ranges::input_range<decltype(alg)>);
    static_assert(std::ranges::viewable_range<decltype(alg)>);

    std::vector traversal = {std::make_pair(0u, 0),  std::make_pair(1u, 7),
                             std::make_pair(2u, 9),  std::make_pair(5u, 11),
                             std::make_pair(4u, 20), std::make_pair(3u, 21)};

    std::size_t cpt = 0;
    for(const auto v : alg) {
        ASSERT_EQ(v, traversal[cpt]);
        ++cpt;
    }
    ASSERT_EQ(cpt, traversal.size());
}

struct dijkstra_traits {
    using semiring = shortest_path_semiring<int>;
    struct entry_cmp {
        [[nodiscard]] constexpr bool operator()(
            const auto & e1, const auto & e2) const noexcept {
            return semiring::less(e1.second, e2.second);
        }
    };
    using heap = d_ary_heap<2, vertex_t<static_digraph>, int, entry_cmp,
                            vertex_map_t<static_digraph, std::size_t>>;

    static constexpr bool store_distances = false;
    static constexpr bool store_paths = true;
};

GTEST_TEST(dijkstra, path_to) {
    static_digraph_builder<static_digraph, int, int> builder(6);

    builder.add_arc(0, 1, 7, 1)
        .add_arc(0, 2, 9, 2)
        .add_arc(0, 5, 14, 3)
        .add_arc(1, 0, 7, 3)
        .add_arc(1, 2, 10, 4)
        .add_arc(1, 3, 15, 5)
        .add_arc(2, 0, 9, 6)
        .add_arc(2, 1, 10, 7)
        .add_arc(2, 3, 12, 8)
        .add_arc(2, 5, 2, 9)
        .add_arc(3, 1, 15, 10)
        .add_arc(3, 2, 12, 11)
        .add_arc(3, 4, 6, 12)
        .add_arc(4, 3, 6, 13)
        .add_arc(4, 5, 9, 14)
        .add_arc(5, 0, 14, 15)
        .add_arc(5, 2, 2, 16)
        .add_arc(5, 4, 9, 17);

    auto [graph, length_map, id] = builder.build();

    dijkstra<decltype(graph), decltype(length_map), dijkstra_traits> alg(
        graph, length_map);
    alg.add_source(0u).run();

    auto path = alg.path_to(3u);
    static_assert(std::ranges::input_range<decltype(path)>);
    static_assert(std::ranges::viewable_range<decltype(path)>);

    ASSERT_TRUE(EQ_MULTISETS(
        std::views::transform(path, [&id](const auto & a) { return id[a]; }),
        {2, 8}));
}
