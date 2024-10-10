#undef NDEBUG
#include <gtest/gtest.h>

#include <numeric>
#include <queue>

#include <range/v3/algorithm/sort.hpp>
#include <range/v3/view/zip.hpp>

#include "melon/container/d_ary_heap.hpp"
#include "melon/utility/priority_queue.hpp"

#include "random_ranges_helper.hpp"
#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(d_ary_heap, 2_heap_push_pop_test) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    d_ary_heap<2, int> heap;

    static_assert(priority_queue<decltype(heap)>);

    for(auto && e : datas) {
        heap.push(e);
    }

    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 11);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 7);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 6);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 5);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 3);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 0);
    heap.pop();
    ASSERT_TRUE(heap.empty());
}

// GTEST_TEST(d_ary_heap, 2_heap_prio_map_push_pop_test) {
//     std::vector<int> datas = {0, 7, 3, 5, 6, 11};
//     d_ary_heap<2, std::pair<bool, int>, views::get_map<1>> heap;
//     for(auto && e : datas) {
//         heap.push(std::make_pair(true, e));
//     }

//     ASSERT_FALSE(heap.empty());
//     ASSERT_EQ(heap.top(), std::make_pair(true, 11));
//     heap.pop();
//     ASSERT_FALSE(heap.empty());
//     ASSERT_EQ(heap.top(), std::make_pair(true, 7));
//     heap.pop();
//     ASSERT_FALSE(heap.empty());
//     ASSERT_EQ(heap.top(), std::make_pair(true, 6));
//     heap.pop();
//     ASSERT_FALSE(heap.empty());
//     ASSERT_EQ(heap.top(), std::make_pair(true, 5));
//     heap.pop();
//     ASSERT_FALSE(heap.empty());
//     ASSERT_EQ(heap.top(), std::make_pair(true, 3));
//     heap.pop();
//     ASSERT_FALSE(heap.empty());
//     ASSERT_EQ(heap.top(), std::make_pair(true, 0));
//     heap.pop();
//     ASSERT_TRUE(heap.empty());
// }

GTEST_TEST(d_ary_heap, 2_heap_fuzzy_push_pop_test) {
    for(int it = 0; it < 10; ++it) {
        std::size_t size = 127;
        std::vector<int> datas = random_vector_all_diff(size, 0, 1000);
        std::vector<std::size_t> permuted_id(size);
        std::iota(permuted_id.begin(), permuted_id.end(), 0);
        auto zip_view = ranges::views::zip(datas, permuted_id);

        d_ary_heap<2, std::pair<std::size_t, int>, std::greater<int>,
                   views::get_map<1>>
            heap;
        for(std::size_t i = 0; i < size; ++i) {
            heap.push(std::make_pair(i, datas[i]));
        }

        ranges::sort(zip_view,
                     [](auto p1, auto p2) { return p1.first > p2.first; });
        for(std::size_t i = 0; i < size; ++i) {
            ASSERT_FALSE(heap.empty());
            ASSERT_EQ(heap.top(), std::make_pair(permuted_id[i], datas[i]));
            heap.pop();
        }
        ASSERT_TRUE(heap.empty());
    }
}
GTEST_TEST(d_ary_heap, 3_heap_fuzzy_push_pop_test) {
    for(int it = 0; it < 10; ++it) {
        std::size_t size = 127;
        std::vector<int> datas = random_vector_all_diff(size, 0, 1000);
        std::vector<std::size_t> permuted_id(size);
        std::iota(permuted_id.begin(), permuted_id.end(), 0);
        auto zip_view = ranges::views::zip(datas, permuted_id);

        d_ary_heap<3, std::pair<std::size_t, int>, std::greater<int>,
                   views::get_map<1>>
            heap;
        for(std::size_t i = 0; i < size; ++i) {
            heap.push(std::make_pair(i, datas[i]));
        }

        ranges::sort(zip_view,
                     [](auto p1, auto p2) { return p1.first > p2.first; });
        for(std::size_t i = 0; i < size; ++i) {
            ASSERT_FALSE(heap.empty());
            ASSERT_EQ(heap.top(), std::make_pair(permuted_id[i], datas[i]));
            heap.pop();
        }
        ASSERT_TRUE(heap.empty());
    }
}
GTEST_TEST(d_ary_heap, 4_heap_fuzzy_push_pop_test) {
    for(int it = 0; it < 10; ++it) {
        std::size_t size = 127;
        std::vector<int> datas = random_vector_all_diff(size, 0, 1000);
        std::vector<std::size_t> permuted_id(size);
        std::iota(permuted_id.begin(), permuted_id.end(), 0);
        auto zip_view = ranges::views::zip(datas, permuted_id);

        d_ary_heap<4, std::pair<std::size_t, int>, std::greater<int>,
                   views::get_map<1>>
            heap;
        for(std::size_t i = 0; i < size; ++i) {
            heap.push(std::make_pair(i, datas[i]));
        }

        ranges::sort(zip_view,
                     [](auto p1, auto p2) { return p1.first > p2.first; });
        for(std::size_t i = 0; i < size; ++i) {
            ASSERT_FALSE(heap.empty());
            ASSERT_EQ(heap.top(), std::make_pair(permuted_id[i], datas[i]));
            heap.pop();
        }
        ASSERT_TRUE(heap.empty());
    }
}

