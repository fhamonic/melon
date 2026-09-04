#undef NDEBUG
#include <gtest/gtest.h>

#include <concepts>
#include <cstddef>
#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/container/mutable_digraph.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/container/static_map.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/graph.hpp"
#include "melon/maps/element.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/views/graph_view.hpp"
#include "melon/views/reverse.hpp"
#include "melon/views/subgraph.hpp"
#include "melon/views/undirect.hpp"
#include "melon/views/undirected_graph_view.hpp"

using namespace melon;

namespace {

struct role_a {};
struct role_b {};

// A map that remembers the role its factory was asked for.
template <typename Key, typename T, typename Role>
struct role_map : std::vector<T> {
    using role = Role;
    using std::vector<T>::vector;
    T & operator[](Key k) { return std::vector<T>::operator[](k); }
    const T & operator[](Key k) const { return std::vector<T>::operator[](k); }
};

// A role-aware view over static_digraph: the two-parameter factory shape for
// vertex and arc maps, everything else forwarded.
struct role_aware_graph
    : detail::graph_forwarding_interface<role_aware_graph, static_digraph> {
    friend detail::graph_forwarding_interface<role_aware_graph, static_digraph>;
    const static_digraph * _g;
    const static_digraph & _forwarding_base() const noexcept { return *_g; }

    template <typename T, typename Role = default_role>
    auto create_vertex_map() const {
        return role_map<unsigned int, T, Role>(melon::num_vertices(*_g));
    }
    template <typename T, typename Role = default_role>
    auto create_vertex_map(const T & d) const {
        return role_map<unsigned int, T, Role>(melon::num_vertices(*_g), d);
    }
    template <typename T, typename Role = default_role>
    auto create_arc_map() const {
        return role_map<unsigned int, T, Role>(melon::num_arcs(*_g));
    }
    template <typename T, typename Role = default_role>
    auto create_arc_map(const T & d) const {
        return role_map<unsigned int, T, Role>(melon::num_arcs(*_g), d);
    }
};

}  // namespace

// ADL free-function factories, in the graph's own namespace: the legacy
// one-parameter shape on one graph, the role-aware shape on the other.
namespace adl_lib {
struct legacy_graph {
    std::size_t n;
};
inline auto vertices(const legacy_graph & g) {
    return std::views::iota(0u, static_cast<unsigned int>(g.n));
}
template <typename V>
auto create_vertex_map(const legacy_graph & g) {
    return std::vector<V>(g.n);
}
template <typename V>
auto create_vertex_map(const legacy_graph & g, const V & d) {
    return std::vector<V>(g.n, d);
}

struct role_graph {
    std::size_t n;
};
inline auto vertices(const role_graph & g) {
    return std::views::iota(0u, static_cast<unsigned int>(g.n));
}
template <typename V, typename Role = melon::default_role>
auto create_vertex_map(const role_graph & g) {
    return role_map<unsigned int, V, Role>(g.n);
}
template <typename V, typename Role = melon::default_role>
auto create_vertex_map(const role_graph & g, const V & d) {
    return role_map<unsigned int, V, Role>(g.n, d);
}
}  // namespace adl_lib

template <typename Map>
using role_of = typename Map::role;

////////////////////////////////////////////////////////////////////////////////
// a legacy factory -- one template parameter, member or ADL -- answers every
// role with its standard map, so no container and no user graph changes
////////////////////////////////////////////////////////////////////////////////

static_assert(has_vertex_map<static_digraph, int, role_a>);
static_assert(has_arc_map<static_digraph, int, role_a>);
static_assert(std::same_as<vertex_map_t<static_digraph, int, role_a>,
                           vertex_map_t<static_digraph, int>>);
static_assert(std::same_as<arc_map_t<static_digraph, int, role_a>,
                           arc_map_t<static_digraph, int>>);
static_assert(std::same_as<vertex_map_t<mutable_digraph, int, role_a>,
                           vertex_map_t<mutable_digraph, int>>);
static_assert(has_vertex_map<adl_lib::legacy_graph, int, role_a>);
static_assert(std::same_as<vertex_map_t<adl_lib::legacy_graph, int, role_a>,
                           std::vector<int>>);

////////////////////////////////////////////////////////////////////////////////
// a role-aware factory receives the role, and the default role when the
// request names none
////////////////////////////////////////////////////////////////////////////////

static_assert(
    std::same_as<role_of<vertex_map_t<role_aware_graph, int>>, default_role>);
static_assert(
    std::same_as<role_of<vertex_map_t<role_aware_graph, int, role_a>>, role_a>);
static_assert(
    std::same_as<role_of<arc_map_t<role_aware_graph, int, role_b>>, role_b>);
static_assert(std::same_as<
              role_of<vertex_map_t<adl_lib::role_graph, int, role_a>>, role_a>);
