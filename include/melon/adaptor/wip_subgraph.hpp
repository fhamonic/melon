#ifndef MELON_ADAPTOR_SUBGRAPH_HPP
#define MELON_ADAPTOR_SUBGRAPH_HPP

#include <algorithm>
#include <ranges>

#include "melon/graph.hpp"

namespace fhamonic {
namespace melon {
namespace adaptors {

template <graph G>
class subgraph {
public:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;

private:
    std::reference_wrapper<const G> _graph;
    vertex_map_t<G, bool> _vertex_filter;
    arc_map_t<G, bool> _arc_filter;

public:
    subgraph(const G & g)
        : _graph(g)
        , _vertex_filter(create_vertex_map<bool>(g,true))
        , _arc_filter(create_arc_map<bool>(g)) {}

    subgraph(const subgraph &) = default;
    subgraph(subgraph &&) = default;

    subgraph & operator=(const subgraph &) = default;
    subgraph & operator=(subgraph &&) = default;

    bool is_valid_vertex(const vertex & u) const noexcept {
        return _vertex_filter[u];
    }
    bool is_valid_arc(const arc & a) const noexcept { return _arcs_filter[a]; }

    void disable(const arc & a) const noexcept { _arcs_filter[a] = false; }
    void restore(const arc & a) const noexcept { _arcs_filter[a] = true; }
    void remove_arc(const arc & a) const noexcept { disable(a); }

    auto vertices() const noexcept {
        return std::views::filter(
            _graph.get().vertices(),
            [this](const vertex & v) { return _vertex_filter[v]; });
    }
    auto arcs() const noexcept {
        return std::views::filter(_graph.get().arcs(), [this](const arc & a) {
            return _arc_filter[a];
        });
    }

    auto arc_source(
        const arc & a) const noexcept requires has_arc_target<G> {
        assert(is_valid_arc(a));
        return _graph.get().arc_target(a);
    }
    auto arc_sources_map() const noexcept requires has_arc_target<G> {
        return _graph.get().arc_targets_map();
    }
    auto arc_target(
        const arc & a) const noexcept requires has_arc_source<G> {
        assert(is_valid_arc(a));
        return _graph.get().arc_source(a);
    }
    auto arc_targets_map() const noexcept requires has_arc_source<G> {
        return _graph.get().arc_sources_map();
    }

    auto in_arcs(const vertex & v)
        const noexcept requires outward_incidence_graph<G> {
        assert(is_valid_vertex(v));
        return std::views::filter(
            _graph.get().in_arcs(v),
            [this](const arc & a) { return _arc_filter[a]; });
    }
    auto out_arcs(const vertex & v)
        const noexcept requires inward_incidence_graph<G> {
        assert(is_valid_vertex(v));
        return std::views::filter(
            _graph.get().out_arcs(v),
            [this](const arc & a) { return _arc_filter[a]; });
    }

    auto in_neighbors(const vertex & v)
        const noexcept requires outward_adjacency_graph<G> {
        assert(is_valid_vertex(v));
        return std::views::filter(
            _graph.get().in_neighbors(v),
            [this](const vertex & v) { return _vertex_filter[v]; });
    }
    auto out_neighbors(const vertex & v)
        const noexcept requires inward_adjacency_graph<G> {
        assert(is_valid_vertex(v));
        return std::views::filter(
            _graph.get().out_neighbors(v),
            [this](const vertex & v) { return _vertex_filter[v]; });
    }

    auto arcs_entries() const noexcept {
        // if constexpr(std::same_as<arcs_range_t<G>,
        //                           std::ranges::iota_view<arc, arc>> &&
        //              (std::integral<arc> ||
        //               std::contiguous_iterator<
        //                   arc>)&&has_arc_source<G> &&
        //              has_arc_target<G>) {
        //     return std::views::transform(arcs(), [](const arcs & a) {
        //         return std::make_pair(_graph.get().arc_source(a),
        //                               _graph.get().arc_target(a));
        //     });
        // } else if constexpr(outward_adjacency_graph<G>) {
        //     return std::views::join(
        //         std::views::transform(vertices(), [this](const vertex & s) {
        //             return std::views::transform(
        //                 out_neighbors(s),
        //                 [s](const vertex & t) { return std::make_pair(s, t); });
        //         }));
        // } else if constexpr(inward_adjacency_graph<G>) {
        //     return std::views::join(
        //         std::views::transform(vertices(), [this](const vertex & t) {
        //             return std::views::transform(
        //                 in_neighbors(t),
        //                 [t](const vertex & s) { return std::make_pair(s, t); });
        //         }));
        // } else {
        //     static_assert(false, "cannot enumerate arcs_entries");
        // }
    }

    template <typename T>
    requires has_vertex_map<G>
    decltype(auto) create_vertex_map() const noexcept {
        return _graph.get().template create_vertex_map<T>();
    }
    template <typename T>
    requires has_vertex_map<G>
    decltype(auto) create_vertex_map(T default_value) const noexcept {
        return _graph.get().template create_vertex_map<T>(default_value);
    }

    template <typename T>
    requires has_arc_map<G>
    decltype(auto) create_arc_map() const noexcept {
        return _graph.get().template create_arc_map<T>();
    }
    template <typename T>
    requires has_arc_map<G>
    decltype(auto) create_arc_map(T default_value) const noexcept {
        return _graph.get().template create_arc_map<T>(default_value);
    }
};

}  // namespace adaptors
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ADAPTOR_SUBGRAPH_HPP