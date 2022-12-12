#ifndef MELON_ADAPTOR_REVERSE_HPP
#define MELON_ADAPTOR_REVERSE_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <ranges>
#include <vector>

#include "melon/concepts/graph.hpp"
#include "melon/data_structures/static_map.hpp"

namespace fhamonic {
namespace melon {
namespace adaptors {

template <typename G>
class snapshot {
public:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;

private:
    G _graph;

public:
    snapshot(G && g) : _graph(std::forward<G>(g)) {}

    snapshot(const snapshot & graph) = default;
    snapshot(snapshot && graph) = default;

    snapshot & operator=(const snapshot &) = default;
    snapshot & operator=(snapshot &&) = default;

    auto nb_vertices() const requires requires(const G & g) { g.nb_vertices(); }
    { return _graph.nb_vertices(); }
    auto nb_arcs() const noexcept requires requires(const G & g) {
        g.nb_arcs();
    }
    { return _graph.nb_arcs(); }

    auto vertices() const noexcept { return _graph.vertices(); }
    auto arcs() const noexcept { return _graph.arcs(); }
    auto arcs_entries() const noexcept {
        return std::views::transform(_graph.arcs_entries(), [](auto && p) {
            return std::make_pair(
                p.first, std::make_pair(p.second.first, p.second.second));
        });
    }

    auto source(
        const arc & a) const noexcept requires concepts::has_arc_target<G> {
        return _graph.target(a);
    }
    auto sources_map() const noexcept requires concepts::has_arc_target<G> {
        return _graph.targets_map();
    }
    auto target(
        const arc & a) const noexcept requires concepts::has_arc_source<G> {
        return _graph.source(a);
    }
    auto targets_map() const noexcept requires concepts::has_arc_source<G> {
        return _graph.sources_map();
    }

    auto out_arcs(const vertex & u)
        const noexcept requires concepts::inward_incidence_graph<G> {
        return _graph.in_arcs(u);
    }
    auto in_arcs(const vertex & u)
        const noexcept requires concepts::outward_incidence_graph<G> {
        return _graph.out_arcs(u);
    }

    auto out_neighbors(const vertex & u)
        const noexcept requires concepts::inward_adjacency_graph<G> {
        return _graph.in_neighbors(u);
    }
    auto in_neighbors(const vertex & u)
        const noexcept requires concepts::outward_adjacency_graph<G> {
        return _graph.out_neighbors(u);
    }

    template <typename T>
    requires concepts::has_vertex_map<G>
    decltype(auto) create_vertex_map() const noexcept {
        return _graph.template create_vertex_map<T>();
    }
    template <typename T>
    requires concepts::has_vertex_map<G>
    decltype(auto) create_vertex_map(T default_value) const noexcept {
        return _graph.template create_vertex_map<T>(default_value);
    }

    template <typename T>
    requires concepts::has_arc_map<G>
    decltype(auto) create_arc_map() const noexcept {
        return _graph.template create_arc_map<T>();
    }
    template <typename T>
    requires concepts::has_arc_map<G>
    decltype(auto) create_arc_map(T default_value) const noexcept {
        return _graph.template create_arc_map<T>(default_value);
    }
};

}  // namespace adaptors
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ADAPTOR_REVERSE_HPP