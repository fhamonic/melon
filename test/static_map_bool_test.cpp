#include <gtest/gtest.h>

#include "melon/static_digraph.hpp"
#include "melon/static_map.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(StaticMap_bool, empty_constructor) {
    StaticMap<bool> map;
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(map.cbegin(), map.cend());
}

GTEST_TEST(StaticMap_bool, size_constructor) {
    StaticMap<bool> map(0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(map.cbegin(), map.cend());

    StaticMap<bool> map2(5);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(map2.cbegin(), map.cend());
}

GTEST_TEST(StaticMap_bool, size_init_constructor) {
    StaticMap<bool> map(0, false);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(map.cbegin(), map.cend());

    StaticMap<bool> map2(5, true);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(map2.cbegin(), map.cend());

    auto test = [](const bool a) { return a; };
    ASSERT_TRUE(std::all_of(map2.begin(), map2.end(), test));
    ASSERT_TRUE(std::all_of(map2.cbegin(), map2.cend(), test));

    StaticMap<bool> map3(5, false);
    ASSERT_EQ(map3.size(), 5);
    ASSERT_NE(map3.begin(), map.end());
    ASSERT_NE(map3.cbegin(), map.cend());

    auto test2 = [](const bool a) { return !a; };
    ASSERT_TRUE(std::all_of(map3.begin(), map3.end(), test2));
    ASSERT_TRUE(std::all_of(map3.cbegin(), map3.cend(), test2));
}



