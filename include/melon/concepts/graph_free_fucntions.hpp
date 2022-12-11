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
[[nodiscard]] constexpr decltype(auto) vertices(const G & g) noexcept {
    if constexpr(requires() { g.vertices(); }) {
        return g.vertices();
    }
}

template <typename G>
using vertices_range_t = decltype(vertices(std::declval<G &&>()));

namespace concepts {
template <typename G>
concept has_vertices = std::ranges::input_range<vertices_range_t<G>>;
}  // namespace concepts

template <concepts::has_vertices G>
using vertex_t = std::ranges::range_value_t<vertices_range_t<G>>;

template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) out_arcs(
    const G & g, const vertex_t<G> & v) noexcept {
    if constexpr(requires() { g.out_arcs(v); }) {
        return g.out_arcs(v);
    }
}

template <concepts::has_vertices G>
using out_arcs_range_t =
    decltype(out_arcs(std::declval<G &&>(), std::declval<vertex_t<G>>()));

template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) in_arcs(const G & g,
                                               const vertex_t<G> & v) noexcept {
    if constexpr(requires() { g.in_arcs(v); }) {
        return g.in_arcs(v);
    }
}

template <typename G>
using in_arcs_range_t =
    decltype(in_arcs(std::declval<G &&>(), std::declval<vertex_t<G>>()));

namespace concepts {
template <has_vertices G>
concept has_out_arcs = std::ranges::input_range<out_arcs_range_t<G>>;

template <has_vertices G>
concept has_in_arcs = std::ranges::input_range<in_arcs_range_t<G>>;
}  // namespace concepts

template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) arcs(const G & g) noexcept {
    if constexpr(requires() { g.arcs(); }) {
        return g.arcs();
    } else if constexpr(concepts::has_out_arcs<G>) {
        return std::views::join(std::views::transform(
            vertices(g), [this](auto v) { return out_arcs(g, v); }));
    } else if constexpr(concepts::has_in_arcs<G>) {
        return std::views::join(std::views::transform(
            vertices(g), [this](auto v) { return in_arcs(g, v); }));
    }
}

template <typename G>
using arcs_range_t = decltype(arcs(std::declval<G &&>()));

namespace concepts {
template <typename G>
concept has_arcs = requires(G g) {
    { arcs(g) } -> std::ranges::input_range;
};
}  // namespace concepts

template <typename G>
using arc_t = std::ranges::range_value_t<arcs_range_t<G>>;

template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) source(const G & g,
                                              const arc_t<G> & a) noexcept {
    if constexpr(requires() {
                     { g.source(a) } -> std::same_as<vertex_t<G>>;
                 }) {
        return g.source(a);
    }
}

template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) sources_map(
    const G & g, const arc_t<G> & a) noexcept {
    if constexpr(requires() {
                     { g.sources_map(a) } -> input_map_of<vertex_t<G>>;
                 }) {
        return g.sources_map(a);
    } else {
        return map_view([](const vertex_t<G> & v) { return source(g, v); });
    }
}

template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) target(const G & g,
                                              const arc_t<G> & a) noexcept {
    if constexpr(requires() {
                     { g.target(a) } -> std::same_as<vertex_t<G>>;
                 }) {
        return g.target(a);
    }
}

template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) targets_map(
    const G & g, const arc_t<G> & a) noexcept {
    if constexpr(requires() {
                     { g.targets_map(a) } -> input_map_of<vertex_t<G>>;
                 }) {
        return g.targets_map(a);
    } else {
        return map_view([](const vertex_t<G> & v) { return target(g, v); });
    }
}

namespace concepts {
template <typename G>
concept has_arc_source = requires(G g, arc_t<G> a) {
    { source(g, a) } -> std::same_as<vertex_t<G>>;
    { sources_map(g) } -> input_map_of<arc_t<G>, vertex_t<G>>;
};

template <typename G>
concept has_arc_target = requires(G g, arc_t<G> a) {
    { target(g, a) } -> std::same_as<vertex_t<G>>;
    { targets_map(g) } -> input_map_of<arc_t<G>, vertex_t<G>>;
};
}  // namespace concepts

template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) arcs_entries(
    const G & g, const arc_t<G> & a) noexcept {
    if constexpr(requires() { g.arcs_entries(); }) {
        return g.arcs_entries();
    }
}

namespace concepts {
template <typename G>
concept graph = concepts::has_vertices<G> && concepts::has_arcs<G> &&
    requires(G g) {
    {
        arcs_entries(g)
        } -> input_range_of<
            std::pair<arc_t<G>, std::pair<vertex_t<G>, vertex_t<G>>>>;
};
}  // namespace concepts

// clang-format off
namespace concepts {
template <typename G>
concept graph = std::copyable<G> &&
requires(G g) {
    { g.vertices() } -> std::ranges::input_range;
    { g.arcs() } -> std::ranges::input_range;
    { g.arc_entries() } -> input_range_of<std::pair<arc_t<G>,std::pair<vertex_t<G>, vertex_t<G>>>>;
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
concept outward_incidence_list = graph<G> &&
requires(G g, vertex_t<G> u) {
    { g.out_arcs(u) } -> input_range_of<arc_t<G>>;
};

template <typename G>
concept inward_incidence_list = graph<G> &&
requires(G g, vertex_t<G> u) {
    { g.in_arcs(u) } -> input_range_of<arc_t<G>>;
};

template <typename G>
concept forward_incidence_graph = outward_incidence_list<G> && has_arc_target<G>;

template <typename G>
concept reverse_incidence_graph = inward_incidence_list<G> && has_arc_source<G>;

template <typename G>
concept outward_adjacency_list = graph<G> && requires(G g, vertex_t<G> u) {
    { g.out_neighbors(u) } -> input_range_of<vertex_t<G>>;
};

template <typename G>
concept inward_adjacency_list = graph<G> && 
requires(G g, vertex_t<G> u) {
    { g.in_neighbors(u) } -> input_range_of<vertex_t<G>>;
};

template <typename G>
concept adjacency_list = outward_adjacency_list<G> && inward_adjacency_list<G>;
}  // namespace concepts
// clang-format on

// clang-format off
namespace concepts {
template <typename G, typename T = std::size_t>
concept has_vertex_map = requires(G g, arc_t<G> a, T v) {
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
requires concepts::has_vertex_map<G, T>
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