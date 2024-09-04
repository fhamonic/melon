#undef NDEBUG
#include <gtest/gtest.h>

#include <range/v3/algorithm/sort.hpp>
#include <range/v3/view/zip.hpp>

#include "melon/container/d_ary_heap.hpp"

#include "random_ranges_helper.hpp"
#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(d_ary_heap, 2_heap_push_pop_test) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    d_ary_heap<2, unsigned int, int> heap;
    for(unsigned int i = 0; i < datas.size(); ++i) {
        heap.push(i, datas[i]);
    }
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(5u, 11));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(1u, 7));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(4u, 6));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(3u, 5));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(2u, 3));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(0u, 0));
    heap.pop();
    ASSERT_TRUE(heap.empty());
}

GTEST_TEST(d_ary_heap, 2_heap_fuzzy_push_pop_test) {
    for(int it = 0; it < 10; ++it) {
        std::size_t size = 127;
        std::vector<int> datas = random_vector_all_diff(size, 0, 1000);
        std::vector<std::size_t> permuted_id(size);
        std::iota(permuted_id.begin(), permuted_id.end(), 0);
        auto zip_view = ranges::views::zip(datas, permuted_id);

        d_ary_heap<2, std::size_t, int> heap;
        for(std::size_t i = 0; i < size; ++i) {
            heap.push(i, datas[i]);
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

        d_ary_heap<3, std::size_t, int> heap;
        for(std::size_t i = 0; i < size; ++i) {
            heap.push(i, datas[i]);
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

        d_ary_heap<4, std::size_t, int> heap;
        for(std::size_t i = 0; i < size; ++i) {
            heap.push(i, datas[i]);
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


GTEST_TEST(d_ary_heap, 2_heap_promote_test) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    d_ary_heap<2, unsigned int, int> heap;
    for(unsigned int i = 0; i < datas.size(); ++i) {
        heap.push(i, datas[i]);
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
}