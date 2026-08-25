#undef NDEBUG
#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

// melon/experimental/ carries no API stability guarantee, but the headers
// that ship must at least keep compiling. This TU is the self-containment
// check for them; the two unfinished headers are deliberately absent (see
// their file-level comments).
#include "melon/experimental/dual.hpp"
#include "melon/experimental/planar_map.hpp"

#include "melon/container/static_digraph.hpp"
#include "melon/container/static_map.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// the planar concepts reject a graph that models none of the planar CPOs
////////////////////////////////////////////////////////////////////////////////

static_assert(!experimental::has_vertex_coordinates<static_digraph>);
static_assert(!experimental::has_arc_twin<static_digraph>);
static_assert(!experimental::has_arc_face<static_digraph>);
static_assert(!experimental::planar_subdivision<static_digraph>);
static_assert(!experimental::planar_map<static_digraph>);

////////////////////////////////////////////////////////////////////////////////
// the create_face_map ADL probe must not find the CPO itself: with the public
// name a function template, a type living in melon::experimental hard-errors
// with "satisfaction of atomic constraint depends on itself". A variable
// template is invisible to ADL, like create_vertex_map in graph.hpp
////////////////////////////////////////////////////////////////////////////////

namespace melon::experimental {
struct adl_probe_dummy {};
}  // namespace melon::experimental

static_assert(!experimental::cpo::has_adl_create_face_map<
              experimental::adl_probe_dummy, int>);

////////////////////////////////////////////////////////////////////////////////
// a triangle drawn in the plane models planar_map: 3 vertices, 3 twin pairs
// of half-arcs (twin of a is a^1), the inner face 0 and the outer face 1
// (face of a is a&1)
////////////////////////////////////////////////////////////////////////////////

namespace planar_test {

struct coord {
    double x, y;
    friend bool operator==(const coord &, const coord &) = default;
};

struct triangle {
    using arc = unsigned int;
    std::array<unsigned int, 3> _vs{0, 1, 2};
    std::array<arc, 6> _as{0, 1, 2, 3, 4, 5};
    std::array<unsigned int, 2> _fs{0, 1};

    auto vertices() const { return _vs; }
    auto arcs() const { return _as; }
    auto arcs_entries() const {
        return std::vector<std::pair<arc, std::pair<unsigned, unsigned>>>{
            {0, {0, 1}}, {1, {1, 0}}, {2, {1, 2}},
            {3, {2, 1}}, {4, {2, 0}}, {5, {0, 2}}};
    }
    unsigned arc_source(arc a) const {
        return std::array<unsigned, 6>{0, 1, 1, 2, 2, 0}[a];
    }
    unsigned arc_target(arc a) const {
        return std::array<unsigned, 6>{1, 0, 2, 1, 0, 2}[a];
    }
    // unary member: the protocol the vertex_coordinates concept probes
    coord vertex_coordinates(unsigned v) const {
        return {double(v), double(v * v)};
    }
    auto faces() const { return _fs; }
    auto bounding_arcs(unsigned f) const {
        return f == 0 ? std::vector<arc>{0, 2, 4} : std::vector<arc>{1, 5, 3};
    }
    arc arc_twin(arc a) const noexcept { return a ^ 1u; }
    unsigned arc_face(arc a) const noexcept { return a & 1u; }

    template <typename V>
    auto create_vertex_map() const {
        return static_map<unsigned, V>(3);
    }
    template <typename V>
    auto create_vertex_map(const V & d) const {
        return static_map<unsigned, V>(3, d);
    }
    template <typename V>
    auto create_arc_map() const {
        return static_map<arc, V>(6);
    }
    template <typename V>
    auto create_arc_map(const V & d) const {
        return static_map<arc, V>(6, d);
    }
    // arithmetic-only on purpose: pins that dual's create_vertex_map gates
    // on has_face_map<P, T> and not on the default std::size_t ValueType
    template <typename V>
        requires std::is_arithmetic_v<V>
    auto create_face_map() const {
        return static_map<unsigned, V>(2);
    }
    template <typename V>
        requires std::is_arithmetic_v<V>
    auto create_face_map(const V & d) const {
        return static_map<unsigned, V>(2, d);
    }
};

// ADL protocol for arc endpoint coordinates: returns a sentinel value the
// vertex_coordinates fallback cannot produce, so dispatch is observable
coord arc_source_coordinates(const triangle &, triangle::arc) {
    return {-1.0, -1.0};
}

}  // namespace planar_test

using planar_test::triangle;

static_assert(experimental::has_vertex_coordinates<triangle>);
static_assert(std::same_as<experimental::vertex_coordinates_t<triangle>,
                           planar_test::coord>);
