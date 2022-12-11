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
class line {
public:
    using vertex = arc_t<G>;
    // using arc = int;

    template <typename T>
    using vertex_map = typename G::arc_map<T>;
    // template <typename T>
    // using arc_map = typename G::arc_map<T>;

private:
    std::reference_wrapper<const G> _graph;

public:
    line(const G & g) : _graph(g) {}

    line(const line & graph) = default;
    line(line && graph) = default;

    line & operator=(const line &) = default;
    line & operator=(line &&) = default;

    auto nb_vertices() const requires requires(const G & g) { g.nb_arcs(); }
    { return _graph.get().nb_arcs(); }
    // auto nb_arcs() const noexcept requires requires(const G & g) {
    //     g.nb_arcs();
    // }
    // { return _graph.get().nb_arcs(); }

    auto vertices() const noexcept { return _graph.get().arcs(); }
    // auto arcs() const noexcept { return _graph.get().arcs(); }

    // vertex_t source(
    //     arc_t a) const noexcept requires concepts::has_arc_target<G> {
    //     return _graph.get().target(a);
    // }
    // vertex_t target(
    //     arc_t a) const noexcept requires concepts::has_arc_source<G> {
    //     return _graph.get().source(a);
    // }

    // auto out_arcs(const vertex u)
    //     const noexcept requires concepts::inward_incidence_graph<G> {
    //     return _graph.get().in_arcs(u);
    // }
    // auto in_arcs(const vertex u)
    //     const noexcept requires concepts::outward_incidence_graph<G> {
    //     return _graph.get().out_arcs(u);
    // }

    // auto out_neighbors(const vertex u)
    //     const noexcept requires concepts::inward_adjacency_graph<G> {
    //     return _graph.get().in_neighbors(u);
    // }
    // auto in_neighbors(const vertex u)
    //     const noexcept requires concepts::outward_adjacency_graph<G> {
    //     return _graph.get().out_neighbors(u);
    // }

    // auto arc_entries()
    //     const noexcept requires concepts::outward_adjacency_graph<G> {
    //     return std::views::transform(_graph.get().arc_entries(), [](auto && p) {
    //         return std::make_pair(p.second, p.first);
    //     });
    // }
};

}  // namespace adaptors
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ADAPTOR_REVERSE_HPP