static_assert(std::same_as<role_of<vertex_map_t<adl_lib::role_graph, int>>,
                           default_role>);

////////////////////////////////////////////////////////////////////////////////
// every forwarding layer carries the role: graph_ref_view / graph_owning_view
// (what views::graph_all wraps every algorithm's graph in), reverse, subgraph,
// undirect, and the undirected ref view
////////////////////////////////////////////////////////////////////////////////

using RG = graph_ref_view<role_aware_graph>;
static_assert(std::same_as<role_of<vertex_map_t<RG, int, role_a>>, role_a>);
static_assert(std::same_as<role_of<arc_map_t<RG, int, role_a>>, role_a>);
static_assert(
    std::same_as<
        role_of<vertex_map_t<graph_owning_view<role_aware_graph>, int, role_a>>,
        role_a>);
static_assert(
    std::same_as<role_of<vertex_map_t<reverse_view<RG>, int, role_a>>, role_a>);
static_assert(
    std::same_as<role_of<arc_map_t<reverse_view<RG>, int, role_b>>, role_b>);
using SG = subgraph_view<RG, maps::true_map, maps::true_map>;
static_assert(std::same_as<role_of<vertex_map_t<SG, int, role_a>>, role_a>);
static_assert(std::same_as<role_of<arc_map_t<SG, int, role_a>>, role_a>);
using UG = undirect_view<RG>;
static_assert(std::same_as<role_of<vertex_map_t<UG, int, role_a>>, role_a>);
static_assert(std::same_as<role_of<edge_map_t<UG, int, role_b>>, role_b>);
using URG = undirected_graph_ref_view<UG>;
static_assert(std::same_as<role_of<vertex_map_t<URG, int, role_a>>, role_a>);
static_assert(std::same_as<role_of<edge_map_t<URG, int, role_b>>, role_b>);
static_assert(
    std::same_as<
        role_of<vertex_map_t<undirected_graph_owning_view<UG>, int, role_a>>,
        role_a>);

// a role never narrows satisfiability
static_assert(has_vertex_map<RG> && has_vertex_map<RG, int, role_a>);
static_assert(has_edge_map<UG> && has_edge_map<UG, int, role_a>);

////////////////////////////////////////////////////////////////////////////////
// the gated map holders forward their fourth parameter as the role
////////////////////////////////////////////////////////////////////////////////

static_assert(
    std::same_as<
        role_of<decltype(detail::vertex_map_if<true, RG, int, role_a>::_map)>,
        role_a>);
static_assert(
    std::same_as<
        role_of<decltype(detail::arc_map_if<true, RG, int, role_b>::_map)>,
        role_b>);
static_assert(
    std::same_as<role_of<decltype(detail::vertex_map_if<true, RG, int>::_map)>,
                 default_role>);

////////////////////////////////////////////////////////////////////////////////
// the heap guard: a heap publishing index_map_type must name exactly the map
// the algorithm creates; a heap without the alias is not checked
////////////////////////////////////////////////////////////////////////////////

namespace {
using entry = std::pair<unsigned int, int>;
using index_map = static_map<unsigned int, std::size_t>;
using indexed_heap = updatable_d_ary_heap<2, entry, std::less<int>, index_map,
                                          maps::element<1>, maps::element<0>>;
struct aliasless_heap {};
}  // namespace
static_assert(std::same_as<indexed_heap::index_map_type, index_map>);
static_assert(heap_index_map_agrees<indexed_heap, index_map>);
static_assert(!heap_index_map_agrees<indexed_heap, std::vector<std::size_t>>);
static_assert(heap_index_map_agrees<aliasless_heap, index_map>);
static_assert(heap_index_map_agrees<aliasless_heap, std::vector<std::size_t>>);

////////////////////////////////////////////////////////////////////////////////
// the role reaches the factory at run time through a stacked view, and the
// default-value form still honours its argument
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(map_roles, role_reaches_the_factory_through_views) {
    static_digraph_builder<static_digraph, int> builder(3);
    builder.add_arc({0u, 1u}, 1).add_arc({1u, 2u}, 1);
    auto [graph, unused] = builder.build();
    role_aware_graph rg{{}, &graph};
    auto stacked = views::reverse(views::subgraph(rg));

    auto m = create_vertex_map<int, role_a>(stacked, 7);
    static_assert(std::same_as<role_of<decltype(m)>, role_a>);
    for(auto && v : vertices(graph)) ASSERT_EQ(m[v], 7);

    auto am = create_arc_map<char, role_b>(stacked);
    static_assert(std::same_as<role_of<decltype(am)>, role_b>);
    ASSERT_EQ(am.size(), num_arcs(graph));

    auto legacy = create_vertex_map<int, role_a>(graph, 3);
    static_assert(
        std::same_as<decltype(legacy), vertex_map_t<static_digraph, int>>);
    for(auto && v : vertices(graph)) ASSERT_EQ(legacy[v], 3);
}
