#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/container/disjoint_sets.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(disjoint_sets, test) {
    std::vector<int> elems = {11, 20, 3, 14};
    disjoint_sets<int> sets;

    for(auto && e : elems) {
        sets.push(e);
    }

    for(auto && e1 : elems) {
        for(auto && e2 : elems) {
            if(e1 == e2) continue;
            ASSERT_NE(sets.find(e1), sets.find(e2));
        }
    }

    sets.merge_keys(11, 20);

    for(auto && e1 : elems) {
        for(auto && e2 : elems) {
            if(e1 == e2) continue;
            if((e1 == 11 || e1 == 20) && (e2 == 11 || e2 == 20)) continue;
            ASSERT_NE(sets.find(e1), sets.find(e2));
        }
    }
    ASSERT_EQ(sets.find(11), sets.find(20));
}
