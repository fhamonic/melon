#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <numeric>
#include <ranges>
#include <utility>
#include <vector>

#include "melon/detail/stdlib_check.hpp"
#include "melon/graph.hpp"

namespace melon {

template <graph G, typename... ArcProperty>
class static_digraph_builder {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;
    using property_maps = std::tuple<std::vector<ArcProperty>...>;

    std::size_t _num_vertices;
    std::vector<vertex> _arc_sources;
    std::vector<vertex> _arc_targets;
    property_maps _arc_property_maps;

public:
    static_digraph_builder() : _num_vertices(0) {}
    explicit static_digraph_builder(std::size_t num_vertices_)
        : _num_vertices(num_vertices_) {}

private:
    template <class Maps, class Properties, std::size_t... Is>
    void add_properties(Maps & maps, Properties && properties,
                        std::index_sequence<Is...>) {
        (get<Is>(maps).push_back(
             std::get<Is>(std::forward<Properties>(properties))),
         ...);
    }

    void push_arc(vertex u, vertex v, ArcProperty... properties) {
        assert(_num_vertices > std::max(u, v));
        _arc_sources.push_back(u);
        _arc_targets.push_back(v);
        add_properties(
            _arc_property_maps, std::forward_as_tuple(std::move(properties)...),
            std::make_index_sequence<std::tuple_size<property_maps>{}>{});
    }

    void sort_arcs() {
        auto arcs_zipped_view = std::apply(
            [this](auto &&... property_map) {
                return std::views::zip(_arc_sources, _arc_targets,
                                       property_map...);
            },
            _arc_property_maps);
        std::ranges::sort(arcs_zipped_view, [](const auto & a, const auto & b) {
            if(std::get<0>(a) == std::get<0>(b))
                return std::get<1>(a) < std::get<1>(b);
            return std::get<0>(a) < std::get<0>(b);
        });
    }

public:
    // Ref-qualified so that value category survives a chain: a single
    // `static_digraph_builder &` return would make `std::move(b).add_arc(u, v)`
    // an *lvalue* and send the following .build() to the copying overload.
    //
    // The usual rvalue-builder caveat applies: the reference is to the object
    // the chain started from, so binding it past the end of the full
    // expression (`auto && b = static_digraph_builder<G>(6).add_arc(0, 1);`)
    // dangles, exactly as it would for any other member returning *this.
    static_digraph_builder & add_arc(vertex u, vertex v,
                                     ArcProperty... properties) & {
        push_arc(u, v, std::move(properties)...);
        return *this;
    }
    static_digraph_builder && add_arc(vertex u, vertex v,
                                      ArcProperty... properties) && {
        push_arc(u, v, std::move(properties)...);
        return std::move(*this);
    }

    // Copies the property vectors: the builder is still usable afterwards
    // (though build() is not idempotent -- it sorts in place, and a second
    // call re-sorts an already-sorted list).
    [[nodiscard]] auto build() & {
        sort_arcs();
        return std::apply(
            [this](auto &&... property_map) {
                return std::make_tuple(
                    G(_num_vertices, _arc_sources, _arc_targets),
                    property_map...);
            },
            _arc_property_maps);
    }

    // Moves them instead. The endpoint vectors are still copied: they go
    // through static_map's range constructor, which copies whatever it is
    // given. Leaves the builder moved-from: valid, to be destroyed rather than
    // added to.
    [[nodiscard]] auto build() && {
        sort_arcs();
        return std::apply(
            [this](auto &&... property_map) {
                return std::make_tuple(
                    G(_num_vertices, _arc_sources, _arc_targets),
                    std::move(property_map)...);
            },
            _arc_property_maps);
    }
};

}  // namespace melon
