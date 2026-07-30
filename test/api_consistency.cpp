// Regression tests for TODO items 2.4 (naming and const-ness drift across the
// algorithm family), 2.5 (encapsulation slips) and 2.6 (noexcept that would
// call std::terminate, and [[nodiscard]] where it buys nothing).
//
// These are family-wide invariants rather than per-algorithm behaviour, so
// they live together: the point of each one is that it holds for *every*
// member of a list, and a new algorithm that breaks the pattern should fail
// here rather than in whichever file happens to cover it.

#undef NDEBUG
#include <gtest/gtest.h>

#include <concepts>
#include <cstdint>
#include <ranges>
#include <type_traits>
#include <vector>

#include "melon/algorithm/bidirectional_dijkstra.hpp"
#include "melon/algorithm/biobjective_dijkstra.hpp"
#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/algorithm/competing_dijkstras.hpp"
#include "melon/algorithm/connected_components.hpp"
#include "melon/algorithm/depth_first_search.hpp"
#include "melon/algorithm/dijkstra.hpp"
#include "melon/algorithm/dinitz.hpp"
#include "melon/algorithm/edmonds_karp.hpp"
#include "melon/algorithm/kruskal.hpp"
#include "melon/algorithm/network_voronoi.hpp"
#include "melon/algorithm/strongly_connected_components.hpp"
#include "melon/algorithm/topological_sort.hpp"
#include "melon/algorithm/traversal_forest.hpp"
#include "melon/container/disjoint_sets.hpp"
#include "melon/container/mutable_digraph.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/container/static_filter_map.hpp"
#include "melon/numeric/bounded_value.hpp"
#include "melon/numeric/rational.hpp"
#include "melon/utility/alias_method_sampler.hpp"
#include "melon/utility/graphviz_printer.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/complete_digraph.hpp"
#include "melon/views/graph_view.hpp"
#include "melon/views/reverse.hpp"
#include "melon/views/subgraph.hpp"
#include "melon/views/undirect.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

