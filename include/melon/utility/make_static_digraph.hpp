#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <functional>
#include <iterator>
#include <limits>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>

#include "melon/container/static_digraph.hpp"
#include "melon/container/static_map.hpp"
#include "melon/detail/stdlib_check.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"

namespace melon {

// Rebuilds any outward-incidence graph as a static_digraph whose vertices are
// renumbered 0..n-1 in the order vertex_cmp induces, and translates the given
// vertex and arc maps onto the new handles. Returns one flat tuple, the
// static_digraph_builder shape: the graph, then the translated vertex maps,
// then the translated arc maps. The input maps are only read, so the tuples
// may hold references (std::tie, forward_as_tuple) as well as values.
template <typename G, typename Cmp = std::less<vertex_t<G>>,
          typename... VertexMap, typename... ArcMap>
    requires outward_incidence_graph<G> && has_vertex_map<G> &&
             std::strict_weak_order<Cmp &, const vertex_t<G> &,
                                    const vertex_t<G> &> &&
             (mapping<VertexMap, vertex_t<G>> && ...) &&
             (mapping<ArcMap, arc_t<G>> && ...)
[[nodiscard]] auto make_static_digraph(
    const G & graph, Cmp vertex_cmp = {},
    std::tuple<VertexMap...> vertex_maps = {},
    std::tuple<ArcMap...> arc_maps = {}) {
    using SG = static_digraph;
    using new_vertex = vertex_t<SG>;
    using new_arc = arc_t<SG>;

    std::vector<vertex_t<G>> new_to_old_vertex;
    if constexpr(has_num_vertices<G>)
        new_to_old_vertex.reserve(melon::num_vertices(graph));
    std::ranges::copy(melon::vertices(graph),
                      std::back_inserter(new_to_old_vertex));
    std::ranges::sort(new_to_old_vertex, vertex_cmp);

    const std::size_t n = new_to_old_vertex.size();
    assert(n <= std::numeric_limits<new_vertex>::max());

    auto old_to_new_vertex = melon::create_vertex_map<new_vertex>(graph);
    for(std::size_t i = 0; i < n; ++i)
        old_to_new_vertex[new_to_old_vertex[i]] = static_cast<new_vertex>(i);

    std::vector<new_vertex> sources;
    std::vector<new_vertex> targets;
    std::vector<arc_t<G>> new_to_old_arc;
    if constexpr(has_num_arcs<G>) {
        const auto m_hint = melon::num_arcs(graph);
        sources.reserve(m_hint);
        targets.reserve(m_hint);
        new_to_old_arc.reserve(m_hint);
    }
    // Emitting arcs grouped by new source order is what satisfies the
    // is_sorted(_arc_source) precondition of static_digraph's constructor.
    for(std::size_t i = 0; i < n; ++i) {
        for(auto && old_a : melon::out_arcs(graph, new_to_old_vertex[i])) {
            sources.emplace_back(static_cast<new_vertex>(i));
            targets.emplace_back(
                old_to_new_vertex[melon::arc_target(graph, old_a)]);
            new_to_old_arc.emplace_back(old_a);
        }
    }
    const std::size_t m = new_to_old_arc.size();
    assert(m <= std::numeric_limits<new_arc>::max());

    const auto translate_vertex_map = [&](const auto & old_map) {
        static_map<new_vertex, mapped_value_t<decltype(old_map), vertex_t<G>>>
            new_map(n);
        for(std::size_t i = 0; i < n; ++i)
            new_map[static_cast<new_vertex>(i)] = old_map[new_to_old_vertex[i]];
        return new_map;
    };
    const auto translate_arc_map = [&](const auto & old_map) {
        static_map<new_arc, mapped_value_t<decltype(old_map), arc_t<G>>>
            new_map(m);
        for(std::size_t i = 0; i < m; ++i)
            new_map[static_cast<new_arc>(i)] = old_map[new_to_old_arc[i]];
        return new_map;
    };

    return std::apply(
        [&](const VertexMap &... vertex_map) {
            return std::apply(
                [&](const ArcMap &... arc_map) {
                    return std::make_tuple(SG(n, sources, targets),
                                           translate_vertex_map(vertex_map)...,
                                           translate_arc_map(arc_map)...);
                },
                arc_maps);
        },
        vertex_maps);
}

}  // namespace melon
