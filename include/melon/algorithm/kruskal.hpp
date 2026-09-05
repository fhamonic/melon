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
#include "melon/graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace melon {

struct kruskal_roles {
    struct component {};
};

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
    disjoint_sets<vertex,
                  vertex_map_t<UGraph, unsigned int, kruskal_roles::component>>
        _components_sets;

public:
    // ---- Construction -------------------------------------------------------

    template <graph_for<UGraph> UG, mapping_for<CostMap> CM>
    constexpr kruskal(UG && ug, CM && cm)
        : _ugraph(views::graph_all(std::forward<UG>(ug)))
        , _cost_map(maps::mapping_all(std::forward<CM>(cm)))
        , _components_sets(
              create_vertex_map<unsigned int, kruskal_roles::component>(
                  _ugraph)) {
        reset();
    }

    // Move-only; see the melon::traversal_algorithm concept.
    // Moves stay defaulted: _cursor is an iterator into _sorted_edges, whose
    // buffer transfers with the move. Any copy would have to rebase it --
    // an iterator into the source's buffer never compares equal to the new
    // object's end(), so finished() never becomes true and advance() walks off
    // the end.
    constexpr kruskal(const kruskal &) = delete;
    constexpr kruskal(kruskal &&) = default;

    constexpr kruskal & operator=(const kruskal &) = delete;
    constexpr kruskal & operator=(kruskal &&) = default;

    // ---- Base access --------------------------------------------------------

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
    constexpr bool merge_endpoints_of(const edge & e) {
        auto && [u, v] = edge_endpoints(_ugraph, e);
        const auto cu = _components_sets.find(u);
        const auto cv = _components_sets.find(v);
        if(cu == cv) return false;
        _components_sets.merge(cu, cv);
        return true;
    }

public:
    // ---- Setup --------------------------------------------------------------

    // Not noexcept: it refills, sorts and re-seeds, all of which allocate, and
    // the sort runs the user's cost map.
    constexpr kruskal & reset() {
        // reset() is re-runnable, so the edge list and the component sets must
        // be emptied first: otherwise a second call appends the whole edge list
        // again and re-pushes every vertex, and both grow without bound.
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
        // Seed the cursor through the same acceptance test advance() applies:
        // merging *_sorted_edges.begin() outright dereferences end() on a graph
        // with no edges, and takes a cheapest self-loop into the tree.
        _cursor = _sorted_edges.begin();
        if(!finished() && !merge_endpoints_of(*_cursor)) advance();
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_cursor == _sorted_edges.end())) {
        return _cursor == _sorted_edges.end();
    }

    [[nodiscard]] constexpr edge current() const
        noexcept(noexcept(edge(*_cursor))) {
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
kruskal(UGraph &&, CostMap &&)
    -> kruskal<views::graph_all_t<UGraph>, maps::mapping_all_t<CostMap>>;

}  // namespace melon
