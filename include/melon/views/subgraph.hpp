#ifndef MELON_VIEWS_SUBGRAPH_HPP
#define MELON_VIEWS_SUBGRAPH_HPP

#include <algorithm>
#include <ranges>

#include "melon/graph.hpp"

namespace fhamonic {
namespace melon {
namespace views {

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
        , _vertex_filter(melon::create_vertex_map<bool>(g, true))
        , _arc_filter(melon::create_arc_map<bool>(g, true)) {}

    subgraph(const subgraph &) = default;
    subgraph(subgraph &&) = default;

    subgraph & operator=(const subgraph &) = default;
    subgraph & operator=(subgraph &&) = default;

    void disable_vertex(const vertex & v) const noexcept {
        _vertex_filter[v] = false;
    }
    void enable_vertex(const vertex & v) const noexcept {
        _vertex_filter[v] = true;
    }
    bool is_valid_vertex(const vertex & v) const noexcept {
        return _vertex_filter[v] && melon::is_valid_vertex(_graph.get(), v);
    }

    void disable_arc(const arc & a) const noexcept { _arc_filter[a] = false; }
    void enable_arc(const arc & a) const noexcept { _arc_filter[a] = true; }
    bool is_valid_arc(const arc & a) const noexcept {
        return _arc_filter[a] && melon::is_valid_arc(_graph.get(), a);
    }

    auto vertices() const noexcept {
        return std::views::filter(
            melon::vertices(_graph.get()),
            [this](const vertex & v) { return _vertex_filter[v]; });
    }
    auto arcs() const noexcept {
        return std::views::filter(
            melon::arcs(_graph.get()),
            [this](const arc & a) { return _arc_filter[a]; });
    }

    auto arc_source(const arc & a) const noexcept
        requires has_arc_source<G>
    {
        assert(is_valid_arc(a));
        return melon::arc_source(_graph.get(), a);
    }
    auto arc_sources_map() const noexcept
        requires has_arc_source<G>
    {
        return melon::arc_sources_map(_graph.get());
    }
    auto arc_target(const arc & a) const noexcept
        requires has_arc_target<G>
    {
        assert(is_valid_arc(a));
        return melon::arc_target(_graph.get(), a);
    }
    auto arc_targets_map() const noexcept
        requires has_arc_target<G>
    {
        return melon::arc_targets_map(_graph.get());
    }

    auto in_arcs(const vertex & v) const noexcept
        requires inward_incidence_graph<G>
    {
        assert(is_valid_vertex(v));
        return std::views::filter(
            melon::in_arcs(_graph.get(), v),
            [this](const arc & a) { return _arc_filter[a]; });
    }
    auto out_arcs(const vertex & v) const noexcept
        requires outward_incidence_graph<G>
    {
        assert(is_valid_vertex(v));
        return std::views::filter(
            static_cast<const out_arcs_range_t<G>>(melon::out_arcs(_graph.get(), v)),
            [&](const arc & a) { return _arc_filter[a]; });
    }

    auto in_neighbors(const vertex & v) const noexcept
        requires inward_adjacency_graph<G>
    {
        assert(is_valid_vertex(v));
        return std::views::filter(
            melon::in_neighbors(_graph.get(), v),
            [this](const vertex & u) { return _vertex_filter[u]; });
    }
    auto out_neighbors(const vertex & v) const noexcept
        requires outward_adjacency_graph<G>
    {
        assert(is_valid_vertex(v));
        return std::views::filter(
            melon::out_neighbors(_graph.get(), v),
            [this](const vertex & u) { return _vertex_filter[u]; });
    }

    template <typename T>
        requires has_vertex_map<G>
    decltype(auto) create_vertex_map() const noexcept {
        return melon::create_vertex_map<T>(_graph.get());
    }
    template <typename T>
        requires has_vertex_map<G>
    decltype(auto) create_vertex_map(T default_value) const noexcept {
        return melon::create_vertex_map<T>(_graph.get(), default_value);
    }

    template <typename T>
        requires has_arc_map<G>
    decltype(auto) create_arc_map() const noexcept {
        return melon::create_arc_map<T>(_graph.get());
    }
    template <typename T>
        requires has_arc_map<G>
    decltype(auto) create_arc_map(T default_value) const noexcept {
        return melon::create_arc_map<T>(_graph.get(), default_value);
    }
};

}  // namespace views
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_VIEWS_SUBGRAPH_HPP