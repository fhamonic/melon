#undef NDEBUG
#include <gtest/gtest.h>

#include <utility>

#include "melon/algorithm/connected_components.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/undirect.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// the algorithm yields the connected components of an undirected view, one
// advance() at a time
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(connected_components, test) {
    static_digraph_builder<static_digraph> builder(8);

    builder.add_arc(0, 1)
        .add_arc(1, 2)
        .add_arc(1, 3)
        .add_arc(2, 0)
        .add_arc(4, 3)
        .add_arc(5, 6)
        .add_arc(6, 5)
        .add_arc(5, 6);

    auto [graph] = builder.build();

    auto ugraph = views::undirect(graph);

    connected_components alg(ugraph);
    // connected_components alg(views::undirect(graph));

    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {0u, 1u, 2u, 3u, 4u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {5u, 6u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {7u}));
    alg.advance();
    ASSERT_TRUE(alg.finished());
}

////////////////////////////////////////////////////////////////////////////////
// a graph with no vertices is finished() from the start, before and after
// reset()
////////////////////////////////////////////////////////////////////////////////

// advance() reads _remaining_vertices.current(); the constructor used to call
// it unconditionally, so a graph with no vertices read past the end of an empty
// view. reset() had the same problem through _reset_current_vertex().
GTEST_TEST(connected_components, empty_graph) {
    static_digraph_builder<static_digraph> builder(0);
    auto [graph] = builder.build();
    auto ugraph = views::undirect(graph);

    connected_components alg(ugraph);
    ASSERT_TRUE(alg.finished());
    alg.reset();
    ASSERT_TRUE(alg.finished());
}

////////////////////////////////////////////////////////////////////////////////
// reset() restores the constructor's state, first component included
////////////////////////////////////////////////////////////////////////////////

// reset() has to restore the constructor's state, first component included:
// without the advance() the queue stayed empty while finished() said false, so
// current() named nothing.
GTEST_TEST(connected_components, reset_restarts_the_whole_run) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(2, 3);
    auto [graph] = builder.build();
    auto ugraph = views::undirect(graph);

    connected_components alg(ugraph);

    const auto count = [](auto & a) {
        int n = 0;
        for(; !a.finished(); a.advance()) ++n;
        return n;
    };

    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {0u, 1u}));
    ASSERT_EQ(count(alg), 2);

    alg.reset();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {0u, 1u}));
    ASSERT_EQ(count(alg), 2);

    alg.reset().reset();
    ASSERT_EQ(count(alg), 2);
}

////////////////////////////////////////////////////////////////////////////////
// regression: an algorithm lvalue must not be swallowed by the greedy
// single-argument constructor
////////////////////////////////////////////////////////////////////////////////

// The unconstrained `connected_components(UG &&)` beat the copy constructor for
// a non-const lvalue and tried to build the algorithm out of itself;
// detail::not_self is what excludes it. Now that copy is deleted, the pin is
// that an algorithm is not constructible from an algorithm lvalue at all.
GTEST_TEST(connected_components, is_not_constructible_from_an_algorithm) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(2, 3);
    auto [graph] = builder.build();
    auto ugraph = views::undirect(graph);

    connected_components alg(ugraph);
    using alg_t = decltype(alg);
    static_assert(!std::is_constructible_v<alg_t, alg_t &>);
    static_assert(!std::is_constructible_v<alg_t, const alg_t &>);
    static_assert(std::is_constructible_v<alg_t, alg_t &&>);

    auto relocated = std::move(alg);
    ASSERT_TRUE(EQ_MULTISETS(relocated.current(), {0u, 1u}));
    relocated.advance();
    ASSERT_TRUE(EQ_MULTISETS(relocated.current(), {2u, 3u}));
}

////////////////////////////////////////////////////////////////////////////////
// regression: 2.8, weakly_connected_components must be constrained on the
// incidence concepts it actually uses
////////////////////////////////////////////////////////////////////////////////

// It required the *adjacency* concepts while views::undirect, which is all it
// does, requires the *incidence* ones. The two are independent, so the
// constraint gated the wrong thing in both directions: a graph with
// out_neighbors/in_neighbors but no out_arcs/in_arcs passed it and then
// hard-errored inside undirect.
namespace adjacency_only {
// A 3-cycle 0->1->2->0 described by endpoints and neighbours, with no
// out_arcs/in_arcs: enough for melon::graph (arcs + arc_source + arc_target
// give arcs_entries) and for both adjacency concepts, but not for either
// incidence one -- so views::undirect cannot be built over it.
struct neighbors_but_no_arcs {
    auto vertices() const { return std::views::iota(0u, 3u); }
    auto arcs() const { return std::views::iota(0u, 3u); }
    unsigned int arc_source(unsigned int a) const { return a; }
    unsigned int arc_target(unsigned int a) const { return (a + 1) % 3u; }
    auto out_neighbors(unsigned int v) const {
        return std::views::iota((v + 1) % 3u, (v + 1) % 3u + 1);
    }
    auto in_neighbors(unsigned int v) const {
        return std::views::iota((v + 2) % 3u, (v + 2) % 3u + 1);
    }
    template <typename T>
    auto create_vertex_map() const {
        return static_map<unsigned int, T>(3);
    }
    template <typename T>
    auto create_vertex_map(const T & d) const {
        return static_map<unsigned int, T>(3, d);
    }
};
}  // namespace adjacency_only

static_assert(melon::graph<adjacency_only::neighbors_but_no_arcs>);
static_assert(outward_adjacency_graph<adjacency_only::neighbors_but_no_arcs>);
static_assert(inward_adjacency_graph<adjacency_only::neighbors_but_no_arcs>);
static_assert(!outward_incidence_graph<adjacency_only::neighbors_but_no_arcs>);
static_assert(!inward_incidence_graph<adjacency_only::neighbors_but_no_arcs>);
static_assert(has_vertex_map<adjacency_only::neighbors_but_no_arcs>);

// The old constraint accepted it; the call now fails as an unsatisfied
// constraint naming weakly_connected_components, not as a hard error deep
// inside views::undirect.
template <typename Gr>
concept can_weakly_connect =
    requires(Gr & g) { melon::weakly_connected_components(g); };
static_assert(!can_weakly_connect<adjacency_only::neighbors_but_no_arcs>);

// and a graph that does have both incidences still works
static_assert(can_weakly_connect<static_digraph>);
