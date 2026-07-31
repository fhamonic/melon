#undef NDEBUG
#include <gtest/gtest.h>

#include <optional>
#include <tuple>
#include <utility>

#include "melon/utility/geometry.hpp"

using namespace melon;
using namespace melon::numeric;

// Exact rational coordinates, as bentley_ottmann uses them: line intersections
// divide, and the predicates below must stay exact.
using coord = rational<int>;
using point = std::tuple<coord, coord>;
using segment = std::tuple<point, point>;
using line = std::tuple<coord, coord, coord>;

static point P(int x, int y) { return point(coord(x), coord(y)); }
// coord's denominator is a compile-time constant 1, so a non-integral value
// needs the general rational<int, int>.
static auto R(int n, int d) { return rational(n, d); }
static segment S(int x1, int y1, int x2, int y2) {
    return segment(P(x1, y1), P(x2, y2));
}

////////////////////////////////////////////////////////////////////////////////
// points, segments and lines are structural concepts over coordinate tuples
////////////////////////////////////////////////////////////////////////////////

static_assert(cartesian_point<point>);
static_assert(cartesian_point<std::pair<int, int>>);
static_assert(!cartesian_point<int>);
static_assert(cartesian_segment<segment>);
static_assert(!cartesian_segment<point>);
static_assert(cartesian_line<line>);
static_assert(!cartesian_line<int>);

////////////////////////////////////////////////////////////////////////////////
// point_xy_comparator orders points by x, breaking ties by y
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(cartesian, point_xy_comparator_orders_by_x_then_y) {
    const cartesian::point_xy_comparator cmp;

    ASSERT_TRUE(cmp(P(1, 5), P(2, 0)));
    ASSERT_FALSE(cmp(P(2, 0), P(1, 5)));
    // Same abscissa: the ordinate breaks the tie.
    ASSERT_TRUE(cmp(P(1, 2), P(1, 3)));
    ASSERT_FALSE(cmp(P(1, 3), P(1, 2)));
    ASSERT_FALSE(cmp(P(1, 2), P(1, 2)));

    static_assert(
        requires { typename cartesian::point_xy_comparator::is_transparent; });
}

// regression: the comparator was unconditionally noexcept over user comparison
// operators -- a throwing coordinate comparison became std::terminate.
namespace {
struct throwing_coordinate {
    int v = 0;
    bool operator==(const throwing_coordinate &) const noexcept(false) {
        return true;
    }
    bool operator<(const throwing_coordinate &) const noexcept(false) {
        return false;
    }
};
}  // namespace

