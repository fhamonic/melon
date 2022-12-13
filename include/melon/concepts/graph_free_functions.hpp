#ifndef MELON_GRAPH_FREE_FUNCTIONS_HPP
#define MELON_GRAPH_FREE_FUNCTIONS_HPP

#include "melon/concepts/graph.hpp"

namespace fhamonic {
namespace melon {

template <typename G>
[[nodiscard]] constexpr decltype(auto) vertices(const G & g) noexcept {
    if constexpr(requires() { g.vertices(); }) {
        return g.vertices();
    }
}
template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) out_arcs(
    const G & g, const vertex_t<G> & v) noexcept {
    if constexpr(requires() { g.out_arcs(v); }) {
        return g.out_arcs(v);
    }
}
template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) in_arcs(const G & g,
                                               const vertex_t<G> & v) noexcept {
    if constexpr(requires() { g.in_arcs(v); }) {
        return g.in_arcs(v);
    }
}
template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) arcs(const G & g) noexcept {
    if constexpr(requires() { g.arcs(); }) {
        return g.arcs();
    } else if constexpr(std::ranges::input_range<out_arcs_range_t<G>>) {
        return std::views::join(std::views::transform(
            vertices(g),
            [&](const vertex_t<G> & v) { return out_arcs(g, v); }));
    } else if constexpr(std::ranges::input_range<in_arcs_range_t<G>>) {
        return std::views::join(std::views::transform(
            vertices(g), [&](const vertex_t<G> & v) { return in_arcs(g, v); }));
    }
}
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
[[nodiscard]] constexpr decltype(auto) sources_map(const G & g) noexcept {
    if constexpr(requires() {
                     {
                         g.sources_map()
                         } -> concepts::input_map_of<arc_t<G>, vertex_t<G>>;
                 }) {
        return g.sources_map();
    } else {
        return map_view([&g](const arc_t<G> & a) { return source(g, a); });
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
[[nodiscard]] constexpr decltype(auto) targets_map(const G & g) noexcept {
    if constexpr(requires() {
                     {
                         g.targets_map()
                         } -> concepts::input_map_of<arc_t<G>, vertex_t<G>>;
                 }) {
        return g.targets_map();
    } else {
        return map_view([&g](const arc_t<G> & a) { return target(g, a); });
    }
}

template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) arcs_entries(const G & g) noexcept {
    // TODO : if there is multiple ways of listing the arcs entries, chose the
    // most optimized one, based on the range type of arcs(g), out_arcs(g,v)...
    if constexpr(requires() { g.arcs_entries(); }) {
        return g.arcs_entries();
    } else if constexpr(concepts::has_out_arcs<G> &&
                        concepts::has_arc_target<G> &&
                        std::ranges::viewable_range<out_arcs_range_t<G>>) {
        return std::views::join(
            std::views::transform(vertices(g), [&g](const vertex_t<G> & s) {
                return std::views::transform(
                    out_arcs(g, s), [&g, &s](const arc_t<G> & a) {
                        return std::make_pair(a,
                                              std::make_pair(s, target(g, a)));
                    });
            }));
    } else if constexpr(concepts::has_in_arcs<G> &&
                        concepts::has_arc_source<G> &&
                        std::ranges::viewable_range<in_arcs_range_t<G>>) {
        return std::views::join(
            std::views::transform(vertices(g), [&g](const vertex_t<G> & t) {
                return std::views::transform(
                    in_arcs(g, t), [&g, &t](const arc_t<G> & a) {
                        return std::make_pair(a,
                                              std::make_pair(source(g, a), t));
                    });
            }));
    } else if constexpr(concepts::has_arcs<G> && concepts::has_arc_source<G> &&
                        concepts::has_arc_target<G> &&
                        std::ranges::viewable_range<arcs_range_t<G>>) {
        return std::views::transform(arcs(g), [&g](const arc_t<G> & a) {
            return std::make_pair(a,
                                  std::make_pair(source(g, a), target(g, a)));
        });
    }
}

template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) out_neighbors(
    const G & g, const vertex_t<G> & s) noexcept {
    if constexpr(requires() { g.out_neighbors(s); }) {
        return g.out_neighbors(s);
    } else if constexpr(concepts::has_out_arcs<G> &&
                        concepts::has_arc_target<G> &&
                        std::ranges::viewable_range<out_arcs_range_t<G>>) {
        return std::views::transform(
            out_arcs(g, s), [&g](const arc_t<G> & a) { return target(g, a); });
    }
}

