#ifndef MELON_ALGORITHM_STRONGLY_CONNECTED_COMPONENTS_HPP
#define MELON_ALGORITHM_STRONGLY_CONNECTED_COMPONENTS_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <stack>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/detail/constexpr_ternary.hpp"
#include "melon/detail/consumable_range.hpp"
#include "melon/graph.hpp"
#include "melon/utility/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

template <outward_adjacency_graph G>
    requires has_vertex_map<G>
class strongly_connected_components {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;

    using component_num = unsigned int;

    static constexpr component_num INVALID_COMPONENT =
        std::numeric_limits<component_num>::max();

    std::reference_wrapper<const G> _graph;

    consumable_range<vertices_range_t<G>> _remaining_vertices;
    std::vector<std::pair<vertex, consumable_range<out_neighbors_range_t<G>>>>
        _dfs_stack;

    std::vector<vertex> _tarjan_stack;
    component_num start_index;
    component_num index;

    vertex_map_t<G, bool> _reached_map;
    vertex_map_t<G, component_num> _index_map;
    vertex_map_t<G, component_num> _lowlink_map;

public:
    [[nodiscard]] constexpr explicit strongly_connected_components(
        const G & g) noexcept
        : _graph(g)
        , _remaining_vertices(vertices(_graph.get()))
        , _dfs_stack()
        , _tarjan_stack()
        , start_index(0)
        , index(0)
        , _reached_map(create_vertex_map<bool>(g, false))
        , _index_map(create_vertex_map<component_num>(g, INVALID_COMPONENT))
        , _lowlink_map(create_vertex_map<component_num>(g, INVALID_COMPONENT)) {
        _dfs_stack.reserve(g.nb_vertices());
    }

    [[nodiscard]] constexpr strongly_connected_components(
        const strongly_connected_components &) = default;
    [[nodiscard]] constexpr strongly_connected_components(
        strongly_connected_components &&) = default;

    constexpr strongly_connected_components & operator=(
        const strongly_connected_components &) = default;
    constexpr strongly_connected_components & operator=(
        strongly_connected_components &&) = default;

    // constexpr strongly_connected_components & reset() noexcept {
    //     _dfs_stack.resize(0);
    //     _reached_map.fill(false);
    //     return *this;
    // }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _remaining_vertices.empty();
    }

    [[nodiscard]] constexpr vertex current() const noexcept {
        assert(!finished());
        return _dfs_stack.back().first;
    }

    constexpr void advance() noexcept { assert(!finished()); }

    constexpr void run() noexcept {
        index = 0;
        for(;;) {
            while(_reached_map[_remaining_vertices.current()]) {
                _remaining_vertices.advance();
                if(_remaining_vertices.empty()) return;
            }
            vertex s = _remaining_vertices.current();
            _reached_map[s] = true;
            _index_map[s] = _lowlink_map[s] = start_index = index;
            ++index;
            _tarjan_stack.push_back(s);
            _dfs_stack.emplace_back(s, out_neighbors(_graph.get(), s));

            for(;;) {
                vertex v = _dfs_stack.back().first;
                for(auto & neighbors = _dfs_stack.back().second;
                    !neighbors.empty(); neighbors.advance()) {
                    vertex w = neighbors.current();
                    if(_reached_map[w]) {
                        if(_index_map[w] >= start_index) {
                            _lowlink_map[v] =
                                std::min(_lowlink_map[v], _lowlink_map[w]);
                        }
                        continue;
                    };
                    _reached_map[w] = true;
                    _index_map[w] = _lowlink_map[w] = index;
                    ++index;
                    _tarjan_stack.push_back(w);
                    _dfs_stack.emplace_back(w, out_neighbors(_graph.get(), w));
                };
                if(_index_map[v] == _lowlink_map[v]) {
                    std::cout << '\n';
                    for(;;) {
                        vertex w = _tarjan_stack.back();
                        std::cout << '\t' << w;
                        _tarjan_stack.pop_back();
                        if(w == v) break;
                    }
                }
                _dfs_stack.pop_back();
                if(_dfs_stack.empty()) break;
                vertex parent = _dfs_stack.back().first;
                _lowlink_map[parent] =
                    std::min(_lowlink_map[parent], _lowlink_map[v]);
            }
        }
    }
    [[nodiscard]] constexpr auto begin() noexcept {
        return traversal_iterator(*this);
    }
    [[nodiscard]] constexpr auto end() const noexcept {
        return traversal_end_sentinel();
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _reached_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_STRONGLY_CONNECTED_COMPONENTS_HPP