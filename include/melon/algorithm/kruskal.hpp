#pragma once

#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/disjoint_sets.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace melon {

template <undirected_graph_view UGraph, mapping_view<edge_t<UGraph>> CostMap>
    requires has_vertex_map<UGraph>
class kruskal : public algorithm_view_interface<kruskal<UGraph, CostMap>> {
private:
    using vertex = vertex_t<UGraph>;
    using edge = edge_t<UGraph>;

private:
    UGraph _ugraph;
    CostMap _cost_map;
    std::vector<edge> _sorted_edges;
    std::vector<edge>::iterator _cursor;
    disjoint_sets<vertex, vertex_map_t<UGraph, unsigned int>> _components_sets;

public:
    template <undirected_graph_for<UGraph> UG, mapping_for<CostMap> CM>
    constexpr kruskal(UG && ug, CM && cm)
        : _ugraph(views::undirected_graph_all(std::forward<UG>(ug)))
        , _cost_map(maps::mapping_all(std::forward<CM>(cm)))
        , _components_sets(create_vertex_map<unsigned int>(_ugraph)) {
        reset();
    }

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    // Moves stay defaulted: _cursor is an iterator into _sorted_edges, whose
    // buffer transfers with the move. Only the copy needed a rebase -- it
    // handed the new object an iterator into the source's buffer, so
    // finished(), comparing it against the copy's own end(), never became true
    // and advance() walked off the end.
    constexpr kruskal(const kruskal &) = delete;
    constexpr kruskal(kruskal &&) = default;

    constexpr kruskal & operator=(const kruskal &) = delete;
    constexpr kruskal & operator=(kruskal &&) = default;

    // The graph the algorithm runs over. An algorithm owns its view rather
    // than adapting it, so this is the std::ranges::owning_view shape --
    // references, ref-qualified -- and not the filter_view shape the graph
    // *views* use. Returning a copy here would also put traversal_forest back
    // where it started: it reaches its sources through base(), and an owned
    // graph view is move-only.
    [[nodiscard]] constexpr UGraph & base() & noexcept { return _ugraph; }
    [[nodiscard]] constexpr const UGraph & base() const & noexcept {
        return _ugraph;
    }
    [[nodiscard]] constexpr UGraph && base() && noexcept {
        return std::move(_ugraph);
    }
    [[nodiscard]] constexpr const UGraph && base() const && noexcept {
        return std::move(_ugraph);
    }

private:
    // The acceptance test reset() and advance() share: take the edge only if
    // its endpoints sit in distinct components, and merge them if so.
    constexpr bool merge_endpoints_of(const edge & e) {
        auto && [u, v] = edge_endpoints(_ugraph, e);
        const auto cu = _components_sets.find(u);
        const auto cv = _components_sets.find(v);
        if(cu == cv) return false;
        _components_sets.merge(cu, cv);
        return true;
    }

public:
    // Not noexcept: it refills, sorts and re-seeds, all of which allocate and
    // all of which run the user's cost map. Returns *this like every other
    // algorithm's reset(); it used to return void.
    constexpr kruskal & reset() {
        // resize(0) first, and clear the sets: without it a second reset()
        // appended the whole edge list again and re-pushed every vertex, so
        // both grew without bound across runs.
        _sorted_edges.resize(0);
        if constexpr(has_num_edges<UGraph>) {
            _sorted_edges.reserve(num_edges(_ugraph));
        }
        std::ranges::copy(edges(_ugraph), std::back_inserter(_sorted_edges));
        std::ranges::sort(_sorted_edges, [this](auto && e1, auto && e2) {
            return _cost_map[e1] < _cost_map[e2];
        });
        _components_sets.clear();
        for(auto && v : vertices(_ugraph)) _components_sets.push(v);
        // Seed the cursor through the same test advance() uses. Merging
        // *_sorted_edges.begin() outright dereferenced end() on a graph with no
        // edges, and took a cheapest self-loop into the tree.
        _cursor = _sorted_edges.begin();
        if(!finished() && !merge_endpoints_of(*_cursor)) advance();
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_cursor == _sorted_edges.end())) {
        return _cursor == _sorted_edges.end();
    }

    [[nodiscard]] constexpr edge current() const noexcept(noexcept(*_cursor)) {
        assert(!finished());
        return *_cursor;
    }

    constexpr void advance() {
        assert(!finished());
        for(++_cursor; !finished(); ++_cursor) {
            if(merge_endpoints_of(*_cursor)) return;
        }
    }
};

template <typename UGraph, typename CostMap>
kruskal(UGraph &&, CostMap &&) -> kruskal<views::undirected_graph_all_t<UGraph>,
                                          maps::mapping_all_t<CostMap>>;

}  // namespace melon
