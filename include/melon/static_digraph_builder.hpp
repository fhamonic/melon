#ifndef STATIC_DIGRAPH_BUILDER_HPP
#define STATIC_DIGRAPH_BUILDER_HPP

#include <algorithm>
#include <numeric>
#include <ranges>
#include <vector>

#include <range/v3/algorithm/sort.hpp>
#include <range/v3/view/zip.hpp>

#include "melon/static_digraph.hpp"

namespace melon {

template <typename... ArcProperty>
class StaticDigraphBuilder {
public:
    using Node = std::size_t;
    using Arc = std::size_t;

    using PropertyMaps = std::tuple<std::vector<ArcProperty>...>;

private:
    std::size_t nb_nodes;
    std::vector<Arc> nb_out_arcs;
    std::vector<Node> arc_sources;
    std::vector<Node> arc_targets;

public:
    PropertyMaps arc_property_maps;

public:
    StaticDigraphBuilder() : nb_nodes(0) {}
    StaticDigraphBuilder(std::size_t nb_nodes)
        : nb_nodes(nb_nodes), nb_out_arcs(nb_nodes, 0) {}

    template <class Maps, class Properties, size_t... Is>
    void addProperties(Maps && maps, Properties && properties,
                       std::index_sequence<Is...>) {
        (get<Is>(maps).push_back(get<Is>(properties)), ...);
    }

    void addArc(Node u, Node v, ArcProperty... properties) {
        assert(nb_nodes > std::max(u, v));
        ++nb_out_arcs[u];
        arc_sources.push_back(u);
        arc_targets.push_back(v);
        addProperties(
            arc_property_maps, std::make_tuple(properties...),
            std::make_index_sequence<std::tuple_size<PropertyMaps>{}>{});
    }

    auto build() {
        // sort arc_sources, arc_tagrets and arc_property_maps
        auto arcs_zipped_view = std::apply(
            [&](auto &&... property_map) {
                return ranges::view::zip(arc_sources, arc_targets, property_map...);
            },
            arc_property_maps);
        ranges::sort(arcs_zipped_view, [](const auto & a, const auto & b) {
            if(std::get<0>(a) == std::get<0>(b))
                return std::get<1>(a) < std::get<1>(b);
            return std::get<0>(a) < std::get<0>(b);
        });
        // compute out_arc_begin
        std::exclusive_scan(nb_out_arcs.begin(), nb_out_arcs.end(),
                            nb_out_arcs.begin(), 0);
        // create graph
        StaticDigraph graph(std::move(nb_out_arcs), std::move(arc_targets));
        return std::apply(
            [&](auto &&... property_map) {
                return std::make_tuple(graph, property_map...);
            },
            arc_property_maps);
    }
};

}  // namespace melon

#endif  // STATIC_DIGRAPH_BUILDER_HPP
