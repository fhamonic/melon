#ifndef MELON_GRAPH_HPP
#define MELON_GRAPH_HPP

#include <concepts>
#include <ranges>
#include <type_traits>

#include "melon/concepts/map_of.hpp"
#include "melon/concepts/range_of.hpp"

namespace fhamonic {
namespace melon {

template <typename G>
using vertex_t =
    std::ranges::range_value_t<decltype(std::declval<G &&>().vertices())>;

template <typename G>
using arc_t = std::ranges::range_value_t<decltype(std::declval<G &&>().arcs())>;

// clang-format off
namespace concepts {
template <typename G>
concept graph = std::copyable<G> &&
requires(G g, vertex_t<G> u, arc_t<G> a) {
    { g.vertices() } -> std::ranges::input_range;
    { g.arcs() } -> std::ranges::input_range;
    { g.arcs_pairs() } -> input_range_of<std::pair<vertex_t<G>, vertex_t<G>>>;
};

template <typename G>
concept has_arc_source = requires(G g, arc_t<G> a) {
    { g.source(a) } -> std::same_as<vertex_t<G>>;
    { g.sources_map() } -> input_map_of<arc_t<G>, vertex_t<G>>;
};

template <typename G>
concept has_arc_target = requires(G g, arc_t<G> a) {
    { g.target(a) } -> std::same_as<vertex_t<G>>;
    { g.targets_map() } -> input_map_of<arc_t<G>, vertex_t<G>>;
};

template <typename G>
concept incidence_list_graph = graph<G> && has_arc_target<G> &&
requires(G g, vertex_t<G> u) {
    { g.out_arcs(u) } -> input_range_of<arc_t<G>>;
};

template <typename G>
concept adjacency_list_graph = graph<G> && requires(G g, vertex_t<G> u) {
    { g.out_neighbors(u) } -> input_range_of<vertex_t<G>>;
};

template <typename G>
concept reversible_incidence_list_graph = graph<G> && has_arc_source<G> &&
requires(G g, vertex_t<G> u) {
    { g.in_arcs(u) } -> input_range_of<arc_t<G>>;
};

template <typename G>
concept reversible_adjacency_list_graph = graph<G> && 
requires(G g, vertex_t<G> u) {
    { g.in_neighbors(u) } -> input_range_of<vertex_t<G>>;
};
}  // namespace concepts
// clang-format on

// clang-format off
namespace concepts {
template <typename G, typename T = std::size_t>
concept has_vertices_map = requires(G g, arc_t<G> a, T v) {
    { g.template create_vertex_map<T>() } -> 
            output_map_of<vertex_t<G>, T>;
    { g.template create_vertex_map<T>(v) } -> 
            output_map_of<vertex_t<G>, T>;
};

template <typename G, typename T = std::size_t>
concept has_arc_map = requires(G g, arc_t<G> a, T v) {
    { g.template create_arc_map<T>() } -> output_map_of<arc_t<G>, T>;
    { g.template create_arc_map<T>(v) } -> output_map_of<arc_t<G>, T>;
};
}  // namespace concepts
// clang-format on

template <typename G, typename T>
requires concepts::has_vertices_map<G, T>
using vertex_map_t =
    decltype(std::declval<G &&>().template create_vertex_map<T>());

template <typename G, typename T>
requires concepts::has_arc_map<G, T>
using arc_map_t = decltype(std::declval<G &&>().template create_arc_map<T>());

// clang-format off
namespace concepts {
template <typename G>
concept has_vertex_creation = graph<G> && requires(G g) {
    { g.create_vertex() } -> std::same_as<vertex_t<G>>;
};

template <typename G>
concept has_vertex_removal = graph<G> && requires(G g, vertex_t<G> u) {
    g.remove_vertex(u);
    { g.is_valid_vertex(u) } -> std::convertible_to<bool>;
};

template <typename G>
concept has_arc_creation = graph<G> &&
    requires(G g, vertex_t<G> s, vertex_t<G> t) {
    { g.create_arc(s, t) } -> std::same_as<arc_t<G>>;
};

template <typename G>
concept has_arc_removal = graph<G> && requires(G g, arc_t<G> a) {
    g.remove_arc(a);
    { g.is_valid_arc(a) } -> std::convertible_to<bool>;
};

template <typename G>
concept has_arc_change_source = graph<G> &&
    requires(G g, arc_t<G> a, vertex_t<G> s) {
    g.change_source(a, s);
};

template <typename G>
concept has_arc_change_target = graph<G> &&
    requires(G g, arc_t<G> a, vertex_t<G> t) {
    g.change_target(a, t);
};
}  // namespace concepts
// clang-format on

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_GRAPH_HPP