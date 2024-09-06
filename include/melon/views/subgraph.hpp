#ifndef MELON_VIEWS_SUBGRAPH_HPP
#define MELON_VIEWS_SUBGRAPH_HPP

#include <algorithm>
#include <ranges>

#include "melon/detail/specialization_of.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/views/graph_view.hpp"

namespace fhamonic {
namespace melon {
namespace views {

template <graph _Graph, input_mapping<vertex_t<_Graph>> _VertexFilter,
          input_mapping<arc_t<_Graph>> _ArcFilter>
    requires std::convertible_to<
                 mapped_value_t<_VertexFilter, vertex_t<_Graph>>, bool> &&
             std::convertible_to<mapped_value_t<_ArcFilter, arc_t<_Graph>>,
                                 bool>
class subgraph : public graph_view_base {
public:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;

private:
    _Graph _graph;
    _VertexFilter _vertex_filter;
    _ArcFilter _arc_filter;

public:
    template <typename _G, typename _VF = true_map, typename _AF = true_map>
        requires(!__detail::__specialization_of<_G, subgraph>)
    [[nodiscard]] constexpr explicit subgraph(_G && g,
                                              _VF && vertex_filter = true_map{},
                                              _AF && arc_filter = true_map{})
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _vertex_filter(views::mapping_all(std::forward<_VF>(vertex_filter)))
        , _arc_filter(views::mapping_all(std::forward<_AF>(arc_filter))) {}

    [[nodiscard]] constexpr subgraph(const subgraph &) = default;
    [[nodiscard]] constexpr subgraph(subgraph &&) = default;

    constexpr subgraph & operator=(const subgraph &) = default;
    constexpr subgraph & operator=(subgraph &&) = default;

    [[nodiscard]] constexpr decltype(auto) num_vertices() const noexcept
        requires has_num_vertices<_Graph> &&
                 std::same_as<_VertexFilter, true_map>
    {
        return melon::num_vertices(_graph);
    }

    void disable_vertex(const vertex & v) noexcept
        requires output_mapping_of<_VertexFilter, vertex_t<_Graph>, bool>
    {
        _vertex_filter[v] = false;
    }
    void enable_vertex(const vertex & v) noexcept
        requires output_mapping_of<_VertexFilter, vertex_t<_Graph>, bool>
    {
        _vertex_filter[v] = true;
    }
    bool is_valid_vertex(const vertex & v) const noexcept {
        if constexpr(has_vertex_removal<_Graph>)
            return _vertex_filter[v] && melon::is_valid_vertex(_graph, v);
        else
            return _vertex_filter[v];
    }

    void disable_arc(const arc & a) const noexcept
        requires output_mapping_of<_ArcFilter, arc_t<_Graph>, bool>
    {
        _arc_filter[a] = false;
    }
    void enable_arc(const arc & a) const noexcept
        requires output_mapping_of<_ArcFilter, arc_t<_Graph>, bool>
    {
        _arc_filter[a] = true;
    }
    bool is_valid_arc(const arc & a) const noexcept {
        if constexpr(has_arc_removal<_Graph>)
            return _arc_filter[a] && melon::is_valid_arc(_graph, a);
        else
            return _arc_filter[a];
    }

    auto vertices() const noexcept {
        if constexpr(std::same_as<_VertexFilter, true_map>) {
            return melon::vertices(_graph);
        } else {
            return std::views::filter(
                melon::vertices(_graph),
                [this](const vertex & v) { return _vertex_filter[v]; });
        }
    }
    auto arcs() const noexcept
        requires std::same_as<_VertexFilter,
                              true_map>  // if false, use the indicidence
                                         // join hierarchy of melon::arcs(g)
    {
        if constexpr(std::same_as<_ArcFilter, true_map>) {
            return melon::arcs(_graph);
        } else {
            return std::views::filter(
                melon::arcs(_graph),
                [this](const arc & a) { return _arc_filter[a]; });
        }
    }

