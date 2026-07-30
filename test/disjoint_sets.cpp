#undef NDEBUG
#include <gtest/gtest.h>

#include <utility>

#include "melon/container/disjoint_sets.hpp"
#include "melon/container/static_map.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// pushed keys live in distinct sets until merge_keys unites them
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// a single-argument constructor call cannot hijack the copy constructor
////////////////////////////////////////////////////////////////////////////////

// regression: the unconstrained `disjoint_sets(CM &&)` beat the copy
// constructor for a non-const lvalue and tried to build the component map out
// of the whole disjoint_sets object.
GTEST_TEST(disjoint_sets, copying_a_mutable_lvalue_uses_the_copy_constructor) {
    disjoint_sets<int> sets;
    sets.push(11);
    sets.push(20);
    sets.push(3);
    sets.merge_keys(11, 20);

    disjoint_sets<int> from_mutable_lvalue(sets);
    ASSERT_EQ(from_mutable_lvalue.find(11), from_mutable_lvalue.find(20));
    ASSERT_NE(from_mutable_lvalue.find(11), from_mutable_lvalue.find(3));

    from_mutable_lvalue.merge_keys(11, 3);
    ASSERT_EQ(from_mutable_lvalue.find(11), from_mutable_lvalue.find(3));
    ASSERT_NE(sets.find(11), sets.find(3));  // an independent copy

    disjoint_sets<int> from_const_lvalue(std::as_const(sets));
    ASSERT_EQ(from_const_lvalue.find(11), from_const_lvalue.find(20));

    // the component-map constructor is still reachable
    disjoint_sets<unsigned int, static_map<unsigned int>> from_map(
        static_map<unsigned int>(4u, 0u));
    ASSERT_EQ(from_map.size(), 0u);
}
