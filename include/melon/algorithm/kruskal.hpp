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
    // undirected_graph_storable_as / mapping_storable_as, so
    // std::is_constructible answers what the mem-initializers actually do --
    // see dijkstra's constructor.
    template <typename UG, typename M>
        requires undirected_graph_storable_as<UG, UGraph> &&
                 mapping_storable_as<M, CostMap>
    constexpr kruskal(UG && g, M && c)
        : _ugraph(detail::store_undirected_graph<UGraph>(std::forward<UG>(g)))
        , _cost_map(detail::store_mapping<CostMap>(std::forward<M>(c)))
        , _components_sets(create_vertex_map<unsigned int>(_ugraph)) {
        reset();
    }

    // _cursor is an iterator *into* _sorted_edges. A memberwise copy hands the
    // new object an iterator into the *source's* buffer, so finished() --
    // which compares it against the copy's own end() -- never becomes true and
    // advance() walks off the end (ASan: heap-buffer-overflow on the first
    // edge_endpoints of a copied kruskal). Copy the edge list, then rebase.
    // Move is fine: the vector's buffer transfers with it.
    // Constrained on the copyability of what it copies: a user-provided
    // special member of a class template is only instantiated when called, so
    // without the requires-clause std::copyable answered true for a move-only
    // UGraph or CostMap and the failure moved to the call site.
    constexpr kruskal(const kruskal & o)
        requires std::copy_constructible<UGraph> &&
                     std::copy_constructible<CostMap>
        : _ugraph(o._ugraph)
        , _cost_map(o._cost_map)
        , _sorted_edges(o._sorted_edges)
        , _components_sets(o._components_sets) {
        _cursor = _sorted_edges.begin() + (o._cursor - o._sorted_edges.begin());
    }
    constexpr kruskal(kruskal &&) = default;

    constexpr kruskal & operator=(const kruskal & o)
        requires std::copyable<UGraph> && std::copyable<CostMap>
    {
        if(this == std::addressof(o)) return *this;
        const auto offset = o._cursor - o._sorted_edges.begin();
        _ugraph = o._ugraph;
        _cost_map = o._cost_map;
        _sorted_edges = o._sorted_edges;
        _cursor = _sorted_edges.begin() + offset;
        _components_sets = o._components_sets;
        return *this;
    }
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
