#include <gtest/gtest.h>

#include "melon/static_digraph.hpp"
#include "melon/static_map.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(StaticMap, empty_constructor) {
    StaticMap<int> map;
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());
}

GTEST_TEST(StaticMap, size_constructor) {
    StaticMap<int> map(0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    StaticMap<int> map2(5);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());
}

GTEST_TEST(StaticMap, size_init_constructor) {
    StaticMap<int> map(0, 0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    StaticMap<int> map2(5, 113);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());

    auto test = [](const int a) { return a == 113; };
    ASSERT_TRUE(std::all_of(map2.begin(), map2.end(), test));
    ASSERT_TRUE(std::all_of(std::as_const(map2).begin(), std::as_const(map2).end(), test));
}

GTEST_TEST(StaticMap, range_constructor) {
    StaticMap<int> map(0, 0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    StaticMap<int> map2(datas);
    ASSERT_EQ(map2.size(), datas.size());
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());

    ASSERT_TRUE(std::ranges::equal(map2, datas));
}

GTEST_TEST(StaticMap, accessor) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    StaticMap<int> map(datas);
    for(std::size_t i = 0; i < map.size(); ++i) {
        ASSERT_EQ(map[i], datas[i]);
    }
}

GTEST_TEST(StaticMap, resize) {
    StaticMap<int> map(20);    
    map.resize(10);
    ASSERT_EQ(map.size(), 10);
}