    auto arc_source(const arc & a) const noexcept
        requires has_arc_source<_Graph>
    {
        assert(is_valid_arc(a));
        return melon::arc_source(_graph, a);
    }
    auto arc_sources_map() const noexcept
        requires has_arc_source<_Graph>
    {
        return melon::arc_sources_map(_graph);
    }
    auto arc_target(const arc & a) const noexcept
        requires has_arc_target<_Graph>
    {
        assert(is_valid_arc(a));
        return melon::arc_target(_graph, a);
    }
    auto arc_targets_map() const noexcept
        requires has_arc_target<_Graph>
    {
        return melon::arc_targets_map(_graph);
    }

    auto in_arcs(const vertex & v) const noexcept
        requires inward_incidence_graph<_Graph>
    {
        assert(is_valid_vertex(v));
        if constexpr(std::same_as<_VertexFilter, true_map> &&
                     std::same_as<_ArcFilter, true_map>) {
            return melon::in_arcs(_graph, v);
        } else if constexpr(std::same_as<_VertexFilter, true_map>) {
            return std::views::filter(
                melon::in_arcs(_graph, v),
                [this](const arc & a) { return _arc_filter[a]; });
        } else {
            return std::views::filter(
                melon::in_arcs(_graph, v), [this](const arc & a) {
                    return _vertex_filter[melon::arc_source(_graph, a)] &&
                           _arc_filter[a];
                });
        }
    }
    auto out_arcs(const vertex & v) const noexcept
        requires outward_incidence_graph<_Graph>
    {
        assert(is_valid_vertex(v));
        if constexpr(std::same_as<_VertexFilter, true_map> &&
                     std::same_as<_ArcFilter, true_map>) {
            return melon::out_arcs(_graph, v);
        } else if constexpr(std::same_as<_VertexFilter, true_map>) {
            return std::views::filter(
                melon::out_arcs(_graph, v),
                [this](const arc & a) { return _arc_filter[a]; });
            // return arc_filter.filter(melon::out_arcs(_graph, v));
        } else {
            return std::views::filter(
                melon::out_arcs(_graph, v), [this](const arc & a) {
                    return _vertex_filter[melon::arc_target(_graph, a)] &&
                           _arc_filter[a];
                });
        }
    }

    auto in_neighbors(const vertex & v) const noexcept
        requires inward_adjacency_graph<_Graph>
    {
        assert(is_valid_vertex(v));
        if constexpr(std::same_as<_VertexFilter, true_map> &&
                     std::same_as<_ArcFilter, true_map>) {
            return melon::in_neighbors(_graph, v);
        } else if constexpr(std::same_as<_ArcFilter, true_map>) {
            return std::views::filter(
                melon::in_neighbors(_graph, v),
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
        requires outward_adjacency_graph<_Graph>
    {
        assert(is_valid_vertex(v));
        if constexpr(std::same_as<_VertexFilter, true_map> &&
                     std::same_as<_ArcFilter, true_map>) {
            return melon::out_neighbors(_graph, v);
        } else if constexpr(std::same_as<_ArcFilter, true_map>) {
            return std::views::filter(
                melon::out_neighbors(_graph, v),
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
        requires has_vertex_map<_Graph>
    decltype(auto) create_vertex_map() const noexcept {
        return melon::create_vertex_map<T>(_graph);
    }
    template <typename T>
        requires has_vertex_map<_Graph>
    decltype(auto) create_vertex_map(T default_value) const noexcept {
        return melon::create_vertex_map<T>(_graph, default_value);
    }

    template <typename T>
        requires has_arc_map<_Graph>
    decltype(auto) create_arc_map() const noexcept {
        return melon::create_arc_map<T>(_graph);
    }
    template <typename T>
        requires has_arc_map<_Graph>
    decltype(auto) create_arc_map(T default_value) const noexcept {
        return melon::create_arc_map<T>(_graph, default_value);
    }
};

template <typename _G, typename _VF = true_map, typename _AF = true_map>
subgraph(_G &&, _VF && = {}, _AF && = {})
    -> subgraph<views::graph_all_t<_G>, views::mapping_all_t<_VF>,
                views::mapping_all_t<_AF>>;

}  // namespace views
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_VIEWS_SUBGRAPH_HPP