#undef NDEBUG
#include <gtest/gtest.h>

#include <utility>

#include "melon/algorithm/traversal_forest.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/subgraph.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// the forest yields one tree per source, every vertex being a source by
// default
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(traversal_forest, test) {
    static_digraph_builder<static_digraph> builder(4);

    builder.add_arc(0, 1).add_arc(2, 1);

    auto [graph] = builder.build();

    traversal_forest alg(graph);

    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {0u, 1u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {2u}));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {3u}));
    alg.advance();
    ASSERT_TRUE(alg.finished());
}

////////////////////////////////////////////////////////////////////////////////
// the caller can supply the source range, including as a temporary
////////////////////////////////////////////////////////////////////////////////

// The two-argument constructor took std::views::all() of the *named* parameter,
// an lvalue, so it produced a ref_view where the deduction guide had already
// committed Sources to std::views::all_t<S> -- an owning_view for a temporary
// container. Passing one did not compile.
GTEST_TEST(traversal_forest, sources_from_a_temporary_range) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(2, 3);
    auto [graph] = builder.build();

    traversal_forest alg(graph, std::vector<unsigned int>{2u});

    ASSERT_FALSE(alg.finished());
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {2u, 3u}));
    alg.advance();
    ASSERT_TRUE(alg.finished());

    ASSERT_FALSE(alg.reached(0u));
    ASSERT_TRUE(alg.reached(3u));
}

////////////////////////////////////////////////////////////////////////////////
// reset() replays the run over the same sources, supplied or defaulted
////////////////////////////////////////////////////////////////////////////////

// reset() used to assign vertices(_graph) over the sources, which threw a
// user-supplied range away -- and for the two-argument form did not compile at
// all, the two ranges' iterators being unrelated types. It also skipped the
// constructor's advance(), leaving current() naming a stale tree.
GTEST_TEST(traversal_forest, reset_keeps_the_given_sources) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(2, 3);
    auto [graph] = builder.build();

    std::vector<unsigned int> sources{2u};
    traversal_forest alg(graph, sources);

    const auto count = [](auto & a) {
        int n = 0;
        for(; !a.finished(); a.advance()) ++n;
        return n;
    };

    ASSERT_EQ(count(alg), 1);

    ASSERT_EQ(std::addressof(alg.reset()), std::addressof(alg));
    ASSERT_FALSE(alg.finished());
    // still the caller's {2}, not every vertex of the graph
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {2u, 3u}));
    ASSERT_FALSE(alg.reached(0u));
    ASSERT_EQ(count(alg), 1);

    alg.reset().reset();
    ASSERT_EQ(count(alg), 1);
}

GTEST_TEST(traversal_forest, reset_restarts_the_default_sources) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(2, 1);
    auto [graph] = builder.build();

    traversal_forest alg(graph);

    const auto count = [](auto & a) {
        int n = 0;
        for(; !a.finished(); a.advance()) ++n;
        return n;
    };

    ASSERT_EQ(count(alg), 3);
    alg.reset();
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {0u, 1u}));
    ASSERT_EQ(count(alg), 3);
}

////////////////////////////////////////////////////////////////////////////////
// an empty graph or an empty source range is finished() from the start
////////////////////////////////////////////////////////////////////////////////

// advance() reads _remaining_sources.current(), so an empty source range used
// to read past the end of an empty view.
GTEST_TEST(traversal_forest, empty_graph) {
    static_digraph_builder<static_digraph> builder(0);
    auto [graph] = builder.build();

    traversal_forest alg(graph);
    ASSERT_TRUE(alg.finished());
    alg.reset();
    ASSERT_TRUE(alg.finished());
}

GTEST_TEST(traversal_forest, empty_source_range) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc(0, 1);
    auto [graph] = builder.build();

    traversal_forest alg(graph, std::vector<unsigned int>{});
    ASSERT_TRUE(alg.finished());
    ASSERT_FALSE(alg.reached(0u));
    alg.reset();
    ASSERT_TRUE(alg.finished());
}

////////////////////////////////////////////////////////////////////////////////
// regression: copying a mutable lvalue must pick the copy constructor, not
// the greedy single-argument constructor
////////////////////////////////////////////////////////////////////////////////

// The unconstrained `traversal_forest(G &&)` beat the copy constructor for a
// non-const lvalue and tried to build the algorithm out of itself. Only the
// one-argument form competes: the (graph, sources) overload takes two.
GTEST_TEST(traversal_forest,
           copying_a_mutable_lvalue_uses_the_copy_constructor) {
    static_digraph_builder<static_digraph> builder(4);
    builder.add_arc(0, 1).add_arc(2, 3);
    auto [graph] = builder.build();

    traversal_forest alg(graph);

    traversal_forest from_mutable_lvalue(alg);
    static_assert(std::same_as<decltype(alg), decltype(from_mutable_lvalue)>);
    ASSERT_TRUE(EQ_MULTISETS(from_mutable_lvalue.current(), {0u, 1u}));

    from_mutable_lvalue.advance();
    ASSERT_TRUE(EQ_MULTISETS(from_mutable_lvalue.current(), {2u, 3u}));
    ASSERT_TRUE(EQ_MULTISETS(alg.current(), {0u, 1u}));  // independent copy

    decltype(alg) from_const_lvalue(std::as_const(alg));
    ASSERT_TRUE(EQ_MULTISETS(from_const_lvalue.current(), {0u, 1u}));
}
