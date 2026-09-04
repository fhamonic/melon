#undef NDEBUG
#include <gtest/gtest.h>

// GCC 15 reports a potential null dereference when a std::vector handed out
// by a provider lambda is subscripted inside an inlined assert: it cannot
// rule out a zero-vertex graph, whose vector has no buffer. A false positive
// specific to std::vector-backed maps, not to the views.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <memory>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/algorithm/bidirectional_dijkstra.hpp"
#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/algorithm/connected_components.hpp"
#include "melon/algorithm/depth_first_search.hpp"
#include "melon/algorithm/dijkstra.hpp"
#include "melon/algorithm/dinitz.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/reverse.hpp"
#include "melon/views/subgraph.hpp"
#include "melon/views/undirect.hpp"
#include "melon/views/with_maps.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

namespace {

// A conforming digraph with no map factory at all. Incidence ranges are
// spans so that dinitz's per-vertex cursor maps stay default-constructible.
class bare_digraph {
public:
    using vertex = unsigned int;
    using arc = unsigned int;

private:
    std::vector<std::vector<arc>> _out_arcs, _in_arcs;
    std::vector<std::pair<vertex, vertex>> _arc_ends;

public:
    bare_digraph(vertex n,
                 const std::vector<std::pair<vertex, vertex>> & arc_pairs)
        : _out_arcs(n), _in_arcs(n) {
        for(const auto & [s, t] : arc_pairs) {
            const arc a = static_cast<arc>(_arc_ends.size());
            _out_arcs[s].push_back(a);
            _in_arcs[t].push_back(a);
            _arc_ends.emplace_back(s, t);
        }
    }
    std::size_t num_vertices() const { return _out_arcs.size(); }
    std::size_t num_arcs() const { return _arc_ends.size(); }
    auto vertices() const {
        return std::views::iota(vertex{0},
                                static_cast<vertex>(_out_arcs.size()));
    }
    auto arcs() const {
        return std::views::iota(arc{0}, static_cast<arc>(_arc_ends.size()));
    }
    vertex arc_source(arc a) const { return _arc_ends[a].first; }
    vertex arc_target(arc a) const { return _arc_ends[a].second; }
    std::span<const arc> out_arcs(vertex v) const { return _out_arcs[v]; }
    std::span<const arc> in_arcs(vertex v) const { return _in_arcs[v]; }
};
static_assert(graph<bare_digraph>);
static_assert(!has_vertex_map<bare_digraph>);
static_assert(!has_arc_map<bare_digraph>);

// The two generic providers: a fresh vector per request.
inline constexpr auto vectors = []<typename T>(auto, const auto & g)
    requires std::default_initializable<T>
{ return std::vector<T>(melon::num_vertices(g)); };
inline constexpr auto arc_vectors = []<typename T>(auto, const auto & g)
    requires std::default_initializable<T>
{ return std::vector<T>(melon::num_arcs(g)); };

// A projection into one field of a shared record array. Co-owning, so a map
// extracted from an expiring algorithm keeps the storage alive by itself.
template <typename Record, typename T, T Record::*Field>
struct field_projection {
    std::shared_ptr<std::vector<Record>> records;
    T & operator[](unsigned int k) const { return (*records)[k].*Field; }
};
template <typename T>
struct shared_vector {
    std::shared_ptr<std::vector<T>> values;
    T & operator[](unsigned int k) const { return (*values)[k]; }
};

// The six-vertex symmetric graph of test/bidirectional_dijkstra.cpp.
auto symmetric_graph() {
    static_digraph_builder<static_digraph, int> builder(6);
    builder.add_arc({0, 1}, 7)
        .add_arc({0, 2}, 9)
        .add_arc({0, 5}, 14)
        .add_arc({1, 0}, 7)
        .add_arc({1, 2}, 10)
        .add_arc({1, 3}, 15)
        .add_arc({2, 0}, 9)
        .add_arc({2, 1}, 10)
        .add_arc({2, 3}, 12)
        .add_arc({2, 5}, 2)
        .add_arc({3, 1}, 15)
        .add_arc({3, 2}, 12)
        .add_arc({3, 4}, 6)
        .add_arc({4, 3}, 6)
        .add_arc({4, 5}, 9)
        .add_arc({5, 0}, 14)
        .add_arc({5, 2}, 2)
        .add_arc({5, 4}, 9);
    return builder.build();
}
const std::vector<std::pair<unsigned int, int>> symmetric_dists_from_0 = {
    {0, 0}, {1, 7}, {2, 9}, {5, 11}, {4, 20}, {3, 21}};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// the three adaptors wrap like graph_all -- ref view for lvalues, owning view
// for rvalues, pass-through for views -- keep the wrapped graph's kind and
// borrowedness, cost nothing for captureless lambdas, and pipe
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(with_maps, graph_view_shape) {
    using G = static_digraph;
    using F = std::remove_const_t<decltype(vectors)>;
    using AF = std::remove_const_t<decltype(arc_vectors)>;
    using V = with_vertex_maps_view<graph_ref_view<G>, F>;

    static_assert(std::same_as<decltype(views::with_vertex_maps(
                                   std::declval<G &>(), vectors)),
                               V>);
    static_assert(
        std::same_as<decltype(views::with_vertex_maps(std::declval<const G &>(),
                                                      vectors)),
                     with_vertex_maps_view<graph_ref_view<const G>, F>>);
    static_assert(std::same_as<decltype(views::with_vertex_maps(
                                   std::declval<G>(), vectors)),
                               with_vertex_maps_view<graph_owning_view<G>, F>>);
    static_assert(std::same_as<decltype(views::with_arc_maps(
                                   std::declval<G &>(), arc_vectors)),
                               with_arc_maps_view<graph_ref_view<G>, AF>>);
    static_assert(graph_view<V>);
    static_assert(!undirected_graph_view<V>);
    static_assert(outward_incidence_graph<V> && inward_incidence_graph<V>);

    using UG = decltype(views::undirect(std::declval<G &>()));
    using UV = decltype(views::with_vertex_maps(std::declval<UG>(), vectors));
    static_assert(std::same_as<UV, with_vertex_maps_view<UG, F>>);
    static_assert(undirected_graph_view<UV> && !graph_view<UV>);
    static_assert(has_incidence<UV>);
    static_assert(std::same_as<decltype(views::with_edge_maps(
                                   std::declval<UG>(), arc_vectors)),
                               with_edge_maps_view<UG, AF>>);

#if !defined(_MSC_VER)
    static_assert(sizeof(V) == sizeof(reverse_view<graph_ref_view<G>>));
#endif
    static_assert(enable_borrowed_graph<V>);
    static_assert(
        !enable_borrowed_graph<with_vertex_maps_view<graph_owning_view<G>, F>>);
    static_assert(std::copyable<V>);

    static_assert(std::same_as<decltype(std::declval<G &>() |
                                        views::with_vertex_maps(vectors)),
                               V>);
    static_assert(std::same_as<decltype(std::declval<G &>() |
                                        views::with_arc_maps(arc_vectors)),
                               with_arc_maps_view<graph_ref_view<G>, AF>>);
    static_assert(std::same_as<decltype(std::declval<UG>() |
                                        views::with_edge_maps(arc_vectors)),
                               with_edge_maps_view<UG, AF>>);
    static_assert(std::same_as<decltype(std::declval<UG>() |
                                        views::with_vertex_maps(vectors)),
                               UV>);
    static_assert(std::same_as<
                  decltype(std::declval<G &>() | views::reverse |
                           views::with_vertex_maps(vectors)),
                  with_vertex_maps_view<reverse_view<graph_ref_view<G>>, F>>);
}

////////////////////////////////////////////////////////////////////////////////
// a graph with no factories runs every algorithm from a single lambda, lvalue
// or owning rvalue base, stacked under other views, mid-run move included
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(with_maps, factoryless_graph_runs_the_algorithms) {
    bare_digraph g(6, {{0, 1}, {0, 2}, {1, 3}, {2, 3}, {3, 4}, {4, 5}, {2, 5}});
    const std::vector<int> lengths = {7, 9, 15, 12, 6, 9, 20};
    const std::vector<std::pair<unsigned int, int>> expected = {
        {0, 0}, {1, 7}, {2, 9}, {3, 21}, {4, 27}, {5, 29}};

    auto view = views::with_vertex_maps(g, vectors);
    static_assert(has_vertex_map<decltype(view)>);
    static_assert(!has_arc_map<decltype(view)>);
    static_assert(
        std::same_as<vertex_map_t<decltype(view), int>, std::vector<int>>);
    static_assert(std::same_as<vertex_map_t<decltype(view), std::size_t,
                                            dijkstra_roles::heap_index>,
                               std::vector<std::size_t>>);

    std::vector<std::pair<unsigned int, int>> reached;
    for(auto && [v, d] : dijkstra(view, lengths, 0u))
        reached.emplace_back(v, d);
    ASSERT_TRUE(EQ_RANGES(reached, expected));

    breadth_first_search bfs(view, 0u);
    ASSERT_EQ(std::ranges::distance(bfs), 6);

    depth_first_search dfs(view, 0u);
    dfs.advance();
    auto moved = std::move(dfs);
    moved.run();
    for(auto && v : vertices(g)) ASSERT_TRUE(moved.reached(v));

    auto sub =
        views::subgraph(views::with_vertex_maps(bare_digraph(g), vectors));
    reached.clear();
    for(auto && [v, d] : dijkstra(sub, lengths, 0u)) reached.emplace_back(v, d);
    ASSERT_TRUE(EQ_RANGES(reached, expected));

    auto both = views::with_arc_maps(view, arc_vectors);
    static_assert(has_arc_map<decltype(both)> &&
                  has_vertex_map<decltype(both)>);
    const std::vector<int> capacities = {3, 5, 3, 2, 4, 6, 3};
    dinitz flow(both, capacities, 0u, 5u);
    ASSERT_EQ(flow.run().flow_value(), 7);
}

////////////////////////////////////////////////////////////////////////////////
// a lambda naming a role serves that role -- into storage that already exists
// -- and every other request falls through to the wrapped graph's factory
////////////////////////////////////////////////////////////////////////////////

namespace {
struct record {
    std::size_t heap_index;
    bool reached;
};
using heap_index_field =
    field_projection<record, std::size_t, &record::heap_index>;
using reached_field = field_projection<record, bool, &record::reached>;
}  // namespace

GTEST_TEST(with_maps, interior_roles_and_fall_through) {
    auto [graph, lengths] = symmetric_graph();
    auto slots = std::make_shared<std::vector<record>>(num_vertices(graph),
                                                       record{12345, true});
    auto heap_index_slots =
        [slots]<typename T>(dijkstra_roles::heap_index, const auto &)
        requires std::same_as<T, std::size_t>
    { return heap_index_field{slots}; };
    auto reached_slots =
        [slots]<typename T>(breadth_first_search_roles::reached, const auto &)
        requires std::same_as<T, bool>
    { return reached_field{slots}; };

    auto view = views::with_vertex_maps(graph, heap_index_slots, reached_slots);
    using V = decltype(view);
    static_assert(
        std::same_as<vertex_map_t<V, std::size_t, dijkstra_roles::heap_index>,
                     heap_index_field>);
    static_assert(
        std::same_as<vertex_map_t<V, bool, breadth_first_search_roles::reached>,
                     reached_field>);
    static_assert(
        std::same_as<vertex_map_t<V, int>, vertex_map_t<static_digraph, int>>);
    static_assert(std::same_as<vertex_map_t<V, std::size_t>,
                               vertex_map_t<static_digraph, std::size_t>>);
    static_assert(
        std::same_as<arc_map_t<V, int>, arc_map_t<static_digraph, int>>);
    // the traits' heap is built on the projection
    static_assert(
        std::same_as<dijkstra_default_traits<V, int>::heap::index_map_type,
                     heap_index_field>);

    std::vector<std::pair<unsigned int, int>> reached;
    for(auto && [v, d] : dijkstra(view, lengths, 0u))
        reached.emplace_back(v, d);
    ASSERT_TRUE(EQ_RANGES(reached, symmetric_dists_from_0));
    for(auto && v : vertices(graph)) ASSERT_NE((*slots)[v].heap_index, 12345u);

    // the derived default-value form filled the interior slots
    breadth_first_search bfs(view);
    for(auto && v : vertices(graph)) ASSERT_FALSE((*slots)[v].reached);
    bfs.add_source(3u).run();
    for(auto && v : vertices(graph)) ASSERT_TRUE((*slots)[v].reached);

    // the first lambda serving a request owns it: a generic lambda listed
    // after the role-specific one leaves it the role and wins over the base
    // graph for everything else; listed before, it shadows it
    auto v1 = views::with_vertex_maps(graph, heap_index_slots, vectors);
    auto v2 = views::with_vertex_maps(graph, vectors, heap_index_slots);
    static_assert(
        std::same_as<
            vertex_map_t<decltype(v1), std::size_t, dijkstra_roles::heap_index>,
            heap_index_field>);
    static_assert(
        std::same_as<
            vertex_map_t<decltype(v2), std::size_t, dijkstra_roles::heap_index>,
            std::vector<std::size_t>>);
    static_assert(
        std::same_as<vertex_map_t<decltype(v1), int>, std::vector<int>>);
}

////////////////////////////////////////////////////////////////////////////////
// the collision roles exist for: bidirectional_dijkstra's two std::size_t
// heap-index maps land in two distinct fields, never one shared slot
////////////////////////////////////////////////////////////////////////////////

namespace {
struct bi_record {
    std::size_t forward;
    std::size_t reverse;
};
// One projection type for both heap-index roles, the field picked at run
// time: the traits name a single heap type, so the graph must answer the two
// roles with the same map type (pinned by the algorithm's static_assert).
struct bi_field {
    std::shared_ptr<std::vector<bi_record>> records;
    std::size_t bi_record::*field;
    std::size_t & operator[](unsigned int k) const {
        return (*records)[k].*field;
    }
};
}  // namespace

GTEST_TEST(with_maps, same_value_type_roles_stay_distinct) {
    auto [graph, lengths] = symmetric_graph();
    constexpr std::size_t untouched = 777;
    auto slots = std::make_shared<std::vector<bi_record>>(
        num_vertices(graph), bi_record{untouched, untouched});
    auto view = views::with_vertex_maps(
        graph,
        [slots]<typename T>(bidirectional_dijkstra_roles::forward_heap_index,
                            const auto &)
            requires std::same_as<T, std::size_t>
                     { return bi_field{slots, &bi_record::forward}; },
                     [slots]<typename T>(
                         bidirectional_dijkstra_roles::reverse_heap_index,
                         const auto &)
                         requires std::same_as<T, std::size_t>
        { return bi_field{slots, &bi_record::reverse}; });

    bidirectional_dijkstra alg(view, lengths, 0u, 3u);
    ASSERT_EQ(alg.run().dist(), 21);
    ASSERT_TRUE(EQ_MULTISETS(alg.path(), {1, 8}));
    ASSERT_NE((*slots)[0].forward, untouched);
    ASSERT_NE((*slots)[3].reverse, untouched);
    ASSERT_EQ((*slots)[0].forward, 0u);
    ASSERT_EQ((*slots)[3].reverse, 0u);
}

////////////////////////////////////////////////////////////////////////////////
// the extraction contract holds over interior maps: a result map moved out of
// an expiring algorithm outlives the view, because the projection co-owns its
// buffer -- a borrowing projection made this a heap-use-after-free
////////////////////////////////////////////////////////////////////////////////

namespace {
template <typename G>
struct store_distances_traits : dijkstra_default_traits<G, int> {
    static constexpr bool store_distances = true;
};
}  // namespace

GTEST_TEST(with_maps, extracted_maps_outlive_the_view) {
    auto [graph, lengths] = symmetric_graph();

    auto dists = std::make_shared<std::vector<int>>(num_vertices(graph), -1);
    auto extracted = [&] {
        auto view = views::with_vertex_maps(
            graph, [dists]<typename T>(dijkstra_roles::distance, const auto &)
                requires std::same_as<T, int>
            { return shared_vector<int>{dists}; });
        using DV = decltype(view);
        static_assert(
            std::same_as<vertex_map_t<DV, int, dijkstra_roles::distance>,
                         shared_vector<int>>);
        return std::move(
                   dijkstra(store_distances_traits<DV>{}, view, lengths, 0u)
                       .run())
            .dists_map();
    }();
    ASSERT_EQ(dists.use_count(), 2);
    dists.reset();
    for(auto && [v, d] : symmetric_dists_from_0) ASSERT_EQ(extracted[v], d);

    auto flows = std::make_shared<std::vector<int>>(num_arcs(graph), 0);
    auto extracted_flows = [&] {
        auto view = views::with_arc_maps(
            graph, [flows]<typename T>(dinitz_roles::flow, const auto &)
                requires std::same_as<T, int>
            { return shared_vector<int>{flows}; });
        static_assert(
            std::same_as<arc_map_t<decltype(view), int, dinitz_roles::flow>,
                         shared_vector<int>>);
        return std::move(dinitz(view, lengths, 0u, 3u).run()).flows_map();
    }();
    ASSERT_EQ(flows.use_count(), 2);
    flows.reset();
    int out_of_0 = 0;
    for(auto && a : out_arcs(graph, 0u)) out_of_0 += extracted_flows[a];
    ASSERT_EQ(out_of_0, dinitz(graph, lengths, 0u, 3u).run().flow_value());
}

////////////////////////////////////////////////////////////////////////////////
// a lambda may declare the bare form, the filled form or both; the missing one
// is derived -- filled as bare + assignment, bare as filled with a
// value-initialized T -- and a form with a native owner is never derived
////////////////////////////////////////////////////////////////////////////////

namespace {
inline constexpr auto default_only = []<typename T>(auto, const auto & g,
                                                    const T & d) {
    return std::vector<T>(melon::num_vertices(g), d);
};
inline constexpr auto pack_forms = []<typename T>(auto, const auto & g,
                                                  const auto &... d) {
    return std::vector<T>(melon::num_vertices(g), d...);
};
struct some_role {};
}  // namespace

GTEST_TEST(with_maps, both_forms_native_or_derived) {
    bare_digraph g(4, {{0, 1}, {1, 2}, {2, 3}});

    auto filled_only = views::with_vertex_maps(g, default_only);
    static_assert(has_vertex_map<decltype(filled_only)>);
    auto zeros = create_vertex_map<int>(filled_only);
    for(auto && v : vertices(g)) ASSERT_EQ(zeros[v], 0);
    auto fives = create_vertex_map<int>(filled_only, 5);
    for(auto && v : vertices(g)) ASSERT_EQ(fives[v], 5);

    auto bare_only = views::with_vertex_maps(g, vectors);
    auto sevens = create_vertex_map<int>(bare_only, 7);
    for(auto && v : vertices(g)) ASSERT_EQ(sevens[v], 7);

    auto packed = views::with_vertex_maps(g, pack_forms);
    auto nines = create_vertex_map<int>(packed, 9);
    for(auto && v : vertices(g)) ASSERT_EQ(nines[v], 9);
    ASSERT_EQ(create_vertex_map<int>(packed).size(), 4u);

    // the first lambda serving a role owns both of its forms: a second lambda
    // for the same role is never reached, the missing form is derived instead
    auto calls = std::make_shared<std::pair<int, int>>(0, 0);
    auto split = views::with_vertex_maps(
        g,
        [calls]<typename T>(some_role, const auto & gr) {
            ++calls->first;
            return std::vector<T>(melon::num_vertices(gr));
        },
        [calls]<typename T>(some_role, const auto & gr, const T & d) {
            ++calls->second;
            return std::vector<T>(melon::num_vertices(gr), d);
        },
        vectors);
    (void)create_vertex_map<int, some_role>(split);
    ASSERT_EQ(*calls, (std::pair{1, 0}));
    auto threes = create_vertex_map<int, some_role>(split, 3);
    ASSERT_EQ(*calls, (std::pair{2, 0}));
    for(auto && v : vertices(g)) ASSERT_EQ(threes[v], 3);
    (void)create_vertex_map<int>(split, 3);
    ASSERT_EQ(*calls, (std::pair{2, 0}));
}

////////////////////////////////////////////////////////////////////////////////
// a role-specific lambda listed first owns its role whichever form it
// declares; the form the algorithm asks for is derived when it is the other
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(with_maps, first_match_owns_the_role_in_either_form) {
    auto [graph, lengths] = symmetric_graph();
    auto slots = std::make_shared<std::vector<record>>(num_vertices(graph),
                                                       record{12345, true});
    auto reached_bare =
        [slots]<typename T>(breadth_first_search_roles::reached, const auto &)
        requires std::same_as<T, bool>
    { return reached_field{slots}; };
    auto reached_filled =
        [slots]<typename T>(breadth_first_search_roles::reached, const auto &,
                            const T & d)
        requires std::same_as<T, bool>
    {
        for(auto & r : *slots) r.reached = d;
        return reached_field{slots};
    };

    // bare-only owner, filled request (what BFS makes): derived by assignment
    auto v1 = views::with_vertex_maps(graph, reached_bare, pack_forms);
    breadth_first_search bfs1(v1);
    for(auto && v : vertices(graph)) ASSERT_FALSE((*slots)[v].reached);
    bfs1.add_source(0u).run();
    for(auto && v : vertices(graph)) ASSERT_TRUE((*slots)[v].reached);

    // filled-only owner, same request: native
    auto v2 = views::with_vertex_maps(graph, reached_filled, vectors);
    breadth_first_search bfs2(v2);
    for(auto && v : vertices(graph)) ASSERT_FALSE((*slots)[v].reached);
    bfs2.add_source(0u).run();
    for(auto && v : vertices(graph)) ASSERT_TRUE((*slots)[v].reached);

    // filled-only owner, bare request (dijkstra's heap index): derived with a
    // value-initialized T
    auto heap_index_filled = [slots]<typename T>(dijkstra_roles::heap_index,
                                                 const auto &, const T & d)
        requires std::same_as<T, std::size_t>
    {
        for(auto & r : *slots) r.heap_index = d;
        return heap_index_field{slots};
    };
    auto v3 = views::with_vertex_maps(graph, heap_index_filled, vectors);
    std::vector<std::pair<unsigned int, int>> reached;
    for(auto && [v, d] : dijkstra(v3, lengths, 0u)) reached.emplace_back(v, d);
    ASSERT_TRUE(EQ_RANGES(reached, symmetric_dists_from_0));
    for(auto && v : vertices(graph)) ASSERT_NE((*slots)[v].heap_index, 12345u);

    // the generic lambda listed first shadows the role-specific one
    auto shadowed = views::with_vertex_maps(graph, vectors, reached_bare);
    static_assert(
        std::same_as<vertex_map_t<decltype(shadowed), bool,
                                  breadth_first_search_roles::reached>,
                     std::vector<bool>>);
}

////////////////////////////////////////////////////////////////////////////////
// what is not a provider: a generic lambda without an explicit value-type
// parameter, and a lambda that is not const-callable -- each serves nothing,
// so the wrapped graph answers, or nothing does
////////////////////////////////////////////////////////////////////////////////

namespace {
inline constexpr auto no_explicit_parameter = [](auto, const auto & g) {
    return std::vector<int>(melon::num_vertices(g));
};
inline constexpr auto not_const = []<typename T>(auto, const auto & g) mutable {
    return std::vector<T>(melon::num_vertices(g));
};
inline constexpr auto heap_index_only =
    []<typename T>(dijkstra_roles::heap_index, const auto & g)
    requires std::same_as<T, std::size_t>
{ return std::vector<std::size_t>(melon::num_vertices(g)); };
template <typename G, typename... Fs>
using wrapped =
    decltype(views::with_vertex_maps(std::declval<G>(), std::declval<Fs>()...));
}  // namespace

static_assert(has_vertex_map<wrapped<static_digraph &, decltype(vectors)>>);
static_assert(has_vertex_map<wrapped<bare_digraph &, decltype(vectors),
                                     decltype(default_only)>>);
static_assert(
    !has_vertex_map<wrapped<bare_digraph &, decltype(no_explicit_parameter)>>);
static_assert(!has_vertex_map<wrapped<bare_digraph &, decltype(not_const)>>);
// a role-specific lambda alone serves its role and nothing else
static_assert(
    !has_vertex_map<wrapped<bare_digraph &, decltype(heap_index_only)>>);
static_assert(has_vertex_map<wrapped<bare_digraph &, decltype(heap_index_only)>,
                             std::size_t, dijkstra_roles::heap_index>);

////////////////////////////////////////////////////////////////////////////////
// default_role is a role like any other: a lambda naming it serves the
// role-less requests only -- every named role falls through to the wrapped
// graph, or to nothing -- and a non-default-constructible T is unserved by
// the generic providers, which are constrained like the container factories
////////////////////////////////////////////////////////////////////////////////

namespace {
inline constexpr auto default_role_only = []<typename T>(default_role,
                                                         const auto & g) {
    return std::vector<T>(melon::num_vertices(g));
};
struct no_default_ctor {
    explicit no_default_ctor(int) {}
};
}  // namespace

static_assert(
    has_vertex_map<wrapped<bare_digraph &, decltype(default_role_only)>, int>);
static_assert(
    !has_vertex_map<wrapped<bare_digraph &, decltype(default_role_only)>, int,
                    some_role>);
static_assert(std::same_as<
              vertex_map_t<
                  wrapped<static_digraph &, decltype(default_role_only)>, int>,
              std::vector<int>>);
static_assert(
    std::same_as<
        vertex_map_t<wrapped<static_digraph &, decltype(default_role_only)>,
                     int, some_role>,
        vertex_map_t<static_digraph, int, some_role>>);
static_assert(!has_vertex_map<wrapped<bare_digraph &, decltype(vectors)>,
                              no_default_ctor>);
// a filled-only lambda cannot serve it either: its bare form has nothing to
// derive from, so has_vertex_map is false rather than a hard error
static_assert(!has_vertex_map<wrapped<bare_digraph &, decltype(default_only)>,
                              no_default_ctor>);

////////////////////////////////////////////////////////////////////////////////
// a graph of the wrong kind in first position is rejected, not stored as a
// lambda by the binding overload
////////////////////////////////////////////////////////////////////////////////

namespace {
using undirected_lvalue =
    decltype(views::undirect(std::declval<bare_digraph &>()));
using arc_vectors_t = std::remove_const_t<decltype(arc_vectors)>;
}  // namespace

static_assert(std::invocable<decltype(views::with_arc_maps), bare_digraph &,
                             const arc_vectors_t &>);
static_assert(!std::invocable<decltype(views::with_arc_maps),
                              undirected_lvalue &, const arc_vectors_t &>);
static_assert(!std::invocable<decltype(views::with_arc_maps),
                              const undirected_lvalue &, arc_vectors_t>);
static_assert(std::invocable<decltype(views::with_edge_maps),
                             undirected_lvalue &, const arc_vectors_t &>);
static_assert(!std::invocable<decltype(views::with_edge_maps), bare_digraph &,
                              const arc_vectors_t &>);
static_assert(!std::invocable<decltype(views::with_edge_maps), bare_digraph,
                              arc_vectors_t>);

////////////////////////////////////////////////////////////////////////////////
// undirected graphs: vertex maps over a factory-less undirected graph, edge
// maps served and falling through
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(with_maps, undirected_graphs) {
    bare_digraph g(5, {{0, 1}, {1, 2}, {3, 4}});
    auto ug = views::undirect(g);
    static_assert(!has_vertex_map<decltype(ug)>);
    auto view = views::with_vertex_maps(ug, vectors);
    static_assert(has_vertex_map<decltype(view)>);

    connected_components alg(view);
    std::size_t count = 0;
    for(auto && component : alg) {
        ++count;
        (void)component;
    }
    ASSERT_EQ(count, 2u);

    auto [graph, lengths] = symmetric_graph();
    auto marks = std::make_shared<std::vector<int>>(num_arcs(graph), 0);
    auto eview = views::with_edge_maps(
        views::undirect(graph), [marks]<typename T>(some_role, const auto &)
            requires std::same_as<T, int>
        { return shared_vector<int>{marks}; });
    using EV = decltype(eview);
    static_assert(
        std::same_as<edge_map_t<EV, int, some_role>, shared_vector<int>>);
    static_assert(
        std::same_as<edge_map_t<EV, int>, arc_map_t<static_digraph, int>>);
    static_assert(
        std::same_as<vertex_map_t<EV, int>, vertex_map_t<static_digraph, int>>);
    auto m = create_edge_map<int, some_role>(eview, 4);
    for(auto && e : edges(eview)) ASSERT_EQ(m[e], 4);
    for(auto && e : edges(eview)) ASSERT_EQ((*marks)[e], 4);
}