GTEST_TEST(cartesian, point_xy_comparator_noexcept_follows_the_coordinates) {
    using throwing_point = std::pair<throwing_coordinate, throwing_coordinate>;
    static_assert(cartesian_point<throwing_point>);
    static_assert(!noexcept(cartesian::point_xy_comparator{}(
        std::declval<const throwing_point &>(),
        std::declval<const throwing_point &>())));
    // Positive control.
    static_assert(noexcept(cartesian::point_xy_comparator{}(
        std::declval<const std::pair<int, int> &>(),
        std::declval<const std::pair<int, int> &>())));
    SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// segment_to_line yields the supporting line a.x + b.y = c of a segment
////////////////////////////////////////////////////////////////////////////////

// The line through (x1,y1) and (x2,y2) is written a.x + b.y = c with
// a = y2-y1, b = x1-x2 and c fixed by the first endpoint.
GTEST_TEST(cartesian, segment_to_line_passes_through_both_endpoints) {
    const auto l = cartesian::segment_to_line(S(0, 0, 2, 2));

    ASSERT_EQ(std::get<0>(l), 2);
    ASSERT_EQ(std::get<1>(l), -2);
    ASSERT_EQ(std::get<2>(l), 0);

    ASSERT_TRUE(cartesian::point_on_line(P(0, 0), l));
    ASSERT_TRUE(cartesian::point_on_line(P(2, 2), l));
    ASSERT_TRUE(cartesian::point_on_line(P(-3, -3), l));
    ASSERT_FALSE(cartesian::point_on_line(P(1, 2), l));
}

GTEST_TEST(cartesian, segment_to_line_handles_axis_parallel_segments) {
    const auto horizontal = cartesian::segment_to_line(S(0, 4, 7, 4));
    ASSERT_EQ(std::get<0>(horizontal), 0);
    ASSERT_TRUE(cartesian::point_on_line(P(100, 4), horizontal));
    ASSERT_FALSE(cartesian::point_on_line(P(100, 5), horizontal));

    const auto vertical = cartesian::segment_to_line(S(3, 0, 3, 9));
    ASSERT_EQ(std::get<1>(vertical), 0);
    ASSERT_TRUE(cartesian::point_on_line(P(3, -50), vertical));
    ASSERT_FALSE(cartesian::point_on_line(P(4, -50), vertical));
}

GTEST_TEST(cartesian, line_slope) {
    ASSERT_EQ(cartesian::line_slope(cartesian::segment_to_line(S(0, 0, 2, 2))),
              1);
    ASSERT_EQ(cartesian::line_slope(cartesian::segment_to_line(S(0, 0, 4, 2))),
              R(1, 2));
    ASSERT_EQ(cartesian::line_slope(cartesian::segment_to_line(S(0, 4, 7, 4))),
              0);

    // A vertical line has no finite slope; it comes back as 1/0.
    const auto infinite =
        cartesian::line_slope(cartesian::segment_to_line(S(3, 0, 3, 9)));
    ASSERT_EQ(infinite.num(), 1);
    ASSERT_EQ(infinite.den(), 0);
}

////////////////////////////////////////////////////////////////////////////////
// lines_intersection returns the exact crossing point, or nothing for
// parallel lines
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(cartesian, lines_intersection_of_crossing_lines) {
    const auto diagonal = cartesian::segment_to_line(S(0, 0, 2, 2));
    const auto antidiagonal = cartesian::segment_to_line(S(0, 2, 2, 0));

    const auto i = cartesian::lines_intersection(diagonal, antidiagonal);
    ASSERT_TRUE(i.has_value());
    ASSERT_EQ(std::get<0>(i.value()), 1);
    ASSERT_EQ(std::get<1>(i.value()), 1);

    // The unchecked overload must agree whenever the lines do cross.
    const auto direct =
        cartesian::intersecting_lines_intersection(diagonal, antidiagonal);
    ASSERT_EQ(std::get<0>(direct), std::get<0>(i.value()));
    ASSERT_EQ(std::get<1>(direct), std::get<1>(i.value()));
}

GTEST_TEST(cartesian, lines_intersection_yields_a_non_integral_point) {
    const auto l1 = cartesian::segment_to_line(S(0, 0, 2, 1));
    const auto l2 = cartesian::segment_to_line(S(0, 1, 1, 0));

    const auto i = cartesian::lines_intersection(l1, l2);
    ASSERT_TRUE(i.has_value());
    ASSERT_EQ(std::get<0>(i.value()), R(2, 3));
    ASSERT_EQ(std::get<1>(i.value()), R(1, 3));
}

GTEST_TEST(cartesian, lines_intersection_rejects_parallel_lines) {
    const auto l1 = cartesian::segment_to_line(S(0, 0, 2, 2));
    const auto parallel = cartesian::segment_to_line(S(0, 1, 2, 3));
    const auto same = cartesian::segment_to_line(S(-1, -1, 5, 5));

    ASSERT_FALSE(cartesian::lines_intersection(l1, parallel).has_value());
    // Colinear lines have no single intersection point either.
    ASSERT_FALSE(cartesian::lines_intersection(l1, same).has_value());
}

////////////////////////////////////////////////////////////////////////////////
// integral coordinates divide exactly through rationals instead of truncating
////////////////////////////////////////////////////////////////////////////////

// regression: plain integer coordinates satisfy cartesian_point too, and `/`
// truncated their intersections -- (1, 1/3) came back as (1, 0).
GTEST_TEST(cartesian, integral_coordinates_intersect_exactly) {
    using ipoint = std::pair<int, int>;
    using isegment = std::pair<ipoint, ipoint>;
    const auto la = cartesian::segment_to_line(isegment{{0, 0}, {3, 1}});
    const auto lb = cartesian::segment_to_line(isegment{{1, 0}, {1, 1}});

    const auto i = cartesian::lines_intersection(la, lb);
    ASSERT_TRUE(i.has_value());
    ASSERT_EQ(std::get<0>(i.value()), 1);
    ASSERT_EQ(std::get<1>(i.value()), R(1, 3));
}

// regression: line_slope on an integral vertical line divided by zero; it now
// yields make_rational's 1/0 infinity sentinel, like the rational-coordinate
// path always did.
GTEST_TEST(cartesian, integral_vertical_line_slope_is_the_infinity_sentinel) {
    using ipoint = std::pair<int, int>;
    using isegment = std::pair<ipoint, ipoint>;
    const auto vertical = cartesian::segment_to_line(isegment{{3, 0}, {3, 9}});

    const auto slope = cartesian::line_slope(vertical);
    ASSERT_EQ(slope.num(), 1);
    ASSERT_EQ(slope.den(), 0);

    const auto diagonal = cartesian::segment_to_line(isegment{{0, 0}, {4, 2}});
    ASSERT_EQ(cartesian::line_slope(diagonal), R(1, 2));
}

////////////////////////////////////////////////////////////////////////////////
// point_on_segment restricts point_on_line to the segment's extent

GTEST_TEST(cartesian, point_on_segment) {
    const auto s = S(0, 0, 4, 4);

    ASSERT_TRUE(cartesian::point_on_segment(P(2, 2), s));
    ASSERT_TRUE(cartesian::point_on_segment(P(0, 0), s));
    ASSERT_TRUE(cartesian::point_on_segment(P(4, 4), s));

    // On the supporting line, but beyond the endpoints.
    ASSERT_FALSE(cartesian::point_on_segment(P(5, 5), s));
    ASSERT_FALSE(cartesian::point_on_segment(P(-1, -1), s));
    // Off the supporting line altogether.
    ASSERT_FALSE(cartesian::point_on_segment(P(1, 2), s));
}

////////////////////////////////////////////////////////////////////////////////
// segments_intersection accepts only crossings that lie inside both segments
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(cartesian, segments_intersection_of_crossing_segments) {
    const auto i =
        cartesian::segments_intersection(S(0, 0, 2, 2), S(0, 2, 2, 0));
    ASSERT_TRUE(i.has_value());
    ASSERT_EQ(std::get<0>(i.value()), 1);
    ASSERT_EQ(std::get<1>(i.value()), 1);
}

GTEST_TEST(cartesian, segments_intersection_touching_at_an_endpoint) {
    const auto i =
        cartesian::segments_intersection(S(0, 0, 2, 2), S(2, 2, 4, 0));
    ASSERT_TRUE(i.has_value());
    ASSERT_EQ(std::get<0>(i.value()), 2);
    ASSERT_EQ(std::get<1>(i.value()), 2);
}

// The supporting lines cross, but outside of at least one of the segments.
GTEST_TEST(cartesian, segments_intersection_rejects_a_crossing_out_of_range) {
    ASSERT_FALSE(cartesian::segments_intersection(S(0, 0, 1, 1), S(2, 0, 3, -1))
                     .has_value());
}

GTEST_TEST(cartesian, segments_intersection_rejects_parallel_segments) {
    ASSERT_FALSE(cartesian::segments_intersection(S(0, 0, 2, 2), S(0, 1, 2, 3))
                     .has_value());
}

////////////////////////////////////////////////////////////////////////////////
// segments_overlap returns the shared sub-segment of colinear segments
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(cartesian, segments_overlap_of_colinear_segments) {
    const auto o = cartesian::segments_overlap(S(0, 0, 4, 0), S(2, 0, 6, 0));
    ASSERT_TRUE(o.has_value());
    ASSERT_EQ(std::get<0>(o.value()), P(2, 0));
    ASSERT_EQ(std::get<1>(o.value()), P(4, 0));

    // The overlap of nested segments is the inner one.
    const auto nested =
        cartesian::segments_overlap(S(0, 0, 10, 10), S(2, 2, 4, 4));
    ASSERT_TRUE(nested.has_value());
    ASSERT_EQ(std::get<0>(nested.value()), P(2, 2));
    ASSERT_EQ(std::get<1>(nested.value()), P(4, 4));
}

GTEST_TEST(cartesian, segments_overlap_rejects_disjoint_and_non_colinear) {
    // Colinear but disjoint.
    ASSERT_FALSE(
        cartesian::segments_overlap(S(0, 0, 1, 0), S(2, 0, 3, 0)).has_value());
    // Parallel but on distinct lines.
    ASSERT_FALSE(
        cartesian::segments_overlap(S(0, 0, 4, 0), S(0, 1, 4, 1)).has_value());
    // Not even parallel.
    ASSERT_FALSE(
        cartesian::segments_overlap(S(0, 0, 4, 0), S(0, 0, 0, 4)).has_value());
}

// colinear_segments_overlap skips the parallelism test: its caller already
// knows the two segments share a direction.
GTEST_TEST(cartesian, colinear_segments_overlap) {
    const auto o =
        cartesian::colinear_segments_overlap(S(0, 0, 4, 4), S(2, 2, 6, 6));
    ASSERT_TRUE(o.has_value());
    ASSERT_EQ(std::get<0>(o.value()), P(2, 2));
    ASSERT_EQ(std::get<1>(o.value()), P(4, 4));

    ASSERT_FALSE(
        cartesian::colinear_segments_overlap(S(0, 0, 4, 4), S(0, 1, 4, 5))
            .has_value());
}

// aligned_segments_overlap is the raw range intersection, in xy order.
GTEST_TEST(cartesian, aligned_segments_overlap) {
    const auto o =
        cartesian::aligned_segments_overlap(S(0, 0, 4, 4), S(6, 6, 2, 2));
    ASSERT_TRUE(o.has_value());
    ASSERT_EQ(std::get<0>(o.value()), P(2, 2));
    ASSERT_EQ(std::get<1>(o.value()), P(4, 4));

    // Touching at a single point still counts as an overlap.
    const auto touching =
        cartesian::aligned_segments_overlap(S(0, 0, 4, 4), S(4, 4, 8, 8));
    ASSERT_TRUE(touching.has_value());
    ASSERT_EQ(std::get<0>(touching.value()), P(4, 4));
    ASSERT_EQ(std::get<1>(touching.value()), P(4, 4));

    ASSERT_FALSE(
        cartesian::aligned_segments_overlap(S(0, 0, 1, 1), S(2, 2, 3, 3))
            .has_value());
}