namespace {

using G = static_digraph;
using LM = static_map<arc_t<G>, int>;
using UG = decltype(views::undirect(std::declval<G &>()));

auto build_graph() {
    static_digraph_builder<G, int> builder(4);
    builder.add_arc(0, 1, 2).add_arc(1, 2, 3).add_arc(0, 2, 9).add_arc(3, 2, 1);
    return builder.build();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// 2.4 -- the traits names follow one spelling: <algo>_traits (plural) for the
// concept, <algo>_default_traits for the struct
////////////////////////////////////////////////////////////////////////////////

// The family had `dijkstra_trait` / `network_voronoi_trait` (singular) beside
// `bentley_ottmann_traits` / `alias_method_sampler_trait`, and
// `dijkstra_default_traits` beside `default_bentley_ottmann_traits` (word
// order). One spelling now. Naming a concept is the only way to pin its name
// from a test.
template <typename T>
concept names_the_traits_concepts = requires {
    requires dijkstra_traits<dijkstra_default_traits<T, int>>;
    requires bidirectional_dijkstra_traits<
        bidirectional_dijkstra_default_traits<T, int>>;
    requires network_voronoi_traits<network_voronoi_default_traits<T, int>>;
    requires alias_method_sampler_traits<alias_method_sampler_default_traits>;
};
static_assert(names_the_traits_concepts<G>);

////////////////////////////////////////////////////////////////////////////////
// 2.4 -- finished()/current() are const on every generator
////////////////////////////////////////////////////////////////////////////////

// They were non-const on connected_components, strongly_connected_components
// and traversal_forest -- the three whose cursor is a consumable_view, whose
// empty() was itself non-const. So a `const algorithm &` could not even be
// asked whether it was done.
template <typename A>
concept const_inspectable = requires(const A & a) {
    { a.finished() } -> std::convertible_to<bool>;
    a.current();
};

static_assert(const_inspectable<breadth_first_search<graph_ref_view<G>>>);
static_assert(const_inspectable<depth_first_search<graph_ref_view<G>>>);
static_assert(const_inspectable<topological_sort<graph_ref_view<G>>>);
static_assert(
    const_inspectable<strongly_connected_components<graph_ref_view<G>>>);
static_assert(const_inspectable<connected_components<UG>>);
static_assert(const_inspectable<
              traversal_forest<graph_ref_view<G>, vertices_range_t<G>>>);
static_assert(
    const_inspectable<dijkstra<graph_ref_view<G>, maps::mapping_all_t<LM &>>>);
static_assert(const_inspectable<
              network_voronoi<graph_ref_view<G>, maps::mapping_all_t<LM &>>>);

// current() hands out a *read-only* window: it is a view onto the algorithm's
// own buffer, which the next advance() rewrites.
static_assert(
    std::is_const_v<std::remove_reference_t<std::ranges::range_reference_t<
        decltype(std::declval<
                     const strongly_connected_components<graph_ref_view<G>> &>()
                     .current())>>>);

GTEST_TEST(api_consistency, const_algorithms_can_still_be_inspected) {
    auto [graph, length_map] = build_graph();

    strongly_connected_components scc(graph);
    const auto & const_scc = scc;
    ASSERT_FALSE(const_scc.finished());
    ASSERT_FALSE(const_scc.current().empty());

    traversal_forest forest(graph);
    const auto & const_forest = forest;
    ASSERT_FALSE(const_forest.finished());

    auto undirected = views::undirect(graph);
    connected_components components(undirected);
    const auto & const_components = components;
    ASSERT_FALSE(const_components.finished());
    ASSERT_FALSE(std::ranges::empty(const_components.current()));
}

////////////////////////////////////////////////////////////////////////////////
// 2.4 -- reset() returns the algorithm, never void
////////////////////////////////////////////////////////////////////////////////

// kruskal returned void, so `alg.reset().add_source(s)` -- the idiom the docs
// show -- did not compile for it alone.
template <typename A>
concept reset_returns_self = requires(A & a) {
    { a.reset() } -> std::same_as<A &>;
};

static_assert(reset_returns_self<breadth_first_search<graph_ref_view<G>>>);
static_assert(reset_returns_self<depth_first_search<graph_ref_view<G>>>);
static_assert(reset_returns_self<topological_sort<graph_ref_view<G>>>);
static_assert(
    reset_returns_self<strongly_connected_components<graph_ref_view<G>>>);
static_assert(reset_returns_self<connected_components<UG>>);
static_assert(reset_returns_self<
              traversal_forest<graph_ref_view<G>, vertices_range_t<G>>>);
static_assert(
    reset_returns_self<dijkstra<graph_ref_view<G>, maps::mapping_all_t<LM &>>>);
static_assert(
    reset_returns_self<
        bidirectional_dijkstra<graph_ref_view<G>, maps::mapping_all_t<LM &>>>);
static_assert(reset_returns_self<
              network_voronoi<graph_ref_view<G>, maps::mapping_all_t<LM &>>>);
static_assert(reset_returns_self<
              kruskal<UG, maps::mapping_all_t<static_map<arc_t<G>, int> &>>>);

////////////////////////////////////////////////////////////////////////////////
// 2.5 -- internals stay private, and vertex_t/arc_t is the one handle spelling
////////////////////////////////////////////////////////////////////////////////

// disjoint_sets exposed its three maps, so `ds._parent_map.clear()` compiled
// and left find() reading past the end of a vector it still believed in.
template <typename DS>
concept exposes_internals = requires(DS & ds) { ds._parent_map; } ||
                            requires(DS & ds) { ds._size_map; } ||
                            requires(DS & ds) { ds._component_map; };
static_assert(!exposes_internals<disjoint_sets<unsigned int>>);
namespace controls {
// positive control: the concept does detect a published member, so the
// assertion above is not vacuously true
struct leaky {
    std::vector<unsigned int> _parent_map;
};
}  // namespace controls
static_assert(exposes_internals<controls::leaky>);

// strongly_connected_components::push_tarjan() let a caller push a vertex onto
// Tarjan's stack behind the traversal's back.
template <typename A>
concept exposes_push_tarjan = requires(A & a, vertex_t<G> v) {
    a.push_tarjan(v);
} || requires(A & a, vertex_t<G> v) { a._push_tarjan(v); };
static_assert(!exposes_push_tarjan<
              strongly_connected_components<graph_ref_view<static_digraph>>>);
namespace controls {
struct pushable {
    void push_tarjan(vertex_t<G>) {}
};
}  // namespace controls
static_assert(exposes_push_tarjan<controls::pushable>);

// vertex_t<T> / arc_t<T> are the supported spelling. static_digraph and
// complete_digraph already kept `vertex` / `arc` private; mutable_digraph and
// subgraph published them, so `melon::static_digraph::vertex` was an error
// while `melon::mutable_digraph::vertex` was not -- two rules for one concept.
template <typename T>
concept publishes_handle_aliases =
    requires { typename T::vertex; } || requires { typename T::arc; };

static_assert(!publishes_handle_aliases<static_digraph>);
static_assert(!publishes_handle_aliases<mutable_digraph>);
static_assert(
    !publishes_handle_aliases<
        subgraph_view<graph_ref_view<G>, maps::true_map, maps::true_map>>);
static_assert(!publishes_handle_aliases<graph_ref_view<G>>);
static_assert(!publishes_handle_aliases<reverse_view<graph_ref_view<G>>>);

namespace controls {
struct with_aliases {
    using vertex = unsigned int;
};
}  // namespace controls
static_assert(publishes_handle_aliases<controls::with_aliases>);

// and the supported spelling still works for all of them
static_assert(std::same_as<vertex_t<mutable_digraph>, unsigned int>);
static_assert(std::same_as<arc_t<mutable_digraph>, unsigned int>);
static_assert(std::same_as<vertex_t<static_digraph>, unsigned int>);

////////////////////////////////////////////////////////////////////////////////
// 2.6 -- nothing that can throw is marked noexcept; conditional noexcept stays
// conditional
////////////////////////////////////////////////////////////////////////////////

// Every one of these allocates -- a heap, a queue, one vertex map per
// algorithm -- or runs the caller's length map, semiring and comparator. A
// noexcept there does not stop the throw, it turns it into std::terminate with
// no diagnostic.
using dijkstra_t = dijkstra<graph_ref_view<G>, maps::mapping_all_t<LM &>>;

static_assert(!std::is_nothrow_constructible_v<dijkstra_t, G &, LM &>);
static_assert(!noexcept(std::declval<dijkstra_t &>().reset()));
static_assert(!noexcept(std::declval<dijkstra_t &>().add_source(
    std::declval<const vertex_t<G> &>())));
static_assert(!noexcept(std::declval<dijkstra_t &>().advance()));
static_assert(!noexcept(std::declval<dijkstra_t &>().run()));

using bfs_t = breadth_first_search<graph_ref_view<G>>;
static_assert(!noexcept(std::declval<bfs_t &>().reset()));
static_assert(!noexcept(
    std::declval<bfs_t &>().add_source(std::declval<const vertex_t<G> &>())));
static_assert(!noexcept(std::declval<bfs_t &>().advance()));

using scc_t = strongly_connected_components<graph_ref_view<G>>;
static_assert(!std::is_nothrow_constructible_v<scc_t, G &>);
static_assert(!noexcept(std::declval<scc_t &>().reset()));
static_assert(!noexcept(std::declval<scc_t &>().advance()));

using tf_t = traversal_forest<graph_ref_view<G>, vertices_range_t<G>>;
static_assert(!std::is_nothrow_constructible_v<tf_t, G &>);
static_assert(!noexcept(std::declval<tf_t &>().advance()));

using cc_t = connected_components<UG>;
static_assert(!noexcept(std::declval<cc_t &>().advance()));

// the containers: every create_*_map allocates, so does every builder
static_assert(!noexcept(std::declval<const G &>().create_vertex_map<int>()));
static_assert(!noexcept(std::declval<const G &>().create_arc_map<int>()));
static_assert(
    !noexcept(melon::create_vertex_map<int>(std::declval<const G &>())));
static_assert(!noexcept(melon::create_arc_map<int>(std::declval<const G &>())));
static_assert(!std::is_nothrow_constructible_v<G, const std::size_t &,
                                               std::vector<unsigned int> &,
                                               std::vector<unsigned int> &>);
static_assert(!noexcept(std::declval<mutable_digraph &>().create_vertex()));
static_assert(!noexcept(std::declval<mutable_digraph &>().create_arc(0u, 0u)));

// disjoint_sets::push push_backs into two vectors and writes the caller's map
static_assert(
    !noexcept(std::declval<disjoint_sets<unsigned int> &>().push(0u)));

// The views' noexcept is *conditional*, not absent: wrapping static_digraph,
// whose vertices()/arc_target()/endpoint maps are honestly noexcept, must not
// throw that guarantee away.
static_assert(
    noexcept(melon::vertices(std::declval<const graph_ref_view<G> &>())));
static_assert(noexcept(melon::arcs(std::declval<const graph_ref_view<G> &>())));
static_assert(
    noexcept(melon::arc_target(std::declval<const graph_ref_view<G> &>(),
                               std::declval<const arc_t<G> &>())));
static_assert(noexcept(
    melon::arc_targets_map(std::declval<const graph_ref_view<G> &>())));
static_assert(noexcept(
    melon::out_arcs(std::declval<const reverse_view<graph_ref_view<G>> &>(),
                    std::declval<const vertex_t<G> &>())));
// ... and create_*_map allocates whether or not it goes through a view
static_assert(!noexcept(
    melon::create_vertex_map<int>(std::declval<const graph_ref_view<G> &>())));

namespace throwing {
// A graph whose every operation is allowed to throw. A view over it must not
// claim noexcept -- that is the whole point of making the specification
// conditional rather than dropping it.
struct throwing_graph {
    std::vector<unsigned int> _targets{1u, 2u, 0u};
    auto vertices() const { return std::views::iota(0u, 3u); }
    auto arcs() const { return std::views::iota(0u, 3u); }
    auto out_arcs(unsigned int v) const { return std::views::iota(v, v + 1); }
    unsigned int arc_source(unsigned int a) const { return a; }
    unsigned int arc_target(unsigned int a) const { return _targets[a]; }
};
}  // namespace throwing

static_assert(melon::graph<throwing::throwing_graph>);
static_assert(!noexcept(
    melon::vertices(std::declval<const throwing::throwing_graph &>())));
static_assert(!noexcept(melon::vertices(
    std::declval<const graph_ref_view<throwing::throwing_graph> &>())));
static_assert(!noexcept(melon::arc_target(
    std::declval<const graph_ref_view<throwing::throwing_graph> &>(),
    std::declval<const unsigned int &>())));

////////////////////////////////////////////////////////////////////////////////
// 2.6 -- [[nodiscard]] sits only where a discarded result is a bug
////////////////////////////////////////////////////////////////////////////////

// It sat on constructors, including defaulted copy/move ones, where the only
// expression it can diagnose is a discarded temporary -- a spelling that is
// usually parsed as a declaration anyway. It also sat on the void-returning
// mutating CPOs, where it means nothing at all. Neither is checkable from a
// test; what *is* checkable is that the CPOs whose result must be used kept
// it, and that a discarded call to them is a warning the CI turns into an
// error (-Werror on the test target).
GTEST_TEST(api_consistency, mutating_cpos_may_be_called_for_effect) {
    mutable_digraph graph;
    const auto u = melon::create_vertex(graph);
    const auto v = melon::create_vertex(graph);
    const auto a = melon::create_arc(graph, u, v);

    // These four return void; calling them as a statement is the only way to
    // use them, and [[nodiscard]] on them was pure noise.
    melon::change_arc_target(graph, a, u);
    melon::change_arc_source(graph, a, v);
    melon::remove_arc(graph, a);
    melon::remove_vertex(graph, u);

    ASSERT_EQ(melon::num_arcs(graph), 0u);
    ASSERT_EQ(melon::num_vertices(graph), 1u);
}

////////////////////////////////////////////////////////////////////////////////
// 2.8 -- iterators return a plain prvalue from operator*, never a const one
////////////////////////////////////////////////////////////////////////////////

// `constexpr const reference operator*() const` returns a *const* prvalue,
// which inhibits moves and makes std::iterator_traits disagree with the
// `reference` typedef sitting right above it. Four sites: complete_digraph's
// custom_iota_iterator and the path iterators of dijkstra, topological_sort
// and bidirectional_dijkstra.
//
// It has to be checked on the *declared return type*, not on `*it`: for the
// scalar handle types these iterators yield, a prvalue's cv-qualification is
// stripped from the expression ([expr]/6), so iter_reference_t already reads
// `arc` and would keep doing so if the `const` came back. The day `reference`
// becomes a class type it would stop being inert -- which is the point of
// pinning it now.
namespace star_return {
template <typename T>
struct return_type;
template <typename R, typename C>
struct return_type<R (C::*)() const> {
    using type = R;
};
template <typename It>
concept star_returns_unqualified = !std::is_const_v<std::remove_reference_t<
    typename return_type<decltype(&It::operator*)>::type>>;

using complete_in_arcs = decltype(melon::in_arcs(
    std::declval<const views::complete_digraph<> &>(), 0u));

struct path_traits : dijkstra_default_traits<graph_ref_view<G>, int> {
    static constexpr bool store_paths = true;
};
using alg = dijkstra<graph_ref_view<G>, maps::mapping_all_t<LM &>, path_traits>;
using path = decltype(std::declval<const alg &>().path_to(
    std::declval<const vertex_t<G> &>()));
}  // namespace star_return

static_assert(star_return::star_returns_unqualified<
              std::ranges::iterator_t<star_return::complete_in_arcs>>);
static_assert(star_return::star_returns_unqualified<
              std::ranges::iterator_t<star_return::path>>);

// and the iterators are still well-formed input iterators
static_assert(std::ranges::input_range<star_return::complete_in_arcs>);
static_assert(std::ranges::input_range<star_return::path>);
static_assert(
    std::indirectly_readable<std::ranges::iterator_t<star_return::path>>);

////////////////////////////////////////////////////////////////////////////////
// 2.8 -- the graph constructors read their members, not the forwarded-from
// parameters
////////////////////////////////////////////////////////////////////////////////

// static_digraph and static_forward_digraph forwarded `sources` / `targets`
// into their static_map members and then went on reading the *parameters* --
// asserts, and the degree-counting loops. It only worked because static_map's
// range constructor copies; the constructors read the members now.
GTEST_TEST(api_consistency, graphs_build_correctly_from_rvalue_ranges) {
    std::vector<unsigned int> sources{0u, 0u, 1u, 2u};
    std::vector<unsigned int> targets{1u, 2u, 2u, 0u};

    const static_digraph from_rvalues(3, std::move(sources),
                                      std::move(targets));
    ASSERT_EQ(melon::num_vertices(from_rvalues), 3u);
    ASSERT_EQ(melon::num_arcs(from_rvalues), 4u);
    ASSERT_TRUE(EQ_RANGES(melon::out_arcs(from_rvalues, 0u), {0u, 1u}));
    ASSERT_TRUE(EQ_RANGES(melon::out_arcs(from_rvalues, 1u), {2u}));
    ASSERT_TRUE(EQ_RANGES(melon::out_arcs(from_rvalues, 2u), {3u}));
    ASSERT_TRUE(EQ_MULTISETS(melon::in_arcs(from_rvalues, 2u),
                             std::vector<unsigned int>{1u, 2u}));
    for(const auto a : melon::arcs(from_rvalues))
        ASSERT_EQ(melon::arc_target(from_rvalues, a),
                  std::vector<unsigned int>({1u, 2u, 2u, 0u})[a]);
}

////////////////////////////////////////////////////////////////////////////////
// 2.8 -- single-argument constructors are explicit
////////////////////////////////////////////////////////////////////////////////

// static_map's was explicit while static_filter_map's, static_digraph_builder's
// and graphviz_printer's were not, so a size or a graph converted implicitly
// into one of them -- `graphviz_printer p = g;` compiled, and every function
// taking one of these by value accepted a bare int or graph.
static_assert(
    std::constructible_from<static_map<unsigned int, int>, std::size_t>);
static_assert(!std::convertible_to<std::size_t, static_map<unsigned int, int>>);

static_assert(
    std::constructible_from<static_filter_map<unsigned int>, std::size_t>);
static_assert(
    !std::convertible_to<std::size_t, static_filter_map<unsigned int>>);

static_assert(std::constructible_from<static_digraph_builder<G>, std::size_t>);
static_assert(!std::convertible_to<std::size_t, static_digraph_builder<G>>);

static_assert(std::constructible_from<graphviz_printer<G>, const G &>);
static_assert(!std::convertible_to<const G &, graphviz_printer<G>>);

////////////////////////////////////////////////////////////////////////////////
// 2.8 -- graph views, mapping views and numeric types each live in their own
// namespace
////////////////////////////////////////////////////////////////////////////////

// melon::views used to hold both graph views (reverse, subgraph, undirect,
// complete_digraph) and mapping views (map, true_map, identity_map,
// element_map, mapping_all) -- two different abstractions that happen to share
// the word "view", so `views::map` read as though it transformed a graph. The
// mapping side moved to melon::maps. melon::integer, melon::rational and
// melon::bounded_value moved to melon::numeric: they are generic enough at
// namespace scope to collide with a user's own, and `integer` was not even one
// -- it is a rational with a unit denominator.
//
// Naming a type is the only way to pin where it lives, so these are spelled
// out rather than derived.
namespace layout {

// graph views stayed in melon::views
using reversed = melon::reverse_view<melon::graph_ref_view<G>>;
using sub = melon::subgraph_view<melon::graph_ref_view<G>,
                                 melon::maps::true_map, melon::maps::true_map>;
using complete = melon::views::complete_digraph<>;
using all_t = melon::views::graph_all_t<G &>;

// mapping views are in melon::maps
using truth = melon::maps::true_map;
using falsity = melon::maps::false_map;
using ident = melon::maps::identity_map;
using elem = melon::maps::element_map<0>;
using map_all_t = melon::maps::mapping_all_t<static_map<arc_t<G>, int> &>;

// the ownership views stay in melon itself, symmetric with their graph twins
using map_ref = melon::mapping_ref_view<static_map<arc_t<G>, int>>;
using graph_ref = melon::graph_ref_view<G>;

// the arithmetic value types are in melon::numeric
using ratio = melon::numeric::rational<int, int>;
using whole = melon::numeric::integer<std::int64_t>;
using bounded = melon::numeric::bounded_value<std::int8_t, -10, 21>;
using unit = melon::numeric::const_value<int, 1>;

}  // namespace layout

// the mapping views really are mappings, and the graph views really are graphs
static_assert(melon::graph<layout::reversed>);
static_assert(melon::graph<layout::sub>);
static_assert(melon::graph<layout::complete>);
static_assert(melon::mapping<layout::truth, unsigned int>);
static_assert(melon::mapping<layout::ident, unsigned int>);
static_assert(melon::mapping_view<layout::map_ref, arc_t<G>>);

// Only the positive direction is checkable: a namespace is not a type, so
// "melon::views no longer has true_map" cannot be spelled as a constraint --
// naming a name that is not there is a hard error, not a substitution failure.
// The assertions above name every moved type at its new home, which is what a
// drift back would have to contradict.

////////////////////////////////////////////////////////////////////////////////
// 3. The algorithm-object lifecycle is one named contract, not a convention:
// every generator-shaped algorithm models melon::traversal_algorithm --
// reset() restores the constructor's state, run() drains and returns the
// algorithm, current()/advance() require !finished() -- and the sourced ones
// model rooted_traversal_algorithm on top. A new algorithm that drifts (a
// value-returning run(), a mandatory post-source init(), a reset() returning
// void) fails right here instead of shipping an eighth dialect.
////////////////////////////////////////////////////////////////////////////////

namespace lifecycle {

using G = melon::static_digraph;
using LM = melon::static_map<unsigned int, int>;
using RG = melon::graph_ref_view<G>;
using RLM = melon::mapping_ref_view<LM>;

static_assert(melon::rooted_traversal_algorithm<
              melon::breadth_first_search<RG>, unsigned int>);
static_assert(melon::rooted_traversal_algorithm<melon::depth_first_search<RG>,
                                                unsigned int>);
static_assert(melon::rooted_traversal_algorithm<melon::dijkstra<RG, RLM>,
                                                unsigned int>);
// competing_dijkstras is rooted through a *pair* of coloured sources, so it
// models the colour-free half of the contract only.
static_assert(
    melon::traversal_algorithm<melon::competing_dijkstras<RG, RLM, RLM>>);
static_assert(melon::rooted_traversal_algorithm<
              melon::biobjective_dijkstra<RG, RLM, RLM>, unsigned int>);
static_assert(melon::traversal_algorithm<melon::topological_sort<RG>>);
static_assert(
    melon::traversal_algorithm<melon::strongly_connected_components<RG>>);
using UG = melon::undirect_view<RG>;
static_assert(melon::traversal_algorithm<melon::connected_components<UG>>);
static_assert(melon::traversal_algorithm<melon::kruskal<UG, RLM>>);
static_assert(melon::traversal_algorithm<
              melon::traversal_forest<RG, melon::vertices_range_t<G>>>);

// bidirectional_dijkstra is a point query, not a generator: it models the
// lifecycle halves it has -- reset()/run() returning itself -- and its
// answer is read through dist() after run(), like every other result
// accessor in the family. Its run() used to *be* the result, which made a
// second call return infty; dist() after a re-run must agree instead.
static_assert(
    std::same_as<decltype(std::declval<melon::bidirectional_dijkstra<
                              RG, RLM> &>()
                              .run()),
                 melon::bidirectional_dijkstra<RG, RLM> &>);

}  // namespace lifecycle

GTEST_TEST(api_consistency, run_is_idempotent_and_results_persist) {
    std::vector<std::pair<unsigned int, unsigned int>> arc_list{
        {0u, 1u}, {1u, 2u}, {2u, 3u}};
    melon::static_digraph g(4u, std::views::keys(arc_list),
                            std::views::values(arc_list));
    std::vector<int> lengths{7, 5, 9};

    melon::bidirectional_dijkstra alg(g, lengths, 0u, 3u);
    ASSERT_EQ(alg.run().dist(), 21);
    // A second run() is a no-op, and the answer is still there.
    ASSERT_EQ(alg.run().dist(), 21);
    // reset() restores the constructor's state: sources gone, distance infty.
    alg.reset();
    ASSERT_EQ(alg.dist(), std::numeric_limits<int>::max());
}

GTEST_TEST(api_consistency, sourced_competing_dijkstras_needs_no_init) {
    // Sources may arrive in any order and any colour mix; iterating right
    // away yields only blue-claimed vertices. There used to be a mandatory
    // init() between the last add_*_source and the first begin(), and
    // forgetting it yielded red-claimed vertices.
    std::vector<std::pair<unsigned int, unsigned int>> arc_list{
        {0u, 1u}, {1u, 3u}, {2u, 1u}};
    melon::static_digraph g(4u, std::views::keys(arc_list),
                            std::views::values(arc_list));
    std::vector<int> lengths{5, 5, 1};

    melon::competing_dijkstras alg(g, lengths, lengths);
    alg.add_blue_source(0u);
    alg.add_red_source(2u);  // red is strictly closer to vertex 1
    std::vector<unsigned int> blue_claimed;
    for(const auto & [v, entry] : alg) blue_claimed.push_back(v);
    // Blue claims only its own source: red reaches 1 first (1 < 5) and 3
    // through it (2 < 10).
    ASSERT_EQ(blue_claimed, (std::vector<unsigned int>{0u}));
}

////////////////////////////////////////////////////////////////////////////////
// 4. Algorithm members are always views -- the std::ranges adaptor rule
// (transform_view<V> requires view<V>), applied to the whole algorithm
// family. Naming an algorithm with a raw container member is ill-formed
// (checkable through a requires-expression, where the head constraint is a
// substitution failure), value ownership is spelled graph_owning_view /
// mapping_owning_view, and the silent deep-copy the raw-member spelling used
// to perform is unspellable. CTAD is unaffected: the deduction guides only
// ever produced view types.
////////////////////////////////////////////////////////////////////////////////

namespace view_only_storage {

using G = melon::static_digraph;
using SM = melon::static_map<unsigned int, int>;

// Through dependent helper concepts, because a *non-dependent*
// requires-expression gets no SFINAE: naming the ill-formed specialization
// directly would be a hard error, which is exactly what these pin.
template <typename GG, typename MM>
concept spellable_dijkstra =
    requires { typename melon::dijkstra<GG, MM>; };
template <typename GG>
concept spellable_bfs = requires { typename melon::breadth_first_search<GG>; };
template <typename GG>
concept spellable_scc =
    requires { typename melon::strongly_connected_components<GG>; };

// Raw containers are rejected at the class head...
static_assert(!spellable_dijkstra<G, SM>);
static_assert(!spellable_dijkstra<melon::graph_ref_view<G>, SM>);
static_assert(!spellable_dijkstra<G, melon::mapping_ref_view<SM>>);
static_assert(!spellable_bfs<G>);
static_assert(!spellable_scc<G>);

// ...and ownership is spelled with the owning views, which store the same
// bytes the raw member would have.
static_assert(melon::traversal_algorithm<
              melon::dijkstra<melon::graph_owning_view<G>,
                              melon::mapping_owning_view<SM>>>);

// Constructor honesty is unchanged by the head constraints, in both
// directions: an lvalue map ref-views into a view-typed member, an rvalue
// cannot.
using RefD =
    melon::dijkstra<melon::graph_ref_view<G>, melon::mapping_ref_view<SM>>;
static_assert(std::is_constructible_v<RefD, G &, SM &>);
static_assert(!std::is_constructible_v<RefD, G &, SM &&>);

}  // namespace view_only_storage
