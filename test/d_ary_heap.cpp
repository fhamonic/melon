#undef NDEBUG
#include <gtest/gtest.h>

#include <numeric>
#include <queue>

#include "melon/container/d_ary_heap.hpp"
#include "melon/utility/priority_queue.hpp"

#include "random_ranges_helper.hpp"
#include "ranges_test_helper.hpp"

using namespace melon;

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
//     d_ary_heap<2, std::pair<bool, int>, views::element_map<1>> heap;
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
        auto zip_view = std::views::zip(datas, permuted_id);

        d_ary_heap<2, std::pair<std::size_t, int>, std::greater<int>,
                   views::element_map<1>>
            heap;
        for(std::size_t i = 0; i < size; ++i) {
            heap.push(std::make_pair(i, datas[i]));
        }

        std::ranges::sort(zip_view, [](auto p1, auto p2) {
            return std::get<0>(p1) > std::get<0>(p2);
        });
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
        auto zip_view = std::views::zip(datas, permuted_id);

        d_ary_heap<3, std::pair<std::size_t, int>, std::greater<int>,
                   views::element_map<1>>
            heap;
        for(std::size_t i = 0; i < size; ++i) {
            heap.push(std::make_pair(i, datas[i]));
        }

        std::ranges::sort(zip_view, [](auto p1, auto p2) {
            return std::get<0>(p1) > std::get<0>(p2);
        });
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
        auto zip_view = std::views::zip(datas, permuted_id);

        d_ary_heap<4, std::pair<std::size_t, int>, std::greater<int>,
                   views::element_map<1>>
            heap;
        for(std::size_t i = 0; i < size; ++i) {
            heap.push(std::make_pair(i, datas[i]));
        }

        std::ranges::sort(zip_view, [](auto p1, auto p2) {
            return std::get<0>(p1) > std::get<0>(p2);
        });
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
                         views::element_map<1>, views::element_map<0>>
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
// ######### regression: promote/demote need a writable priority map ###########

// promote()/demote() rewrite the priority inside an entry via
// `_entry_priority_map[e] = p`. With views::identity_map -- the default
// _EntryPriorityMap -- operator[] returns a prvalue, so the write landed on a
// temporary and was discarded: the heap silently kept the old priority and was
// never re-ordered. The operations are now constrained on a priority map that
// yields a reference into the entry.
namespace {
struct heap_item {
    int id;
    int prio;
};
struct heap_item_cmp {
    bool operator()(const heap_item & a, const heap_item & b) const {
        return a.prio < b.prio;
    }
};
struct heap_item_id_map : mapping_view_base {
    std::size_t operator[](const heap_item & i) const {
        return static_cast<std::size_t>(i.id);
    }
};

using indices_map = mapping_owning_view<std::vector<std::size_t>>;

// a priority map handing back a reference into the entry: supported
using writable_heap =
    updatable_d_ary_heap<2, std::pair<std::size_t, int>, std::greater<int>,
                         indices_map, views::element_map<1>,
                         views::element_map<0>>;
// a priority map handing back a copy of the entry: rejected
using copying_heap =
    updatable_d_ary_heap<2, heap_item, heap_item_cmp, indices_map,
                         views::identity_map, heap_item_id_map>;
}  // namespace

static_assert(mutable_entry_priority_map<views::element_map<1>,
                                         std::pair<std::size_t, int>>);
static_assert(!mutable_entry_priority_map<views::identity_map, heap_item>);

// the constrained members disappear rather than silently misbehaving
// (named concepts so the probe stays dependent and actually SFINAEs)
namespace {
template <typename H>
concept can_promote =
    requires(H h, typename H::id_type k, typename H::priority_type p) {
        h.promote(k, p);
    };
template <typename H>
concept can_demote =
    requires(H h, typename H::id_type k, typename H::priority_type p) {
        h.demote(k, p);
    };
}  // namespace

static_assert(can_promote<writable_heap>);
static_assert(can_demote<writable_heap>);
static_assert(!can_promote<copying_heap>);
static_assert(!can_demote<copying_heap>);

// ...which is what makes the whole type stop modelling updatable_priority_queue
static_assert(updatable_priority_queue<writable_heap>);
static_assert(!updatable_priority_queue<copying_heap>);

// std::greater orders the heap so that the largest priority is on top, so
// "promote" raises a priority and "demote" lowers it, matching the existing
// 2_heap_promote_test above.
GTEST_TEST(updatable_d_ary_heap, promote_actually_reorders) {
    writable_heap heap(std::greater<int>{},
                       indices_map(std::vector<std::size_t>(8)));
    heap.push({0u, 50});
    heap.push({1u, 90});
    heap.push({2u, 70});

    ASSERT_EQ(heap.top().first, 1u);
    ASSERT_EQ(heap.priority(0u), 50);

    heap.promote(0u, 100);

    // the write used to land on a temporary: priority stayed 50 and the heap
    // was never re-ordered
    ASSERT_EQ(heap.priority(0u), 100);
    ASSERT_EQ(heap.top().first, 0u);
    ASSERT_EQ(heap.top().second, 100);
}

GTEST_TEST(updatable_d_ary_heap, demote_actually_reorders) {
    writable_heap heap(std::greater<int>{},
                       indices_map(std::vector<std::size_t>(8)));
    heap.push({0u, 10});
    heap.push({1u, 20});
    heap.push({2u, 30});

    ASSERT_EQ(heap.top().first, 2u);

    heap.demote(2u, 5);

    ASSERT_EQ(heap.priority(2u), 5);
    ASSERT_EQ(heap.top().first, 1u);
    ASSERT_EQ(heap.top().second, 20);
}