template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) in_neighbors(
    const G & g, const vertex_t<G> & t) noexcept {
    if constexpr(requires() { g.in_neighbors(t); }) {
        return g.in_neighbors(t);
    } else if constexpr(concepts::has_in_arcs<G> &&
                        concepts::has_arc_target<G> &&
                        std::ranges::viewable_range<in_arcs_range_t<G>>) {
        return std::views::transform(
            in_arcs(g, t), [&g](const arc_t<G> & a) { return target(g, a); });
    }
}

template <typename T, concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) create_vertex_map(const G & g) noexcept {
    if constexpr(requires() { g.template create_vertex_map<T>(); }) {
        return g.template create_vertex_map<T>();
    }
}
template <typename T, concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) create_vertex_map(
    const G & g, const T & default_value) noexcept {
    if constexpr(requires() {
                     g.template create_vertex_map<T>(default_value);
                 }) {
        return g.template create_vertex_map<T>(default_value);
    }
}

template <typename T, concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) create_arc_map(const G & g) noexcept {
    if constexpr(requires() { g.template create_arc_map<T>(); }) {
        return g.template create_arc_map<T>();
    }
}
template <typename T, concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) create_arc_map(
    const G & g, const T & default_value) noexcept {
    if constexpr(requires() { g.template create_arc_map<T>(default_value); }) {
        return g.template create_arc_map<T>(default_value);
    }
}

template <concepts::has_vertices G>
[[nodiscard]] constexpr std::size_t nb_vertices(const G & g) noexcept {
    if constexpr(requires() { g.nb_vertices(); }) {
        return g.nb_vertices();
    } else {
        return std::ranges::distance(vertices(g));
    }
}
template <concepts::has_arcs G>
[[nodiscard]] constexpr std::size_t nb_arcs(const G & g) noexcept {
    if constexpr(requires() { g.nb_arcs(); }) {
        return g.nb_arcs();
    } else {
        return std::ranges::distance(arcs(g));
    }
}

template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) create_vertex(const G & g) noexcept {
    if constexpr(requires() { g.create_vertex(); }) {
        return g.create_vertex();
    }
}
template <concepts::has_vertices G>
constexpr void remove_vertex(const G & g, const vertex_t<G> & v) noexcept {
    if constexpr(requires() { g.remove_vertex(v); }) {
        g.remove_vertex(v);
        return;
    }
}
template <concepts::has_vertices G>
[[nodiscard]] constexpr decltype(auto) is_valid_vertex(
    const G & g, const vertex_t<G> & v) noexcept {
    if constexpr(requires() { g.is_valid_vertex(v); }) {
        return g.is_valid_vertex(v);
    }
}

template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) create_arc(
    const G & g, const vertex_t<G> & s, const vertex_t<G> & t) noexcept {
    if constexpr(requires() { g.create_arc(s, t); }) {
        return g.create_arc(s, t);
    }
}
template <concepts::has_arcs G>
constexpr void remove_arc(const G & g, const arc_t<G> & a) noexcept {
    if constexpr(requires() { g.remove_arc(a); }) {
        g.remove_arc(a);
        return;
    }
}
template <concepts::has_arcs G>
[[nodiscard]] constexpr decltype(auto) is_valid_arc(
    const G & g, const arc_t<G> & a) noexcept {
    if constexpr(requires() { g.is_valid_arc(a); }) {
        return g.is_valid_arc(a);
    }
}

template <concepts::has_arcs G>
constexpr void change_arc_source(const G & g, const arc_t<G> & a,
                                 const vertex_t<G> & s) noexcept {
    if constexpr(requires() { g.change_arc_source(a, s); }) {
        g.change_arc_source(a, s);
        return;
    }
}
template <concepts::has_arcs G>
constexpr void change_arc_target(const G & g, const arc_t<G> & a,
                                 const vertex_t<G> & t) noexcept {
    if constexpr(requires() { g.change_arc_target(a, t); }) {
        g.change_arc_target(a, t);
        return;
    }
}

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_GRAPH_FREE_FUNCTIONS_HPP