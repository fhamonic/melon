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

////////////////////////////////////////////////////////////////////////////////
// the output is a well-formed dot digraph, with sensible defaults
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// per-element and map setters are reflected in the output, untouched elements
// keep the defaults
////////////////////////////////////////////////////////////////////////////////

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

    auto vertex_names = maps::map(
        [](const vertex & v) { return std::string("v") + char('0' + v); });
    auto arc_names = maps::map(
        [](const arc & a) { return std::string("a") + char('0' + a); });

    p.set_vertex_label_map(vertex_names)
        .set_vertex_size_map(
            maps::map([](const vertex & v) { return 1.0 + double(v); }))
        .set_arc_label_map(arc_names)
        .set_arc_size_map(
            maps::map([](const arc & a) { return 2.0 + double(a); }));

    const auto dot = print(p);
    for(const auto & name : {"v0", "v1", "v2", "a0", "a1", "a2"})
        ASSERT_TRUE(contains(dot, std::string("label=\"") + name + "\""))
            << "missing " << name;
    ASSERT_TRUE(contains(dot, "penwidth=\"2\""));
    ASSERT_TRUE(contains(dot, "penwidth=\"4\""));
}

////////////////////////////////////////////////////////////////////////////////
// layout: positions are rescaled to the page, same-colored elements share one
// directive
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// a graph without arcs still prints a valid document
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(graphviz_printer, handles_a_graph_without_arcs) {
    static_digraph_builder<static_digraph> builder(2);
    auto [graph] = builder.build();

    const auto dot = print(printer(graph));
    ASSERT_TRUE(dot.starts_with("digraph {"));
    ASSERT_TRUE(dot.ends_with("}\n"));
    ASSERT_FALSE(contains(dot, "->"));
    ASSERT_EQ(count(dot, "width="), 2u);
}

////////////////////////////////////////////////////////////////////////////////
// print threads the output iterator, emits the dot `graph` keyword, and
// refuses to bind a temporary graph
////////////////////////////////////////////////////////////////////////////////

// regression: discarding the std::format_to results rewinds a positional
// iterator like char * to its start on each call, and the calls overwrite one
// another; only stateful inserters like back_insert_iterator come out whole.
GTEST_TEST(graphviz_printer, threads_positional_output_iterators) {
    const auto graph = triangle_digraph();
    printer p(graph);
    const std::string reference = print(p);

    std::vector<char> buffer(reference.size(), '\0');
    char * const end = p.print(buffer.data());
    ASSERT_EQ(static_cast<std::size_t>(end - buffer.data()), reference.size());
    ASSERT_EQ(std::string(buffer.data(), end), reference);
}

// regression: the literal is DOT's `graph` keyword, not the _graph member's
// name -- a rename that hits it emits a spurious node named _graph and loses
// the graph attributes.
GTEST_TEST(graphviz_printer, emits_the_graph_attributes_not_a_node) {
    const auto graph = triangle_digraph();
    const auto dot = print(printer(graph));

    ASSERT_TRUE(contains(dot, "graph [pad=\"0.2,0.1\""));
    ASSERT_FALSE(contains(dot, "_graph"));
}

// _graph is a reference_wrapper: binding a temporary leaves the printer
// pointing at a dead graph, so the rvalue overload is deleted (the
// mapping_ref_view precedent).
static_assert(!std::is_constructible_v<printer, static_digraph>);
static_assert(std::is_constructible_v<printer, static_digraph &>);
static_assert(std::is_constructible_v<printer, const static_digraph &>);

////////////////////////////////////////////////////////////////////////////////
// labels are escaped, so user data cannot become graph syntax
////////////////////////////////////////////////////////////////////////////////

// regression: a label interpolated raw into `label="{}"` becomes syntax. A
// DOT quoted string ends at the first unescaped '"', so a label carrying one
// closes the attribute list early and everything after it is parsed as
// further attributes -- `a" shape=box color="red` silently restyles the node.
GTEST_TEST(graphviz_printer, quotes_in_labels_do_not_inject_attributes) {
    const auto graph = triangle_digraph();
    printer p(graph);
    p.set_vertex_label(0u, std::string("a\" shape=box color=\"red"));
    const std::string dot = print(p);

    // The whole attribute list, not just the label: what the payload must not
    // do is end the quoted string, and only the closing bracket landing right
    // after it shows that it did not. (`shape=box` still occurs in the output
    // -- as label text, which is the point.)
    ASSERT_TRUE(contains(
        dot, "0 [width=\"1\" label=\"a\\\" shape=box color=\\\"red\"]"));
}

// a trailing backslash would otherwise escape the closing quote and swallow
// the rest of the line
GTEST_TEST(graphviz_printer, backslashes_in_labels_are_escaped) {
    const auto graph = triangle_digraph();
    printer p(graph);
    p.set_vertex_label(1u, std::string("c:\\"));
    const std::string dot = print(p);

    ASSERT_TRUE(contains(dot, "label=\"c:\\\\\""));
}

// arc labels go through the same path
GTEST_TEST(graphviz_printer, arc_labels_are_escaped_too) {
    const auto graph = triangle_digraph();
    printer p(graph);
    p.set_arc_label(0u, std::string("w=\"1\""));
    const std::string dot = print(p);

    ASSERT_TRUE(contains(dot, "label=\"w=\\\"1\\\"\""));
}
