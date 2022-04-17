#ifndef MELON_STATIC_DIGRAPH_BUILDER_HPP
#define MELON_STATIC_DIGRAPH_BUILDER_HPP

#include <algorithm>
#include <numeric>
#include <ranges>
#include <vector>

#include <range/v3/algorithm/sort.hpp>
#include <range/v3/view/zip.hpp>

#include "melon/concepts/graph_concepts.hpp"

namespace fhamonic {
namespace melon {

template <concepts::graph G, typename... ArcProperty>
class arc_list_builder {
public:
    using vertex_t = typename G::vertex_t;
    using arc_t = typename G::arc_t;

    using PropertyMaps = std::tuple<std::vector<ArcProperty>...>;

private:
    std::size_t _nb_vertices;
    std::vector<vertex_t> _arc_sources;
    std::vector<vertex_t> _arc_targets;
    PropertyMaps _arc_property_maps;

public:
    arc_list_builder() : _nb_vertices(0) {}
    arc_list_builder(std::size_t nb_vertices)
        : _nb_vertices(nb_vertices) {}

private:
    template <class Maps, class Properties, std::size_t... Is>
    void add_properties(Maps && maps, Properties && properties,
                        std::index_sequence<Is...>) {
        (get<Is>(maps).push_back(get<Is>(properties)), ...);
    }

public:
    void add_arc(vertex_t u, vertex_t v, ArcProperty... properties) {
        assert(_nb_vertices > std::max(u, v));
        _arc_sources.push_back(u);
        _arc_targets.push_back(v);
        add_properties(
            _arc_property_maps, std::make_tuple(properties...),
            std::make_index_sequence<std::tuple_size<PropertyMaps>{}>{});
    }

    auto build() {
        // sort _arc_sources, arc_tagrets and _arc_property_maps
        auto arcs_zipped_view = std::apply(
            [this](auto &&... property_map) {
                return ranges::view::zip(_arc_sources, _arc_targets,
                                         property_map...);
            },
            _arc_property_maps);
        ranges::sort(arcs_zipped_view, [](const auto & a, const auto & b) {
            if(std::get<0>(a) == std::get<0>(b))
                return std::get<1>(a) < std::get<1>(b);
            return std::get<0>(a) < std::get<0>(b);
        });
        // create graph
        G graph(_nb_vertices, std::move(_arc_sources), std::move(_arc_targets));
        return std::apply(
            [this, &graph](auto &&... property_map) {
                return std::make_tuple(graph, property_map...);
            },
            _arc_property_maps);
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STATIC_DIGRAPH_BUILDER_HPP
