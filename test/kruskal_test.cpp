#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/kruskal.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/undirect.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(kruskal, test) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder
        .add_arc(0, 1, 7)   // 0
        .add_arc(0, 2, 9)   // 1
        .add_arc(0, 5, 14)  // 2
        .add_arc(1, 2, 10)  // 3
        .add_arc(1, 3, 15)  // 4
        .add_arc(2, 3, 12)  // 5
        .add_arc(2, 5, 2)   // 6
        .add_arc(3, 4, 6)   // 7
        .add_arc(4, 5, 11);  // 8

    auto [graph, cost_map] = builder.build();
    auto ugraph = views::undirect(graph);

    kruskal alg(ugraph, cost_map);

    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 6);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 7);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 0);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 1);
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), 8);
    alg.advance();
    ASSERT_TRUE(alg.finished());
    alg.reset();
}

// GTEST_TEST(kruskal, algorithm_iterator) {
//     static_digraph_builder<static_digraph, int> builder(6);

//     builder.add_arc(0, 1, 7)
//         .add_arc(0, 2, 9)
//         .add_arc(0, 5, 14)
//         .add_arc(1, 0, 7)
//         .add_arc(1, 2, 10)
//         .add_arc(1, 3, 15)
//         .add_arc(2, 0, 9)
//         .add_arc(2, 1, 10)
//         .add_arc(2, 3, 12)
//         .add_arc(2, 5, 2)
//         .add_arc(3, 1, 15)
//         .add_arc(3, 2, 12)
//         .add_arc(3, 4, 6)
//         .add_arc(4, 3, 6)
//         .add_arc(4, 5, 9)
//         .add_arc(5, 0, 14)
//         .add_arc(5, 2, 2)
//         .add_arc(5, 4, 9);

//     auto [graph, length_map] = builder.build();

//     kruskal alg(graph, length_map);
//     alg.add_source(0u);

//     static_assert(std::ranges::input_range<decltype(alg)>);
//     static_assert(std::ranges::viewable_range<decltype(alg)>);

//     std::vector traversal = {std::make_pair(0u, 0),  std::make_pair(1u, 7),
//                              std::make_pair(2u, 9),  std::make_pair(5u, 11),
//                              std::make_pair(4u, 20), std::make_pair(3u, 21)};

//     std::size_t cpt = 0;
//     for(const auto v : alg) {
//         ASSERT_EQ(v, traversal[cpt]);
//         ++cpt;
//     }
//     ASSERT_EQ(cpt, traversal.size());
// }

// struct kruskal_traits {
//     using semiring = shortest_path_semiring<int>;
//     struct entry_cmp {
//         [[nodiscard]] constexpr bool operator()(
//             const auto & e1, const auto & e2) const noexcept {
//             return semiring::less(e1.second, e2.second);
//         }
//     };
//     using heap = d_ary_heap<2, vertex_t<static_digraph>, int, entry_cmp,
//                             vertex_map_t<static_digraph, std::size_t>>;

//     static constexpr bool store_distances = false;
//     static constexpr bool store_paths = true;
// };

// GTEST_TEST(kruskal, path_to) {
//     static_digraph_builder<static_digraph, int, int> builder(6);

//     builder.add_arc(0, 1, 7, 1)
//         .add_arc(0, 2, 9, 2)
//         .add_arc(0, 5, 14, 3)
//         .add_arc(1, 0, 7, 3)
//         .add_arc(1, 2, 10, 4)
//         .add_arc(1, 3, 15, 5)
//         .add_arc(2, 0, 9, 6)
//         .add_arc(2, 1, 10, 7)
//         .add_arc(2, 3, 12, 8)
//         .add_arc(2, 5, 2, 9)
//         .add_arc(3, 1, 15, 10)
//         .add_arc(3, 2, 12, 11)
//         .add_arc(3, 4, 6, 12)
//         .add_arc(4, 3, 6, 13)
//         .add_arc(4, 5, 9, 14)
//         .add_arc(5, 0, 14, 15)
//         .add_arc(5, 2, 2, 16)
//         .add_arc(5, 4, 9, 17);

//     auto [graph, length_map, id] = builder.build();

//     auto alg = kruskal(kruskal_traits{}, graph, length_map);
//     alg.add_source(0u).run();

//     auto path = alg.path_to(3u);
//     static_assert(std::ranges::input_range<decltype(path)>);
//     static_assert(std::ranges::viewable_range<decltype(path)>);

//     ASSERT_TRUE(EQ_MULTISETS(
//         std::views::transform(path, [&id](const auto & a) { return id[a]; }),
//         {2, 8}));
// }
