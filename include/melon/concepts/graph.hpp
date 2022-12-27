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
[[nodiscard]] constexpr decltype(auto) vertices(const G &) noexcept;
template <typename G>
using vertices_range_t = decltype(vertices(std::declval<G &&>()));

namespace concepts {
template <typename G>
concept has_vertices = std::ranges::input_range<vertices_range_t<G>>;
}  // namespace concepts

template <concepts::has_vertices G>
using vertex_t = std::ranges::range_value_t<vertices_range_t<G>>;

template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) arcs(const G &) noexcept;
template <concepts::has_vertices G>
using arcs_range_t = decltype(arcs(std::declval<G &&>()));

namespace concepts {
template <typename G>
concept has_arcs = requires(G g) {
    { arcs(g) } -> std::ranges::input_range;
};
}  // namespace concepts

template <concepts::has_arcs G>
using arc_t = std::ranges::range_value_t<arcs_range_t<G>>;

template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) arcs_entries(const G & g) noexcept;
template <concepts::has_vertices G>
using arcs_entries_range_t = decltype(arcs_entries(std::declval<G &&>()));

namespace concepts {
template <typename G>
concept graph = concepts::has_vertices<G> && concepts::has_arcs<G> &&
    std::same_as<std::ranges::range_value_t<arcs_entries_range_t<G>>,
                 std::pair<arc_t<G>, std::pair<vertex_t<G>, vertex_t<G>>>>;

}  // namespace concepts

template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) out_arcs(const G &,
                                                const vertex_t<G> &) noexcept;
template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) in_arcs(const G &,
                                               const vertex_t<G> &) noexcept;

template <concepts::has_vertices G>
using out_arcs_range_t =
    decltype(out_arcs(std::declval<G &&>(), std::declval<vertex_t<G>>()));
template <concepts::has_vertices G>
using in_arcs_range_t =
    decltype(in_arcs(std::declval<G &&>(), std::declval<vertex_t<G>>()));

namespace concepts {
template <typename G>
concept has_out_arcs = std::ranges::input_range<out_arcs_range_t<G>> &&
    std::same_as<std::ranges::range_value_t<out_arcs_range_t<G>>, arc_t<G>>;

template <typename G>
concept has_in_arcs = std::ranges::input_range<in_arcs_range_t<G>> &&
    std::same_as<std::ranges::range_value_t<in_arcs_range_t<G>>, arc_t<G>>;
}  // namespace concepts

template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) source(const G &,
                                              const arc_t<G> &) noexcept;
template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) sources_map(const G &) noexcept;
template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) target(const G &,
                                              const arc_t<G> &) noexcept;
template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) targets_map(const G &) noexcept;

namespace concepts {
template <typename G>
concept has_arc_source = requires(G g, arc_t<G> a) {
    { source(g, a) } -> std::same_as<vertex_t<G>>;
    { sources_map(g) } -> concepts::input_map_of<arc_t<G>, vertex_t<G>>;
};
template <typename G>
concept has_arc_target = requires(G g, arc_t<G> a) {
    { target(g, a) } -> std::same_as<vertex_t<G>>;
    { targets_map(g) } -> concepts::input_map_of<arc_t<G>, vertex_t<G>>;
};
}  // namespace concepts

template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) in_neighbors(
    const G &, const vertex_t<G> &) noexcept;
template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) out_neighbors(
    const G &, const vertex_t<G> &) noexcept;

template <concepts::has_vertices G>
using out_neighbors_range_t =
    decltype(out_neighbors(std::declval<G &&>(), std::declval<vertex_t<G>>()));

template <concepts::has_vertices G>
using in_neighbors_range_t =
    decltype(in_neighbors(std::declval<G &&>(), std::declval<vertex_t<G>>()));

namespace concepts {
template <typename G>
concept has_out_neighbors =
    std::ranges::input_range<out_neighbors_range_t<G>> &&
    std::same_as<std::ranges::range_value_t<out_neighbors_range_t<G>>,
                 vertex_t<G>>;

template <typename G>
concept has_in_neighbors = std::ranges::input_range<in_neighbors_range_t<G>> &&
    std::same_as<std::ranges::range_value_t<in_neighbors_range_t<G>>,
                 vertex_t<G>>;
}  // namespace concepts

