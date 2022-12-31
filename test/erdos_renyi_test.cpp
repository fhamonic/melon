#include <gtest/gtest.h>

#include "melon/utility/erdos_renyi.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(erdos_renyi, test) {
    auto random_graph1 = erdos_renyi(100, 0.0);
    ASSERT_EQ(random_graph1.nb_vertices(), 100);
    ASSERT_EQ(random_graph1.nb_arcs(), 0);

    auto random_graph2 = erdos_renyi(100, 1.0);
    ASSERT_EQ(random_graph2.nb_vertices(), 100);
    ASSERT_EQ(random_graph2.nb_arcs(), 9900);

    auto random_graph3 = erdos_renyi(100, 0.5);
    ASSERT_EQ(random_graph3.nb_vertices(), 100);
    // the probability of the two following tests to fail is 2*10^-60,
    // a billion times more than the number of atoms on Earth...
    ASSERT_TRUE(random_graph3.nb_arcs() > 1000);
    ASSERT_TRUE(random_graph3.nb_arcs() < 8900);
}
