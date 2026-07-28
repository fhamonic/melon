#pragma once

#include <algorithm>
#include <cassert>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/disjoint_sets.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace melon {

template <undirected_graph UGraph, input_mapping<edge_t<UGraph>> CostMap>
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
    template <typename UG, typename M>
    [[nodiscard]] constexpr explicit kruskal(UG && g, M && c)
        : _ugraph(views::undirected_graph_all(std::forward<UG>(g)))
        , _cost_map(views::mapping_all(std::forward<M>(c)))
        , _components_sets(create_vertex_map<unsigned int>(_ugraph)) {
        reset();
    }

    [[nodiscard]] constexpr kruskal(const kruskal &) = default;
    [[nodiscard]] constexpr kruskal(kruskal &&) = default;

    constexpr kruskal & operator=(const kruskal &) = default;
    constexpr kruskal & operator=(kruskal &&) = default;

    constexpr void reset() noexcept {
        if constexpr(has_num_edges<UGraph>) {
            _sorted_edges.reserve(num_edges(_ugraph));
        }
        std::ranges::copy(edges(_ugraph), std::back_inserter(_sorted_edges));
        std::ranges::sort(_sorted_edges, [this](auto && e1, auto && e2) {
            return _cost_map[e1] < _cost_map[e2];
        });
        _cursor = _sorted_edges.begin();
        for(auto && v : vertices(_ugraph)) _components_sets.push(v);
        auto && [u, v] = edge_endpoints(_ugraph, *_cursor);
        _components_sets.merge_keys(u, v);
    }

    constexpr bool finished() const noexcept {
        return _cursor == _sorted_edges.end();
    }

    constexpr edge current() const noexcept { return *_cursor; }

    constexpr void advance() noexcept {
        assert(!finished());
        for(++_cursor; !finished(); ++_cursor) {
            auto && [u, v] = edge_endpoints(_ugraph, *_cursor);
            auto cu = _components_sets.find(u);
            auto cv = _components_sets.find(v);
            if(cu == cv) continue;
            _components_sets.merge(cu, cv);
            return;
        }
    }
};

template <typename UGraph, typename CostMap>
kruskal(UGraph &&, CostMap &&) -> kruskal<views::undirected_graph_all_t<UGraph>,
                                          views::mapping_all_t<CostMap>>;

}  // namespace melon
