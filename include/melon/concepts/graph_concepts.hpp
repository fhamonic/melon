#ifndef MELON_CONCEPTS_GRAPH_HPP
#define MELON_CONCEPTS_GRAPH_HPP

#include <concepts>
#include <ranges>
#include <type_traits>

#include "melon/concepts/key_value_map.hpp"
#include "melon/concepts/range_of.hpp"

namespace fhamonic {
namespace melon {

template <typename G>
using graph_vertex_t = typename std::remove_reference_t<G>::vertex_t;

template <typename G>
using graph_arc_t = typename std::remove_reference_t<G>::arc_t;

// clang-format off
namespace concepts {
template <typename G>
concept graph = std::copyable<G> &&
requires(G g, graph_vertex_t<G> u, graph_arc_t<G> a) {
    { g.vertices() } -> range_of<graph_vertex_t<G>>;
    { g.arcs() } -> range_of<graph_arc_t<G>>;
    { g.arcs_pairs() } -> 
            range_of<std::pair<graph_vertex_t<G>, graph_vertex_t<G>>>;
};

template <typename G>
concept has_arc_source = requires(G g, graph_arc_t<G> a) {
    { g.source(a) } -> std::same_as<graph_vertex_t<G>>;
    { g.sources_map() } -> key_value_map_view<graph_vertex_t<G>, graph_vertex_t<G>>;
};

template <typename G>
concept has_arc_target = requires(G g, graph_arc_t<G> a) {
    { g.target(a) } -> std::same_as<graph_vertex_t<G>>;
    { g.targets_map() } -> key_value_map_view<graph_vertex_t<G>, graph_vertex_t<G>>;
};

template <typename G>
concept incidence_list_graph = graph<G> && has_arc_target<G> &&
requires(G g, graph_vertex_t<G> u) {
    { g.out_arcs(u) } -> range_of<graph_arc_t<G>>;
};

template <typename G>
concept adjacency_list_graph = graph<G> && requires(G g, graph_vertex_t<G> u) {
    { g.out_neighbors(u) } -> range_of<graph_vertex_t<G>>;
};

template <typename G>
concept reversible_incidence_list_graph = graph<G> && has_arc_source<G> &&
requires(G g, graph_vertex_t<G> u) {
    { g.in_arcs(u) } -> range_of<graph_arc_t<G>>;
};

template <typename G>
concept reversible_adjacency_list_graph = graph<G> && 
requires(G g, graph_vertex_t<G> u) {
    { g.in_neighbors(u) } -> range_of<graph_vertex_t<G>>;
};
}  // namespace concepts
// clang-format on

// clang-format off
namespace concepts {
template <typename G, typename T = std::size_t>
concept has_vertex_map = requires(G g, graph_arc_t<G> a, T v) {
    { g.template create_vertex_map<T>() } -> 
            key_value_map<graph_vertex_t<G>, T>;
    { g.template create_vertex_map<T>(v) } -> 
            key_value_map<graph_vertex_t<G>, T>;
};

template <typename G, typename T = std::size_t>
concept has_arc_map = requires(G g, graph_arc_t<G> a, T v) {
    { g.template create_arc_map<T>() } -> key_value_map<graph_arc_t<G>, T>;
    { g.template create_arc_map<T>(v) } -> key_value_map<graph_arc_t<G>, T>;
};
}  // namespace concepts
// clang-format on

template <typename G, typename T>
requires concepts::has_vertex_map<G, T>
using graph_vertex_map =
    decltype(std::declval<G>().template create_vertex_map<T>());

template <typename G, typename T>
requires concepts::has_arc_map<G, T>
using graph_arc_map = decltype(std::declval<G>().template create_arc_map<T>());

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_GRAPH_HPP