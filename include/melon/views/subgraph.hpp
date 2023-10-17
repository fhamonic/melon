#ifndef MELON_VIEWS_SUBGRAPH_HPP
#define MELON_VIEWS_SUBGRAPH_HPP

#include <algorithm>
#include <ranges>

#include "melon/graph.hpp"
#include "melon/utility/value_map.hpp"

namespace fhamonic {
namespace melon {
namespace views {

template <graph G, input_value_map<vertex_t<G>> VF = true_map,
          input_value_map<arc_t<G>> AF = true_map>
    requires std::convertible_to<mapped_value_t<VF, vertex_t<G>>, bool> &&
             std::convertible_to<mapped_value_t<AF, arc_t<G>>, bool>
class subgraph {
public:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;

private:
    std::reference_wrapper<const G> _graph;
    VF _vertex_filter;
    AF _arc_filter;

public:
    subgraph(const G & g, VF && vertex_filter = true_map{},
             AF && arc_filter = true_map{})
        : _graph(g)
        , _vertex_filter(std::forward<VF>(vertex_filter))
        , _arc_filter(std::forward<AF>(arc_filter)) {}

    subgraph(const subgraph &) = default;
    subgraph(subgraph &&) = default;

    subgraph & operator=(const subgraph &) = default;
    subgraph & operator=(subgraph &&) = default;

    auto nb_vertices() const noexcept
        requires has_nb_vertices<G> && std::same_as<VF, true_map>
    {
        return melon::nb_vertices(_graph.get());
    }

    void disable_vertex(const vertex & v) noexcept
        requires output_value_map_of<VF, vertex_t<G>, bool>
    {
        _vertex_filter[v] = false;
    }
    void enable_vertex(const vertex & v) noexcept
        requires output_value_map_of<VF, vertex_t<G>, bool>
    {
        _vertex_filter[v] = true;
    }
    bool is_valid_vertex(const vertex & v) const noexcept {
        return _vertex_filter[v] && melon::is_valid_vertex(_graph.get(), v);
    }

    void disable_arc(const arc & a) const noexcept
        requires output_value_map_of<AF, arc_t<G>, bool>
    {
        _arc_filter[a] = false;
    }
    void enable_arc(const arc & a) const noexcept
        requires output_value_map_of<AF, arc_t<G>, bool>
    {
        _arc_filter[a] = true;
    }
    bool is_valid_arc(const arc & a) const noexcept {
        return _arc_filter[a] && melon::is_valid_arc(_graph.get(), a);
    }

    auto vertices() const noexcept {
        if constexpr(std::same_as<VF, true_map>) {
            return melon::vertices(_graph.get());

        } else {
            return std::views::filter(
                melon::vertices(_graph.get()),
                [this](const vertex & v) { return _vertex_filter[v]; });
        }
    }
    auto arcs() const noexcept
        requires std::same_as<VF, true_map>  // if false, use the indicidence
                                             // join hierarchy of melon::arcs(g)
    {
        if constexpr(std::same_as<AF, true_map>) {
            return melon::arcs(_graph.get());
        } else {
            return std::views::filter(
                melon::arcs(_graph.get()),
                [this](const arc & a) { return _arc_filter[a]; });
        }
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
        if constexpr(std::same_as<VF, true_map> && std::same_as<AF, true_map>) {
            return melon::in_arcs(_graph.get(), v);
        } else if constexpr(std::same_as<VF, true_map>) {
            return std::views::filter(
                melon::in_arcs(_graph.get(), v),
                [this](const arc & a) { return _arc_filter[a]; });
        } else {
            return std::views::filter(
                melon::in_arcs(_graph.get(), v), [this](const arc & a) {
                    return _vertex_filter[melon::arc_source(_graph.get(), a)] &&
                           _arc_filter[a];
                });
        }
    }
    auto out_arcs(const vertex & v) const noexcept
        requires outward_incidence_graph<G>
    {
        assert(is_valid_vertex(v));
        if constexpr(std::same_as<VF, true_map> && std::same_as<AF, true_map>) {
            return melon::out_arcs(_graph.get(), v);
        } else if constexpr(std::same_as<VF, true_map>) {
            return std::views::filter(
                melon::out_arcs(_graph.get(), v),
                [this](const arc & a) { return _arc_filter[a]; });
        } else {
            return std::views::filter(
                melon::out_arcs(_graph.get(), v), [this](const arc & a) {
                    return _vertex_filter[melon::arc_target(_graph.get(), a)] &&
                           _arc_filter[a];
                });
        }
    }

    auto in_neighbors(const vertex & v) const noexcept
        requires inward_adjacency_graph<G>
    {
        assert(is_valid_vertex(v));
        if constexpr(std::same_as<VF, true_map> && std::same_as<AF, true_map>) {
            return melon::in_neighbors(_graph.get(), v);
        } else if constexpr(std::same_as<AF, true_map>) {
            return std::views::filter(
                melon::in_neighbors(_graph.get(), v),
                [this](const vertex & u) { return _vertex_filter[u]; });
        } else {
            return std::views::filter(
                std::views::transform(
                    in_arcs(v),
                    [&](const arc & a) -> vertex { return arc_source(a); }),
                [&](const vertex & u) -> bool { return _vertex_filter[u]; });
        }
    }
    auto out_neighbors(const vertex & v) const noexcept
        requires outward_adjacency_graph<G>
    {
        assert(is_valid_vertex(v));
        if constexpr(std::same_as<VF, true_map> && std::same_as<AF, true_map>) {
            return melon::out_neighbors(_graph.get(), v);
        } else if constexpr(std::same_as<AF, true_map>) {
            return std::views::filter(
                melon::out_neighbors(_graph.get(), v),
                [&](const vertex & u) { return _vertex_filter[u]; });
        } else {
            return std::views::filter(
                std::views::transform(
                    out_arcs(v),
                    [&](const arc & a) -> vertex { return arc_target(a); }),
                [&](const vertex & u) -> bool { return _vertex_filter[u]; });
        }
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