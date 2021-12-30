#include <gtest/gtest.h>
#include <iostream>


int main(int argc, char ** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

GTEST_TEST(IdentifyStrong2, test) {
    // EXPECT_EQ(strong_nodes[2], e);
}
