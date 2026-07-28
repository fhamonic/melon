#undef NDEBUG
#include <gtest/gtest.h>

#include <iterator>
#include <ranges>
#include <string>
#include <vector>

#include "melon/container/static_digraph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/graphviz_printer.hpp"
#include "melon/utility/static_digraph_builder.hpp"

using namespace melon;

using vertex = vertex_t<static_digraph>;
using arc = arc_t<static_digraph>;
using printer = graphviz_printer<static_digraph>;

// 0 -> 1 -> 2, plus the shortcut 0 -> 2.
static auto triangle_digraph() {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc(0u, 1u).add_arc(1u, 2u).add_arc(0u, 2u);
    auto [graph] = builder.build();
    return graph;
}

static std::string print(const printer & p) {
    std::string out;
    p.print(std::back_inserter(out));
    return out;
}

static std::size_t count(const std::string & haystack,
                         const std::string & needle) {
    std::size_t n = 0;
    for(std::size_t i = haystack.find(needle); i != std::string::npos;
        i = haystack.find(needle, i + needle.size()))
        ++n;
    return n;
}

static bool contains(const std::string & haystack, const std::string & needle) {
    return haystack.find(needle) != std::string::npos;
}

GTEST_TEST(graphviz_printer, emits_a_well_formed_digraph) {
    const auto graph = triangle_digraph();
    const auto dot = print(printer(graph));

    ASSERT_TRUE(dot.starts_with("digraph {"));
    ASSERT_TRUE(dot.ends_with("}\n"));
    ASSERT_TRUE(contains(dot, "node [style=filled shape=\"circle\"]"));
    ASSERT_TRUE(contains(dot, "edge [style=filled]"));

    // Every vertex and every arc must show up exactly once.
    ASSERT_EQ(count(dot, "0 [width="), 1u);
    ASSERT_EQ(count(dot, "1 [width="), 1u);
    ASSERT_EQ(count(dot, "2 [width="), 1u);
    ASSERT_EQ(count(dot, "0 -> 1 "), 1u);
    ASSERT_EQ(count(dot, "1 -> 2 "), 1u);
    ASSERT_EQ(count(dot, "0 -> 2 "), 1u);

    // Attribute lists are balanced.
    ASSERT_EQ(count(dot, "["), count(dot, "]"));
}

GTEST_TEST(graphviz_printer, uses_the_default_colors_and_page_size) {
    const auto graph = triangle_digraph();

    ASSERT_TRUE(contains(print(printer(graph)), "size=\"8,11\""));
    ASSERT_TRUE(contains(print(printer(graph)), "fillcolor=\"#ffffff\""));
    ASSERT_TRUE(contains(print(printer(graph)), "color=\"#000000\""));

    printer p(graph);
    p.page_size(4.5, 6);
    ASSERT_TRUE(contains(print(p), "size=\"4.5,6\""));
}

GTEST_TEST(graphviz_printer, setters_chain_and_are_reflected) {
    const auto graph = triangle_digraph();
    printer p(graph);

    // Each setter returns the printer, so a whole drawing can be described in
    // one expression.
    p.set_vertex_label(0u, "source")
        .set_vertex_size(0u, 4.0)
        .set_vertex_color(0u, printer::color{255, 0, 0})
        .set_arc_label(0u, "first")
        .set_arc_size(0u, 3.0)
        .set_arc_color(0u, printer::color{0, 128, 255});

    const auto dot = print(p);
    ASSERT_TRUE(contains(dot, "label=\"source\""));
    ASSERT_TRUE(contains(dot, "label=\"first\""));
    ASSERT_TRUE(contains(dot, "fillcolor=\"#ff0000\""));
    ASSERT_TRUE(contains(dot, "color=\"#0080ff\""));
    // A size of 4 is drawn as a width of sqrt(4).
    ASSERT_TRUE(contains(dot, "0 [width=\"2\""));
    ASSERT_TRUE(contains(dot, "penwidth=\"3\""));

    // Vertices and arcs left untouched keep the defaults.
    ASSERT_TRUE(contains(dot, "1 [width=\"1\""));
    ASSERT_TRUE(contains(dot, "label=\"\""));
}

GTEST_TEST(graphviz_printer, map_setters_cover_every_element) {
    const auto graph = triangle_digraph();
    printer p(graph);

    auto vertex_names = views::map(
        [](const vertex & v) { return std::string("v") + char('0' + v); });
    auto arc_names = views::map(
        [](const arc & a) { return std::string("a") + char('0' + a); });

    p.set_vertex_label_map(vertex_names)
        .set_vertex_size_map(
            views::map([](const vertex & v) { return 1.0 + double(v); }))
        .set_arc_label_map(arc_names)
        .set_arc_size_map(
            views::map([](const arc & a) { return 2.0 + double(a); }));

    const auto dot = print(p);
    for(const auto & name : {"v0", "v1", "v2", "a0", "a1", "a2"})
        ASSERT_TRUE(contains(dot, std::string("label=\"") + name + "\""))
            << "missing " << name;
    ASSERT_TRUE(contains(dot, "penwidth=\"2\""));
    ASSERT_TRUE(contains(dot, "penwidth=\"4\""));
}

// Positions are rescaled to fit the page, so the bounding box of the drawing
// spans the requested width or height, and starts at the origin.
GTEST_TEST(graphviz_printer, rescales_positions_to_the_page) {
    const auto graph = triangle_digraph();
    printer p(graph);

    p.page_size(10, 10)
        .set_vertex_pos(0u, {-5.0, -5.0})
        .set_vertex_pos(1u, {0.0, 0.0})
        .set_vertex_pos(2u, {5.0, 5.0});

    const auto dot = print(p);
    ASSERT_TRUE(contains(dot, "pos=\"0,0\""));
    ASSERT_TRUE(contains(dot, "pos=\"5,5\""));
    ASSERT_TRUE(contains(dot, "pos=\"10,10\""));
    ASSERT_EQ(count(dot, "pos="), 3u);
    ASSERT_EQ(count(dot, "["), count(dot, "]"));
}

GTEST_TEST(graphviz_printer, groups_elements_by_color) {
    const auto graph = triangle_digraph();
    printer p(graph);

    const printer::color red{255, 0, 0};
    p.set_vertex_color(0u, red).set_vertex_color(2u, red);

    // The two red vertices are printed consecutively, so a single
    // `node [fillcolor=...]` directive covers both.
    const auto dot = print(p);
    ASSERT_EQ(count(dot, "fillcolor=\"#ff0000\""), 1u);
    ASSERT_EQ(count(dot, "fillcolor=\"#ffffff\""), 1u);
}

GTEST_TEST(graphviz_printer, handles_a_graph_without_arcs) {
    static_digraph_builder<static_digraph> builder(2);
    auto [graph] = builder.build();

    const auto dot = print(printer(graph));
    ASSERT_TRUE(dot.starts_with("digraph {"));
    ASSERT_TRUE(dot.ends_with("}\n"));
    ASSERT_FALSE(contains(dot, "->"));
    ASSERT_EQ(count(dot, "width="), 2u);
}
