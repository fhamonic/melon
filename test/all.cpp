#undef NDEBUG
#include <gtest/gtest.h>

// Guards the whole public surface against rot: if any public header stops
// compiling or goes missing, this translation unit breaks the build.
#include "melon/all.hpp"

static_assert(MELON_VERSION_MAJOR >= 1);
static_assert(MELON_VERSION == MELON_VERSION_MAJOR * 10000 +
                                   MELON_VERSION_MINOR * 100 +
                                   MELON_VERSION_PATCH);

GTEST_TEST(all, includes_the_full_public_api) {
    melon::static_digraph_builder<melon::static_digraph, int> builder(2);
    builder.add_arc(0u, 1u, 1);
    auto [graph, length_map] = builder.build();

    melon::dijkstra alg(graph, length_map);
    alg.add_source(0u);
    for(const auto & [v, dist] : alg) {
        ASSERT_EQ(dist, static_cast<int>(v));
    }
}