static_assert(experimental::has_arc_twin<triangle>);
static_assert(experimental::has_arc_face<triangle>);
static_assert(experimental::drawable_graph<triangle>);
static_assert(experimental::planar_subdivision<triangle>);
static_assert(experimental::planar_map<triangle>);
static_assert(experimental::has_face_map<triangle, double>);
static_assert(!experimental::has_face_map<triangle, std::string>);

GTEST_TEST(experimental_planar_map, cpos_dispatch_on_the_triangle) {
    triangle g;
    ASSERT_EQ(experimental::vertex_coordinates(g, 2u),
              (planar_test::coord{2.0, 4.0}));
    // ADL protocol wins over the vertex_coordinates fallback
    ASSERT_EQ(experimental::arc_source_coordinates(g, 0u),
              (planar_test::coord{-1.0, -1.0}));
    // no arc_target protocol: falls back to the target's coordinates
    ASSERT_EQ(experimental::arc_target_coordinates(g, 0u),
              (planar_test::coord{1.0, 1.0}));
    ASSERT_EQ(experimental::num_faces(g), 2u);
    ASSERT_TRUE(std::ranges::equal(experimental::faces(g),
                                   std::array<unsigned, 2>{0, 1}));
    ASSERT_TRUE(std::ranges::equal(experimental::bounding_arcs(g, 0u),
                                   std::array<unsigned, 3>{0, 2, 4}));
    ASSERT_EQ(experimental::arc_twin(g, 4u), 5u);
    ASSERT_EQ(experimental::arc_face(g, 4u), 0u);
    auto face_map = experimental::create_face_map<double>(g, 0.5);
    ASSERT_EQ(face_map[1u], 0.5);
}

////////////////////////////////////////////////////////////////////////////////
// views::dual swaps vertices and faces and twins the arc incidences
////////////////////////////////////////////////////////////////////////////////

using dual_t = experimental::views::dual<triangle>;

// forwarding members stay honest about noexcept: the wrapped calls build
// vectors and may throw, except arc_twin/arc_face which the triangle
// declares noexcept
static_assert(!noexcept(std::declval<const dual_t &>().vertices()));
static_assert(!noexcept(std::declval<const dual_t &>().in_arcs(0u)));
static_assert(!noexcept(std::declval<const dual_t &>().num_vertices()));
static_assert(
    !noexcept(std::declval<const dual_t &>().create_arc_map<double>()));
static_assert(noexcept(std::declval<const dual_t &>().arc_twin(0u)));
static_assert(noexcept(std::declval<const dual_t &>().arc_target(0u)));

// dual vertex maps are face maps of the wrapped graph: the arithmetic-only
// restriction on triangle::create_face_map must carry over per ValueType
static_assert(has_vertex_map<dual_t, double>);
static_assert(!has_vertex_map<dual_t, std::string>);
static_assert(has_arc_map<dual_t, std::string>);

GTEST_TEST(experimental_dual, swaps_vertices_and_faces) {
    triangle g;
    experimental::views::dual d(g);
    ASSERT_EQ(d.num_vertices(), 2u);
    ASSERT_EQ(d.num_arcs(), 6u);
    ASSERT_EQ(d.num_faces(), 3u);
    ASSERT_TRUE(
        std::ranges::equal(d.vertices(), std::array<unsigned, 2>{0, 1}));
    ASSERT_TRUE(
        std::ranges::equal(d.faces(), std::array<unsigned, 3>{0, 1, 2}));
}

GTEST_TEST(experimental_dual, out_arcs_are_twins_of_bounding_arcs) {
    triangle g;
    experimental::views::dual d(g);
    ASSERT_TRUE(
        std::ranges::equal(d.in_arcs(0u), std::array<unsigned, 3>{0, 2, 4}));
    ASSERT_TRUE(
        std::ranges::equal(d.out_arcs(0u), std::array<unsigned, 3>{1, 3, 5}));
    for(auto && a : d.arcs()) {
        ASSERT_EQ(d.arc_target(a), g.arc_face(a));
        ASSERT_EQ(d.arc_source(a), g.arc_face(g.arc_twin(a)));
        ASSERT_EQ(d.arc_twin(a), g.arc_twin(a));
        ASSERT_EQ(d.arc_face(a), g.arc_source(a));
    }
}

GTEST_TEST(experimental_dual, create_maps_swap_roles) {
    triangle g;
    experimental::views::dual d(g);
    auto vertex_map = d.create_vertex_map<double>(0.25);  // face map of g
    ASSERT_EQ(vertex_map[1u], 0.25);
    auto arc_map = d.create_arc_map<int>(7);
    ASSERT_EQ(arc_map[5u], 7);
    auto face_map = d.create_face_map<int>(3);  // vertex map of g
    ASSERT_EQ(face_map[2u], 3);
}
