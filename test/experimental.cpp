#undef NDEBUG
#include <gtest/gtest.h>

// melon/experimental/ carries no API stability guarantee, but the headers
// that ship must at least keep compiling. This TU is the self-containment
// check for them; the two unfinished headers are deliberately absent (see
// their file-level comments).
#include "melon/experimental/dual.hpp"
#include "melon/experimental/planar_map.hpp"

#include "melon/container/static_digraph.hpp"

using namespace melon;

// The concepts must reject a graph that models none of the planar CPOs.
// There is no concrete planar_map container yet, so this is the only
// property that can be asserted today.
static_assert(!experimental::has_vertex_coordinates<static_digraph>);
static_assert(!experimental::has_arc_twin<static_digraph>);
static_assert(!experimental::has_arc_face<static_digraph>);
static_assert(!experimental::planar_subdivision<static_digraph>);
static_assert(!experimental::planar_map<static_digraph>);

GTEST_TEST(experimental, headers_are_self_contained) { SUCCEED(); }
