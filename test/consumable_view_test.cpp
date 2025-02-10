#undef NDEBUG
#include <gtest/gtest.h>

#include "range/v3/view/iota.hpp"

#include "melon/detail/consumable_view.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(consumable_view, test_std_iota_view) {
    auto v = std::views::iota(0, 9);
    auto r = consumable_view(v);
}

GTEST_TEST(consumable_view, test_ranges_v3_iota_view) {
    auto v = ranges::views::iota(0, 9);
    auto r = consumable_view(v);
}

GTEST_TEST(consumable_view, test_vector) {

    std::vector<int> v = {7,5,4,3,2,6,8,1,5};

    auto r = consumable_view(v);

}