namespace concepts {
template <typename G>
concept outward_incidence_graph =
    graph<G> && has_out_arcs<G> && has_arc_target<G>;

template <typename G>
concept inward_incidence_graph =
    graph<G> && has_in_arcs<G> && has_arc_source<G>;

template <typename G>
concept outward_adjacency_graph = graph<G> && has_out_neighbors<G>;

template <typename G>
concept inward_adjacency_graph = graph<G> && has_in_neighbors<G>;
}  // namespace concepts

template <typename T, concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) create_vertex_map(const G &) noexcept;
template <typename T, concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) create_vertex_map(const G &,
                                                         const T &) noexcept;

template <typename T, concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) create_arc_map(const G &) noexcept;
template <typename T, concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) create_arc_map(const G &,
                                                      const T &) noexcept;

template <typename G, typename T = std::size_t>
using vertex_map_t = decltype(create_vertex_map<T>(std::declval<G &&>()));

template <typename G, typename T = std::size_t>
using arc_map_t = decltype(create_arc_map<T>(std::declval<G &&>()));

// clang-format off
namespace concepts {
template <typename G, typename T = std::size_t>
concept has_vertex_map = concepts::output_map_of<vertex_map_t<G>, vertex_t<G>, T> && requires(G g, T default_value) {
{ create_vertex_map<T>(g, default_value) } -> std::same_as<vertex_map_t<G>>;
};

template <typename G, typename T = std::size_t>
concept has_arc_map = concepts::output_map_of<arc_map_t<G>, arc_t<G>, T> && requires(G g, T default_value) {
{ create_arc_map<T>(g, default_value) } -> std::same_as<arc_map_t<G>>;
};
}  // namespace concepts
// clang-format on

template <concepts::has_vertices G>
[[nodiscard]] constexpr std::size_t nb_vertices(const G &) noexcept;
template <concepts::has_arcs G>
[[nodiscard]] constexpr std::size_t nb_arcs(const G &) noexcept;

template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) create_vertex(G &) noexcept;
template <concepts::has_vertices G>
constexpr void remove_vertex(G &, const vertex_t<G> &) noexcept;
template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) is_valid_vertex(
    const G &, const vertex_t<G> &) noexcept;

template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) create_arc(G &, const vertex_t<G> &,
                                                  const vertex_t<G> &) noexcept;
template <concepts::has_arcs G>
constexpr void remove_arc(G &, const arc_t<G> &) noexcept;
template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) is_valid_arc(const G &,
                                                    const arc_t<G> &) noexcept;

template <concepts::has_arcs G>
constexpr void change_arc_source(G &, const arc_t<G> &,
                                 const vertex_t<G> &) noexcept;
template <concepts::has_arcs G>
constexpr void change_arc_target(G &, const arc_t<G> &,
                                 const vertex_t<G> &) noexcept;

// clang-format off
namespace concepts {
template <typename G>
concept has_vertex_creation = graph<G> && requires(G g) {
    { create_vertex(g) } -> std::same_as<vertex_t<G>>;
};
template <typename G>
concept has_vertex_removal = graph<G> && requires(G g, vertex_t<G> v) {
    remove_vertex(g,v);
    { is_valid_vertex(g,v) } -> std::convertible_to<bool>;
};

template <typename G>
concept has_arc_creation = graph<G> && requires(G g, vertex_t<G> v) {
    { create_arc(g, v, v) } -> std::same_as<arc_t<G>>;
};
template <typename G>
concept has_arc_removal = graph<G> && requires(G g, arc_t<G> a) {
    remove_arc(g,a);
    { is_valid_arc(g,a) } -> std::convertible_to<bool>;
};

template <typename G>
concept has_change_arc_source = graph<G> &&
    requires(G g, arc_t<G> a, vertex_t<G> s) {
    change_arc_source(g, a, s);
};
template <typename G>
concept has_change_arc_target = graph<G> &&
    requires(G g, arc_t<G> a, vertex_t<G> t) {
    change_arc_target(g, a, t);
};
}  // namespace concepts
// clang-format on

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_GRAPH_HPP

#include "melon/concepts/graph_free_functions.hpp"