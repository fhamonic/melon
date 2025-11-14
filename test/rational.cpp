#undef NDEBUG
#include <gtest/gtest.h>

#include <concepts>

#include "melon/utility/rational.hpp"
#include "melon/utility/bounded_value.hpp"

using namespace fhamonic::melon;

GTEST_TEST(rational, add_test) {
    auto a = bounded_value<int8_t, -10, 21>(5);
    auto b = bounded_value<int8_t, -1, 15>(15);

    auto r = rational(a,b);
    
}