GTEST_TEST(updatable_d_ary_heap, 2_heap_promote_test) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    constexpr std::size_t num_elements = 6;
    updatable_d_ary_heap<2, std::pair<unsigned int, int>, std::greater<int>,
                         std::array<std::size_t, num_elements>,
                         views::get_map<1>, views::get_map<0>>
        heap;

    static_assert(updatable_priority_queue<decltype(heap)>);

    for(unsigned int i = 0; i < num_elements; ++i) {
        heap.push(std::make_pair(i, datas[i]));
    }
    heap.promote(3u, 8);

    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(5u, 11));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(3u, 8));
    heap.pop();

    heap.promote(0u, 9);

    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(0u, 9));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(1u, 7));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(4u, 6));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(2u, 3));
    heap.pop();
    ASSERT_TRUE(heap.empty());

    // heap.promote(3u, 8);

    // for(int i = 0; i < 2; ++i) {
    //     auto && [u, dist] = heap.top();
    //     std::cout << u << "  " << dist << std::endl;
    //     heap.pop();
    // }

    // heap.promote(0u, 9);

    // while(!heap.empty()) {
    //     auto && [u, dist] = heap.top();
    //     std::cout << u << "  " << dist << std::endl;
    //     heap.pop();
    // }
}

// GTEST_TEST(updatable_d_ary_heap, 2_heap_promote_external_priority_test) {
//     external_priority_map::array = {0, 7, 3, 5, 6, 11};
//     constexpr std::size_t num_elements = 6;
//     d_ary_heap<2, unsigned int, external_priority_map, std::greater<int>,
//                views::identity_map>
//         heap;
//     for(unsigned int i = 0; i < external_priority_map::array.size(); ++i) {
//         heap.push(i);
//     }
//     heap.promote(3u, 8);

//     ASSERT_FALSE(heap.empty());
//     ASSERT_EQ(heap.top(), 5u);
//     heap.pop();
//     ASSERT_FALSE(heap.empty());
//     ASSERT_EQ(heap.top(), 3u);
//     heap.pop();

//     heap.promote(0u, 9);

//     ASSERT_FALSE(heap.empty());
//     ASSERT_EQ(heap.top(), 0u);
//     heap.pop();
//     ASSERT_FALSE(heap.empty());
//     ASSERT_EQ(heap.top(), 1u);
//     heap.pop();
//     ASSERT_FALSE(heap.empty());
//     ASSERT_EQ(heap.top(), 4u);
//     heap.pop();
//     ASSERT_FALSE(heap.empty());
//     ASSERT_EQ(heap.top(), 2u);
//     heap.pop();
//     ASSERT_TRUE(heap.empty());

//     // heap.promote(3u, 8);

//     // for(int i = 0; i < 2; ++i) {
//     //     auto && [u, dist] = heap.top();
//     //     std::cout << u << "  " << dist << std::endl;
//     //     heap.pop();
//     // }

//     // heap.promote(0u, 9);

//     // while(!heap.empty()) {
//     //     auto && [u, dist] = heap.top();
//     //     std::cout << u << "  " << dist << std::endl;
//     //     heap.pop();
//     // }
// }