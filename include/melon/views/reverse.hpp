#ifndef MELON_VIEWS_REVERSE_HPP
#define MELON_VIEWS_REVERSE_HPP

#include <algorithm>
#include <ranges>

#include "melon/graph.hpp"

namespace fhamonic {
namespace melon {
namespace views {

template <graph G>
class reverse {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;

    std::reference_wrapper<const G> _graph;

public:
    [[nodiscard]] constexpr explicit reverse(const G & g) : _graph(g) {}

    [[nodiscard]] constexpr reverse(const reverse &) = default;
    [[nodiscard]] constexpr reverse(reverse &&) = default;

    constexpr reverse & operator=(const reverse &) = default;
    constexpr reverse & operator=(reverse &&) = default;

    [[nodiscard]] constexpr decltype(auto) nb_vertices() const requires
        requires(G g) {
        melon::nb_vertices(g);
    }
    { return melon::nb_vertices(_graph.get()); }
    [[nodiscard]] constexpr decltype(auto) nb_arcs() const noexcept requires
        requires(G g) {
        melon::nb_arcs(g);
    }
    { return melon::nb_arcs(_graph.get()); }

    [[nodiscard]] constexpr decltype(auto) vertices() const noexcept {
        return melon::vertices(_graph.get());
    }
    [[nodiscard]] constexpr decltype(auto) arcs() const noexcept {
        return melon::arcs(_graph.get());
    }

    [[nodiscard]] constexpr vertex arc_source(
        arc a) const noexcept requires has_arc_target<G> {
        return melon::arc_target(_graph.get(), a);
    }
    [[nodiscard]] constexpr vertex arc_target(
        arc a) const noexcept requires has_arc_source<G> {
        return melon::arc_source(_graph.get(), a);
    }

    [[nodiscard]] constexpr decltype(auto) sources_map()
        const noexcept requires has_arc_target<G> {
        return melon::arc_targets_map(_graph.get());
    }
    [[nodiscard]] constexpr decltype(auto) targets_map()
        const noexcept requires has_arc_source<G> {
        return melon::arc_sources_map(_graph.get());
    }

    [[nodiscard]] constexpr decltype(auto) out_arcs(
        const vertex u) const noexcept requires has_in_arcs<G> {
        return melon::in_arcs(_graph.get(), u);
    }
    [[nodiscard]] constexpr decltype(auto) in_arcs(
        const vertex u) const noexcept requires has_out_arcs<G> {
        return melon::out_arcs(_graph.get(), u);
    }

    [[nodiscard]] constexpr decltype(auto) out_neighbors(
        const vertex u) const noexcept requires inward_adjacency_graph<G> {
        return melon::in_neighbors(_graph.get(), u);
    }
    [[nodiscard]] constexpr decltype(auto) in_neighbors(
        const vertex u) const noexcept requires outward_adjacency_graph<G> {
        return melon::out_neighbors(_graph.get(), u);
    }

    template <typename T>
    requires has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const noexcept {
        return melon::create_vertex_map<T>(_graph.get());
    }
    template <typename T>
    requires has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        T default_value) const noexcept {
        return melon::create_vertex_map<T>(_graph.get(), default_value);
    }

    template <typename T>
    requires has_arc_map<G>
    [[nodiscard]] constexpr decltype(auto) create_arc_map() const noexcept {
        return melon::create_arc_map<T>(_graph.get());
    }
    template <typename T>
    requires has_arc_map<G>
    [[nodiscard]] constexpr decltype(auto) create_arc_map(
        T default_value) const noexcept {
        return melon::create_arc_map<T>(_graph.get(), default_value);
    }
};

}  // namespace views
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_VIEWS_REVERSE_HPP