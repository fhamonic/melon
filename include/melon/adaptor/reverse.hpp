#ifndef MELON_ADAPTOR_REVERSE_HPP
#define MELON_ADAPTOR_REVERSE_HPP

#include <algorithm>
#include <ranges>

#include "melon/concepts/graph.hpp"

namespace fhamonic {
namespace melon {
namespace adaptors {

template <typename G>
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

    [[nodiscard]] auto nb_vertices() const requires requires(const G & g) {
        g.nb_vertices();
    }
    { return _graph.get().nb_vertices(); }
    [[nodiscard]] auto nb_arcs() const noexcept requires requires(const G & g) {
        g.nb_arcs();
    }
    { return _graph.get().nb_arcs(); }

    [[nodiscard]] constexpr decltype(auto) vertices() const noexcept {
        return _graph.get().vertices();
    }
    [[nodiscard]] constexpr decltype(auto) arcs() const noexcept {
        return _graph.get().arcs();
    }

    [[nodiscard]] constexpr vertex source(
        arc a) const noexcept requires concepts::has_arc_target<G> {
        return _graph.get().target(a);
    }
    [[nodiscard]] constexpr vertex target(
        arc a) const noexcept requires concepts::has_arc_source<G> {
        return _graph.get().source(a);
    }

    [[nodiscard]] constexpr decltype(auto) sources_map()
        const noexcept requires concepts::has_arc_target<G> {
        return _graph.get().targets_map();
    }
    [[nodiscard]] constexpr decltype(auto) targets_map()
        const noexcept requires concepts::has_arc_source<G> {
        return _graph.get().sources_map();
    }

    [[nodiscard]] constexpr decltype(auto) out_arcs(const vertex u)
        const noexcept requires concepts::inward_incidence_graph<G> {
        return _graph.get().in_arcs(u);
    }
    [[nodiscard]] constexpr decltype(auto) in_arcs(const vertex u)
        const noexcept requires concepts::outward_incidence_graph<G> {
        return _graph.get().out_arcs(u);
    }

    [[nodiscard]] constexpr decltype(auto) out_neighbors(const vertex u)
        const noexcept requires concepts::inward_adjacency_graph<G> {
        return _graph.get().in_neighbors(u);
    }
    [[nodiscard]] constexpr decltype(auto) in_neighbors(const vertex u)
        const noexcept requires concepts::outward_adjacency_graph<G> {
        return _graph.get().out_neighbors(u);
    }

    [[nodiscard]] constexpr decltype(auto) arc_entries()
        const noexcept requires concepts::outward_adjacency_graph<G> {
        return std::views::transform(_graph.get().arc_entries(), [](auto && p) {
            return std::make_pair(
                p.first, std::make_pair(p.second.second, p.second.first));
        });
    }

    template <typename T>
    requires concepts::has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const noexcept {
        return _graph.get().template create_vertex_map<T>();
    }
    template <typename T>
    requires concepts::has_vertex_map<G>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        T default_value) const noexcept {
        return _graph.get().template create_vertex_map<T>(default_value);
    }

    template <typename T>
    requires concepts::has_arc_map<G>
    [[nodiscard]] constexpr decltype(auto) create_arc_map() const noexcept {
        return _graph.get().template create_arc_map<T>();
    }
    template <typename T>
    requires concepts::has_arc_map<G>
    [[nodiscard]] constexpr decltype(auto) create_arc_map(
        T default_value) const noexcept {
        return _graph.get().template create_arc_map<T>(default_value);
    }
};

}  // namespace adaptors
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ADAPTOR_REVERSE_HPP