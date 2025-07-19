#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/detail/consumable_view.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(consumable_view, test_std_iota_view) {
    auto v = std::views::iota(0, 9);
    auto r = consumable_view(v);
}

GTEST_TEST(consumable_view, test_vector_manual_loop) {
    std::vector<bool> filter(9, false);
    std::vector<unsigned int> v = {7u, 5u, 4u, 3u, 2u, 6u, 8u, 1u};

    auto r = consumable_view(v);
    std::vector<unsigned int> took;

    for(; !r.empty(); r.advance()) {
        auto i = r.current();
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
        break;
    }
    for(; !r.empty(); r.advance()) {
        auto i = r.current();
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
        break;
    }
    ASSERT_TRUE(EQ_RANGES(took, {7u, 5u}));

    took.resize(0);
    for(; !r.empty(); r.advance()) {
        auto i = r.current();
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
    }
    ASSERT_TRUE(EQ_RANGES(took, {4u, 3u, 2u, 6u, 8u, 1u}));
}

GTEST_TEST(consumable_view, test_vector_iterator_loop) {
    std::vector<bool> filter(9, false);
    std::vector<unsigned int> v = {7u, 5u, 4u, 3u, 2u, 6u, 8u, 1u};

    auto r = consumable_view(v);
    std::vector<unsigned int> took;

    for(auto it = std::begin(r); it != std::end(r); ++it) {
        const auto i = *it;
        // std::cout << "loop1 i : " << i << std::endl;
        // std::cout << "loop1 filter[i] : " << filter[i] << std::endl;
        // std::cout << "loop1 it == end ? : " << (it == std::end(r)) <<
        // std::endl;
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
        break;
    }
    for(auto it = std::begin(r); it != std::end(r); ++it) {
        const auto i = *it;
        // std::cout << "loop2 i : " << i << std::endl;
        // std::cout << "loop2 filter[i] : " << filter[i] << std::endl;
        // std::cout << "loop2 it == end ? : " << (it == std::end(r)) <<
        // std::endl;
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
        break;
    }
    // std::cout << std::format("{}\n", took);
    ASSERT_TRUE(EQ_RANGES(took, {7u, 5u}));

    took.resize(0);
    for(auto it = std::begin(r); it != std::end(r); ++it) {
        const auto i = *it;
        // std::cout << "loop3 i : " << i << std::endl;
        // std::cout << "loop3 filter[i] : " << filter[i] << std::endl;
        // std::cout << "loop3 it == end ? : " << (it == std::end(r)) <<
        // std::endl;
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
    }
    // std::cout << std::format("{}\n", took);
    ASSERT_TRUE(EQ_RANGES(took, {4u, 3u, 2u, 6u, 8u, 1u}));
}

GTEST_TEST(consumable_view, test_vector_for_loop) {
    std::vector<bool> filter(9, false);
    std::vector<unsigned int> v = {7u, 5u, 4u, 3u, 2u, 6u, 8u, 1u};

    auto r = consumable_view(v);
    std::vector<unsigned int> took;

    for(auto i : r) {
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
        break;
    }
    for(auto i : r) {
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
        break;
    }
    ASSERT_TRUE(EQ_RANGES(took, {7u, 5u}));

    took.resize(0);
    for(auto i : r) {
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
    }
    ASSERT_TRUE(EQ_RANGES(took, {4u, 3u, 2u, 6u, 8u, 1u}));
}