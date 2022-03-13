#ifndef MELON_ADAPTOR_REVERSE_HPP
#define MELON_ADAPTOR_REVERSE_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

#include "melon/concepts/graph_concepts.hpp"
#include "melon/data_structures/static_map.hpp"

namespace fhamonic {
namespace melon {

template <typename G>
class reverse {
public:
    using vertex = typename G::vertex;
    using arc = typename G::arc;

    template <typename T>
    using vertex_map = typename G::static_map<T>;
    template <typename T>
    using arc_map = typename G::static_map<T>;

private:
    const G & _graph;

public:
    reverse(G && g) : _graph(std::forward(g)) {}

    reverse(const reverse & graph) = default;
    reverse(reverse && graph) = default;

    reverse & operator=(const reverse &) = default;
    reverse & operator=(reverse &&) = default;

    auto nb_vertices() const requires requires(const G & g) { g.nb_vertices(); }
    { return _graph.nb_vertices(); }
    auto nb_arcs() const noexcept requires requires(const G & g) {
        g.nb_arcs();
    }
    { return _graph.nb_arcs(); }

    auto vertices() const noexcept { return _graph.vertices(); }
    auto arcs() const noexcept { return _graph.arcs(); }

    vertex source(arc a) const noexcept requires concepts::has_arc_target<G> {
        return _graph.target(a);
    }
    vertex target(arc a) const noexcept requires concepts::has_arc_source<G> {
        return _graph.source(a);
    }

    auto out_arcs(const vertex u)
        const noexcept requires concepts::reversible_incidence_list_graph<G> {
        return _graph.in_arcs(u);
    }
    auto in_arcs(const vertex u)
        const noexcept requires concepts::incidence_list_graph<G> {
        return _graph.out_arcs(u);
    }

    auto out_neighbors(const vertex u)
        const noexcept requires concepts::reversible_adjacency_list_graph<G> {
        return _graph.in_neighbors(u);
    }
    auto in_neighbors(const vertex u)
        const noexcept requires concepts::adjacency_list_graph<G> {
        return _graph.out_neighbors(u);
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ADAPTOR_REVERSE_HPP