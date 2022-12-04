#include <gtest/gtest.h>

#include "melon/concepts/map_of.hpp"
#include "melon/data_structures/static_map.hpp"
#include "melon/static_digraph.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

static_assert(std::copyable<static_map<std::size_t, int>>);
static_assert(std::ranges::random_access_range<static_map<std::size_t, int>>);
static_assert(fhamonic::melon::concepts::output_map_of<
              static_map<std::size_t, int>, std::size_t, int>);

GTEST_TEST(static_map, empty_constructor) {
    static_map<std::size_t, int> map;
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());
}

GTEST_TEST(static_map, size_constructor) {
    static_map<std::size_t, int> map(0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    static_map<std::size_t, int> map2(5);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());
}

GTEST_TEST(static_map, size_init_constructor) {
    static_map<std::size_t, int> map(0, 0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    static_map<std::size_t, int> map2(5, 113);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());

    int value = 113;
    ASSERT_TRUE(EQ_RANGES(
        map2, std::vector<std::pair<const std::size_t, int &>>({{0u, value},
                                                                {1u, value},
                                                                {2u, value},
                                                                {3u, value},
                                                                {4u, value}})));
    ASSERT_TRUE(EQ_RANGES(
        std::as_const(map2),
        std::vector<std::pair<const std::size_t, int>>(
            {{0u, 113}, {1u, 113}, {2u, 113}, {3u, 113}, {4u, 113}})));
}

GTEST_TEST(static_map, range_constructor) {
    static_map<std::size_t, int> map(0, 0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    static_map<std::size_t, int> map2(datas.begin(), datas.end());
    ASSERT_EQ(map2.size(), datas.size());
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());

    ASSERT_TRUE(std::ranges::equal(std::views::values(map2), datas));
    ASSERT_TRUE(EQ_RANGES(
        map2,
        std::vector<std::pair<const std::size_t, int &>>({{0u, datas[0]},
                                                          {1u, datas[1]},
                                                          {2u, datas[2]},
                                                          {3u, datas[3]},
                                                          {4u, datas[4]},
                                                          {5u, datas[5]}})));
    ASSERT_TRUE(EQ_RANGES(
        std::as_const(map2),
        std::vector<std::pair<const std::size_t, int>>(
            {{0u, 0}, {1u, 7}, {2u, 3}, {3u, 5}, {4u, 6}, {5u, 11}})));
}

GTEST_TEST(static_map, read_operator) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    const static_map<std::size_t, int> map(datas.begin(), datas.end());
    for(std::size_t i = 0; i < map.size(); ++i) {
        ASSERT_EQ(map[i], datas[i]);
    }
}
GTEST_TEST(static_map, write_operator) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    static_map<std::size_t, int> map(datas.begin(), datas.end());
    for(std::size_t i = 0; i < map.size(); ++i) {
        ASSERT_EQ(map[i], datas[i]);
        map[i] = 3 * static_cast<int>(i) + 1;
        ASSERT_EQ(map[i], 3 * i + 1);
    }
}
GTEST_TEST(static_map, for_each_read) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    const static_map<std::size_t, int> map(datas.begin(), datas.end());
    std::size_t cpt = 0;
    for(auto && [k, v] : map) {
        ASSERT_EQ(k, cpt);
        ASSERT_EQ(v, datas[cpt]);
        ++cpt;
    }
}
GTEST_TEST(static_map, for_each_write) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    static_map<std::size_t, int> map(datas.begin(), datas.end());
    std::size_t cpt = 0;
    for(auto && [k, v] : map) {
        ASSERT_EQ(k, cpt);
        ASSERT_EQ(v, datas[cpt]);
        v = 3 * static_cast<int>(cpt) + 1;
        ASSERT_EQ(map[cpt], 3 * static_cast<int>(cpt) + 1);
        ++cpt;
    }
}
GTEST_TEST(static_map, resize) {
    static_map<std::size_t, int> map(20);
    map.resize(10);
    ASSERT_EQ(map.size(), 10);
}
