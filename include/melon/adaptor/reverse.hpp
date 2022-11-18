#ifndef MELON_ADAPTOR_REVERSE_HPP
#define MELON_ADAPTOR_REVERSE_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <ranges>
#include <vector>

#include "melon/concepts/graph_concepts.hpp"
#include "melon/data_structures/static_map.hpp"

namespace fhamonic {
namespace melon {
namespace adaptors {

template <typename G>
class reverse {
public:
    using vertex_t = typename G::vertex_t;
    using arc_t = typename G::arc_t;

    template <typename T>
    using vertex_map = typename G::vertex_map<T>;
    template <typename T>
    using arc_map = typename G::arc_map<T>;

private:
    std::reference_wrapper<const G> _graph;

public:
    reverse(const G & g) : _graph(g) {}

    reverse(const reverse & graph) = default;
    reverse(reverse && graph) = default;

    reverse & operator=(const reverse &) = default;
    reverse & operator=(reverse &&) = default;

    auto nb_vertices() const requires requires(const G & g) { g.nb_vertices(); }
    { return _graph.get().nb_vertices(); }
    auto nb_arcs() const noexcept requires requires(const G & g) {
        g.nb_arcs();
    }
    { return _graph.get().nb_arcs(); }

    auto vertices() const noexcept { return _graph.get().vertices(); }
    auto arcs() const noexcept { return _graph.get().arcs(); }

    vertex_t source(
        arc_t a) const noexcept requires concepts::has_arc_target<G> {
        return _graph.get().target(a);
    }
    vertex_t target(
        arc_t a) const noexcept requires concepts::has_arc_source<G> {
        return _graph.get().source(a);
    }

    auto sources_map() const noexcept requires concepts::has_arc_target<G> {
        return _graph.get().targets_map();
    }
    auto targets_map() const noexcept requires concepts::has_arc_source<G> {
        return _graph.get().sources_map();
    }

    auto out_arcs(const vertex_t u)
        const noexcept requires concepts::reversible_incidence_list_graph<G> {
        return _graph.get().in_arcs(u);
    }
    auto in_arcs(const vertex_t u)
        const noexcept requires concepts::incidence_list_graph<G> {
        return _graph.get().out_arcs(u);
    }

    auto out_neighbors(const vertex_t u)
        const noexcept requires concepts::reversible_adjacency_list_graph<G> {
        return _graph.get().in_neighbors(u);
    }
    auto in_neighbors(const vertex_t u)
        const noexcept requires concepts::adjacency_list_graph<G> {
        return _graph.get().out_neighbors(u);
    }

    auto arcs_pairs()
        const noexcept requires concepts::adjacency_list_graph<G> {
        return std::views::transform(_graph.get().arcs_pairs(), [](auto && p) {
            return std::make_pair(p.second, p.first);
        });
    }
};

}  // namespace adaptors
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ADAPTOR_REVERSE_HPP