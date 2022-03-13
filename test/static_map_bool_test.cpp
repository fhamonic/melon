#include <gtest/gtest.h>

#include <random>

#include "melon/static_digraph.hpp"
#include "melon/data_structures/static_map.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(static_map_bool, empty_constructor) {
    static_map<bool> map;
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());
}

GTEST_TEST(static_map_bool, size_constructor) {
    static_map<bool> map(0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    static_map<bool> map2(5);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());
}

GTEST_TEST(static_map_bool, size_init_constructor) {
    static_map<bool> map(0, false);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    static_map<bool> map2(5, true);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());

    auto test = [](const bool a) { return a; };
    ASSERT_TRUE(std::all_of(map2.begin(), map2.end(), test));
    ASSERT_TRUE(std::all_of(std::as_const(map2).begin(), std::as_const(map2).end(), test));

    static_map<bool> map3(5, false);
    ASSERT_EQ(map3.size(), 5);
    ASSERT_NE(map3.begin(), map.end());
    ASSERT_NE(std::as_const(map3).begin(), std::as_const(map).end());

    auto test2 = [](const bool a) { return !a; };
    ASSERT_TRUE(std::all_of(map3.begin(), map3.end(), test2));
    ASSERT_TRUE(std::all_of(std::as_const(map3).begin(), std::as_const(map3).end(), test2));
}

GTEST_TEST(static_map_bool, accessor_uniform_read) {
    static_map<bool> map(5, false);
    for(std::size_t i = 0; i < map.size(); ++i) {
        ASSERT_FALSE(map[i]);
    }

    static_map<bool> map2(5, true);
    for(std::size_t i = 0; i < map2.size(); ++i) {
        ASSERT_TRUE(map2[i]);
    }
}

GTEST_TEST(static_map_bool, accessor_write_and_read) {
    static_map<bool> map(5, false);
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
    const std::size_t nb_bools = 153;
    std::vector<bool> datas(nb_bools);
    static_map<bool> map(nb_bools, false);

    auto gen = std::bind(std::uniform_int_distribution<>(0, 1),
                         std::default_random_engine());

    for(std::size_t i = 0; i < nb_bools; ++i) {
        bool b = gen();
        datas[i] = b;
        map[i] = b;
    }

    for(std::size_t i = 0; i < nb_bools; ++i) {
        ASSERT_EQ(datas[i], map[i]);
    }
}

GTEST_TEST(static_map_bool, iterator_extensive_read) {
    const std::size_t nb_bools = 153;
    std::vector<bool> datas(nb_bools);
    static_map<bool> map(nb_bools);

    auto gen = std::bind(std::uniform_int_distribution<>(0, 1),
                         std::default_random_engine());

    for(std::size_t i = 0; i < nb_bools; ++i) {
        bool b = gen();
        datas[i] = b;
        map[i] = b;
    }

    static_assert(std::random_access_iterator<std::vector<bool>::iterator>);
    static_assert(std::random_access_iterator<static_map<bool>::iterator>);

    ASSERT_TRUE(std::ranges::equal(map, datas));
}