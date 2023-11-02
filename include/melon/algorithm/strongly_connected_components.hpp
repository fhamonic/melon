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

#include <range/v3/view/concat.hpp>
#include <range/v3/view/single.hpp>

#include "melon/detail/constexpr_ternary.hpp"
#include "melon/detail/consumable_range.hpp"
#include "melon/detail/intrusive_view.hpp"
#include "melon/graph.hpp"
#include "melon/utility/traversal_iterator.hpp"

#include "melon/views/graph_view.hpp"

namespace fhamonic {
namespace melon {

template <outward_adjacency_graph _Graph>
    requires has_vertex_map<_Graph>
class strongly_connected_components {
private:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;

    using component_num = unsigned int;

    static constexpr component_num INVALID_COMPONENT =
        std::numeric_limits<component_num>::max();

    _Graph _graph;

    consumable_range<vertices_range_t<_Graph>> _remaining_vertices;
    std::vector<
        std::pair<vertex, consumable_range<out_neighbors_range_t<_Graph>>>>
        _dfs_stack;

    std::vector<vertex> _tarjan_stack;
    component_num start_index;
    component_num index;

    vertex_map_t<_Graph, bool> _reached_map;
    vertex_map_t<_Graph, component_num> _index_map;
    vertex_map_t<_Graph, component_num> _lowlink_map;

public:
    template <typename _Tp>
    [[nodiscard]] constexpr explicit strongly_connected_components(
        _Tp && g) noexcept
        : _graph(views::graph_all(std::forward<_Tp>(g)))
        , _remaining_vertices(vertices(_graph))
        , _dfs_stack()
        , _tarjan_stack()
        , start_index(0)
        , index(0)
        , _reached_map(create_vertex_map<bool>(_graph, false))
        , _index_map(
              create_vertex_map<component_num>(_graph, INVALID_COMPONENT))
        , _lowlink_map(
              create_vertex_map<component_num>(_graph, INVALID_COMPONENT)) {
        if constexpr(has_nb_vertices<_Graph>) {
            _dfs_stack.reserve(nb_vertices(_graph));
            _tarjan_stack.reserve(nb_vertices(_graph));
        }
        advance();
    }

    [[nodiscard]] constexpr strongly_connected_components(
        const strongly_connected_components &) = default;
    [[nodiscard]] constexpr strongly_connected_components(
        strongly_connected_components &&) = default;

    constexpr strongly_connected_components & operator=(
        const strongly_connected_components &) = default;
    constexpr strongly_connected_components & operator=(
        strongly_connected_components &&) = default;

    constexpr strongly_connected_components & reset() noexcept {
        index = 0;
        _remaining_vertices = vertices(_graph);
        _dfs_stack.clear();
        _tarjan_stack.resize(0);
        _reached_map.fill(false);
        return *this;
    }

    [[nodiscard]] constexpr bool finished() noexcept {
        return _remaining_vertices.empty();
    }

    [[nodiscard]] constexpr auto current() noexcept {
        assert(!finished());
        return ranges::views::concat(
            intrusive_view(
                std::monostate{},
                [this](std::monostate) -> vertex {
                    return _tarjan_stack.back();
                },
                [this](std::monostate) mutable -> std::monostate {
                    _tarjan_stack.pop_back();
                    return std::monostate{};
                },
                [this](std::monostate) -> bool {
                    return _dfs_stack.back().first != _tarjan_stack.back();
                }),
            ranges::single_view(_dfs_stack.back().first));
    }

    constexpr void advance() noexcept {
        assert(!_remaining_vertices.empty());

        if(!_dfs_stack.empty()) {
            vertex v = _dfs_stack.back().first;
            while(_tarjan_stack.back() != v) {
                _tarjan_stack.pop_back();
            }
            _tarjan_stack.pop_back();
            _dfs_stack.pop_back();
            if(!_dfs_stack.empty()) {
                vertex parent = _dfs_stack.back().first;
                _lowlink_map[parent] =
                    std::min(_lowlink_map[parent], _lowlink_map[v]);
            }
        }

        if(_dfs_stack.empty()) {
            while(_reached_map[_remaining_vertices.current()]) {
                _remaining_vertices.advance();
                if(_remaining_vertices.empty()) return;
            }
            vertex s = _remaining_vertices.current();
            _reached_map[s] = true;
            _index_map[s] = _lowlink_map[s] = start_index = index;
            ++index;
            _tarjan_stack.push_back(s);
            _dfs_stack.emplace_back(s, out_neighbors(_graph, s));
        }

        for(;;) {
            while(!_dfs_stack.back().second.empty()) {
                vertex w = _dfs_stack.back().second.current();
                _dfs_stack.back().second.advance();
                if(_reached_map[w]) {
                    if(_index_map[w] >= start_index) {
                        vertex v = _dfs_stack.back().first;
                        _lowlink_map[v] =
                            std::min(_lowlink_map[v], _lowlink_map[w]);
                    }
                    continue;
                }
                _reached_map[w] = true;
                _index_map[w] = _lowlink_map[w] = index;
                ++index;
                _tarjan_stack.push_back(w);
                _dfs_stack.emplace_back(w, out_neighbors(_graph, w));
            }
            vertex v = _dfs_stack.back().first;
            if(_index_map[v] == _lowlink_map[v]) return;
            _dfs_stack.pop_back();
            vertex parent = _dfs_stack.back().first;
            _lowlink_map[parent] =
                std::min(_lowlink_map[parent], _lowlink_map[v]);
        }
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
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

    [[nodiscard]] constexpr bool same_component(
        const vertex & u, const vertex & v) const noexcept {
        return _lowlink_map[u] == _lowlink_map[v];
    }
};

template <typename _Graph>
strongly_connected_components(_Graph &&)
    -> strongly_connected_components<views::graph_all_t<_Graph>>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_STRONGLY_CONNECTED_COMPONENTS_HPP