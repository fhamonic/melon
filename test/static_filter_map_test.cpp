#undef NDEBUG
#include <gtest/gtest.h>

#include <random>

#include "melon/container/static_digraph.hpp"
#include "melon/container/static_map.hpp"
#include "melon/mapping.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

static_assert(std::copyable<static_map<std::size_t, bool>>);
static_assert(std::ranges::random_access_range<static_map<std::size_t, bool>>);
static_assert(
    output_mapping_of<static_map<std::size_t, bool>, std::size_t, bool>);

GTEST_TEST(static_map_bool, empty_constructor) {
    static_map<std::size_t, bool> map;
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());
}

GTEST_TEST(static_map_bool, size_constructor) {
    static_map<std::size_t, bool> map(0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    static_map<std::size_t, bool> map2(5);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());
}

GTEST_TEST(static_map_bool, size_init_constructor) {
    static_map<std::size_t, bool> map(0, false);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    static_map<std::size_t, bool> map2(5, true);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());

    // auto test = [](const bool a) { return a; };
    // ASSERT_TRUE(std::ranges::all_of(std::views::values(map2), test));
    // ASSERT_TRUE(
    //     std::ranges::all_of(std::views::values(std::as_const(map2)), test));

    static_map<std::size_t, bool> map3(5, false);
    ASSERT_EQ(map3.size(), 5);
    ASSERT_NE(map3.begin(), map.end());
    ASSERT_NE(std::as_const(map3).begin(), std::as_const(map).end());

    // auto test2 = [](const bool a) { return !a; };
    // ASSERT_TRUE(std::ranges::all_of(std::views::values(map3), test2));
    // ASSERT_TRUE(
    //     std::ranges::all_of(std::views::values(std::as_const(map3)), test2));
}

GTEST_TEST(static_map_bool, accessor_uniform_read) {
    static_map<std::size_t, bool> map(5, false);
    for(std::size_t i = 0; i < map.size(); ++i) {
        ASSERT_FALSE(map[i]);
    }

    static_map<std::size_t, bool> map2(5, true);
    for(std::size_t i = 0; i < map2.size(); ++i) {
        ASSERT_TRUE(map2[i]);
    }
}

GTEST_TEST(static_map_bool, accessor_write_and_read) {
    static_map<std::size_t, bool> map(5, false);
    map[0] = true;
    map[2] = true;
    map[3] = true;

    ASSERT_TRUE(map[0]);
    ASSERT_FALSE(map[1]);
    ASSERT_TRUE(map[2]);
    ASSERT_TRUE(map[3]);
    ASSERT_FALSE(map[4]);

    map[0] = false;
    map[1] = false;
    map[2] = true;

    ASSERT_FALSE(map[0]);
    ASSERT_FALSE(map[1]);
    ASSERT_TRUE(map[2]);
    ASSERT_TRUE(map[3]);
    ASSERT_FALSE(map[4]);
}

GTEST_TEST(static_map_bool, accessor_extensive_write_and_read) {
    const std::size_t num_bools = 153;
    std::vector<bool> datas(num_bools);
    static_map<std::size_t, bool> map(num_bools, false);

    auto gen = std::bind(std::uniform_int_distribution<>(0, 1),
                         std::default_random_engine());

    for(std::size_t i = 0; i < num_bools; ++i) {
        bool b = gen();
        datas[i] = b;
        map[i] = b;
    }

    for(std::size_t i = 0; i < num_bools; ++i) {
        ASSERT_EQ(datas[i], map[i]);
    }
}

GTEST_TEST(static_map_bool, iterator_extensive_read) {
    const std::size_t num_bools = 153;
    std::vector<bool> datas(num_bools);
    static_map<std::size_t, bool> map(num_bools);

    auto gen = std::bind(std::uniform_int_distribution<>(0, 1),
                         std::default_random_engine());

    for(std::size_t i = 0; i < num_bools; ++i) {
        bool b = gen();
        datas[i] = b;
        map[i] = b;
    }

    static_assert(std::random_access_iterator<std::vector<bool>::iterator>);
    static_assert(
        std::random_access_iterator<static_map<std::size_t, bool>::iterator>);

    // ASSERT_TRUE(std::ranges::equal(std::views::values(map), datas));
}

GTEST_TEST(static_map_bool, filter) {
    const std::size_t num_bools = 153;
    static_filter_map<std::size_t> map(num_bools, false);
    std::vector<std::size_t> indices;

    auto gen = std::bind(std::uniform_int_distribution<>(0, 1),
                         std::default_random_engine());

    for(std::size_t i = 0; i < num_bools; ++i) {
        bool b = gen();
        if(!b) continue;
        indices.emplace_back(i);
        map[i] = b;
    }

    static_assert(std::random_access_iterator<std::vector<bool>::iterator>);
    static_assert(
        std::random_access_iterator<static_map<std::size_t, bool>::iterator>);

    ASSERT_TRUE(EQ_MULTISETS(
        map.filter(std::views::iota(std::size_t{0}, num_bools)), indices));

    ASSERT_TRUE(EQ_MULTISETS(
        map.filter(std::views::iota(int{0}, int{num_bools})), indices));
}

GTEST_TEST(static_map_bool, filter_bench) {
    std::cout << "density,map_filter,manual_filter\n";

    auto gen = std::bind(std::uniform_real_distribution<>(0, 1),
                         std::default_random_engine());

    for(double hole_density : std::initializer_list<double>{
            1.0, 0.99, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1, 0.01, 0}) {
        std::cout << hole_density << ',';
        const std::size_t num_bools = 13451;
        static_filter_map<std::size_t> map(num_bools, false);
        std::vector<std::size_t> indices;

        std::size_t hole_size = 3;

        for(std::size_t i = 0; i < num_bools - hole_size; ++i) {
            if(gen() > hole_density) continue;
            for(std::size_t j = 0; j < hole_size; ++j) map[i + j] = true;
        }
        for(std::size_t i = 0; i < num_bools; ++i) {
            if(map[i]) indices.emplace_back(i);
        }

        static_assert(std::random_access_iterator<std::vector<bool>::iterator>);
        static_assert(std::random_access_iterator<
                      static_map<std::size_t, bool>::iterator>);

        {
            auto start = std::chrono::high_resolution_clock::now();

            std::vector<std::size_t> ids;
            for(auto && i :
                map.filter(std::views::iota(std::size_t{0}, num_bools))) {
                ids.emplace_back(i);
            }
            ASSERT_TRUE(EQ_MULTISETS(ids, indices));

            auto stop = std::chrono::high_resolution_clock::now();
            auto duration =
                duration_cast<std::chrono::microseconds>(stop - start);

            std::cout << duration.count() << ',';
        }

        {
            auto start = std::chrono::high_resolution_clock::now();

            std::vector<std::size_t> ids;
            for(std::size_t i = 0; i < num_bools; ++i) {
                if(!map[i]) continue;
                ids.emplace_back(i);
            }
            ASSERT_TRUE(EQ_MULTISETS(ids, indices));

            auto stop = std::chrono::high_resolution_clock::now();
            auto duration =
                duration_cast<std::chrono::microseconds>(stop - start);

            std::cout << duration.count() << std::endl;
        }
    }
}