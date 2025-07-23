#ifndef MELON_ALGORITHM_KRUSKAL_HPP
#define MELON_ALGORITHM_KRUSKAL_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// #include "melon/detail/map_if.hpp"
#include "melon/container/disjoint_sets.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace fhamonic {
namespace melon {

template <undirected_graph _UGraph, input_mapping<edge_t<_UGraph>> _CostMap>
    requires has_vertex_map<_UGraph>
class kruskal {
private:
    using vertex = vertex_t<_UGraph>;
    using edge = edge_t<_UGraph>;

private:
    _UGraph _ugraph;
    _CostMap _cost_map;
    std::vector<edge> _sorted_edges;
    std::vector<edge>::iterator _cursor;
    disjoint_sets<vertex, vertex_map_t<_UGraph, unsigned int>> _components_sets;

public:
    template <typename _G, typename _M>
    [[nodiscard]] constexpr explicit kruskal(_G && g, _M && c)
        : _ugraph(views::undirected_graph_all(std::forward<_G>(g)))
        , _cost_map(views::mapping_all(std::forward<_M>(c)))
        , _components_sets(create_vertex_map<unsigned int>(_ugraph)) {
        reset();
    }

    [[nodiscard]] constexpr kruskal(const kruskal &) = default;
    [[nodiscard]] constexpr kruskal(kruskal &&) = default;

    constexpr kruskal & operator=(const kruskal &) = default;
    constexpr kruskal & operator=(kruskal &&) = default;

    constexpr void reset() noexcept {
        if constexpr(has_num_edges<_UGraph>) {
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

    [[nodiscard]] constexpr auto begin() noexcept {
        return algorithm_iterator(*this);
    }
    [[nodiscard]] constexpr auto end() noexcept {
        return std::default_sentinel;
    }
};

template <typename _UGraph, typename _CostMap>
kruskal(_UGraph &&, _CostMap &&)
    -> kruskal<views::undirected_graph_all_t<_UGraph>,
               views::mapping_all_t<_CostMap>>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_KRUSKAL_HPP
