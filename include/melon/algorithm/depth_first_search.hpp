#ifndef MELON_ALGORITHM_depth_first_search_HPP
#define MELON_ALGORITHM_depth_first_search_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <stack>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/detail/consumable_view.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace fhamonic {
namespace melon {

struct depth_first_search_default_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
};

template <outward_adjacency_graph _Graph,
          typename _Traits = depth_first_search_default_traits>
    requires has_vertex_map<_Graph>
class depth_first_search {
private:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;

    static_assert(!_Traits::store_pred_arcs || outward_incidence_graph<_Graph>,
                  "storing predecessor arcs requires outward_incidence_graph.");

    using stack_range =
        std::conditional_t<_Traits::store_pred_arcs, out_arcs_range_t<_Graph>,
                           out_neighbors_range_t<_Graph>>;

private:
    _Graph _graph;
    std::vector<std::pair<vertex, consumable_view<stack_range>>> _stack;
    vertex_map_t<_Graph, bool> _reached_map;

    [[no_unique_address]] vertex_map_if<_Traits::store_pred_vertices, _Graph,
                                        vertex> _pred_vertices_map;
    [[no_unique_address]] vertex_map_if<_Traits::store_pred_arcs, _Graph, arc>
        _pred_arcs_map;
    [[no_unique_address]] vertex_map_if<_Traits::store_distances, _Graph, int>
        _dist_map;

public:
    template <typename _G>
    [[nodiscard]] constexpr explicit depth_first_search(_G && g)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _stack()
        , _reached_map(create_vertex_map<bool>(_graph, false))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _dist_map(_graph) {
        if constexpr(has_num_vertices<_Graph>) {
            _stack.reserve(num_vertices(_graph));
        }
    }

    template <typename _G>
    [[nodiscard]] constexpr depth_first_search(_G && g, const vertex & s)
        : depth_first_search(std::forward<_G>(g)) {
        add_source(s);
    }

    [[nodiscard]] constexpr depth_first_search(const depth_first_search &) =
        default;
    [[nodiscard]] constexpr depth_first_search(depth_first_search &&) = default;

    constexpr depth_first_search & operator=(const depth_first_search &) =
        default;
    constexpr depth_first_search & operator=(depth_first_search &&) = default;

    constexpr depth_first_search & reset() noexcept {
        _stack.resize(0);
        _reached_map.fill(false);
        return *this;
    }
    constexpr depth_first_search & add_source(const vertex & s) noexcept {
        assert(!_reached_map[s]);
        if constexpr(_Traits::store_pred_arcs)
            _stack.emplace_back(s, out_arcs(_graph, s));
        else
            _stack.emplace_back(s, out_neighbors(_graph, s));
        _reached_map[s] = true;
        if constexpr(_Traits::store_pred_vertices) _pred_vertices_map[s] = s;
        if constexpr(_Traits::store_distances) _dist_map[s] = 0;
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _stack.empty();
    }

    [[nodiscard]] constexpr vertex current() const noexcept {
        assert(!finished());
        return _stack.back().first;
    }

    constexpr void advance() noexcept {
        assert(!finished());
        do {
            if constexpr(_Traits::store_pred_arcs) {
                for(auto & remaining_arcs = _stack.back().second;
                    !remaining_arcs.empty(); remaining_arcs.advance()) {
                    auto a = remaining_arcs.current();
                    auto w = arc_target(_graph, a);
                    if(_reached_map[w]) continue;
                    _reached_map[w] = true;
                    _pred_arcs_map[w] = a;
                    if constexpr(_Traits::store_pred_vertices)
                        _pred_vertices_map[w] = _stack.back().first;
                    if constexpr(_Traits::store_distances)
                        _dist_map[w] = _dist_map[_stack.back().first] + 1;
                    _stack.emplace_back(w, out_arcs(_graph, w));
                    remaining_arcs.advance();
                    return;
                }
            } else {
                for(auto & remaining_neighbors = _stack.back().second;
                    !remaining_neighbors.empty();
                    remaining_neighbors.advance()) {
                    auto w = remaining_neighbors.current();
                    if(_reached_map[w]) continue;
                    _reached_map[w] = true;
                    if constexpr(_Traits::store_pred_vertices)
                        _pred_vertices_map[w] = _stack.back().first;
                    if constexpr(_Traits::store_distances)
                        _dist_map[w] = _dist_map[_stack.back().first] + 1;
                    _stack.emplace_back(w, out_neighbors(_graph, w));
                    remaining_neighbors.advance();
                    return;
                }
            }
            _stack.pop_back();
        } while(!_stack.empty());
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }
    [[nodiscard]] constexpr auto begin() noexcept {
        return algorithm_iterator(*this);
    }
    [[nodiscard]] constexpr auto end() const noexcept {
        return std::default_sentinel;
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _reached_map[u];
    }
    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const noexcept
        requires(_Traits::store_pred_vertices)
    {
        assert(reached(u));
        return _pred_vertices_map[u];
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const noexcept
        requires(_Traits::store_pred_arcs)
    {
        assert(reached(u));
        return _pred_arcs_map[u];
    }
};

template <typename _Graph, typename _Traits = depth_first_search_default_traits>
depth_first_search(_Graph &&)
    -> depth_first_search<views::graph_all_t<_Graph>, _Traits>;

template <typename _Graph, typename _Traits = depth_first_search_default_traits>
depth_first_search(_Graph &&, const vertex_t<_Graph> &)
    -> depth_first_search<views::graph_all_t<_Graph>, _Traits>;

template <typename _Graph, typename _Traits>
depth_first_search(_Traits, _Graph &&)
    -> depth_first_search<views::graph_all_t<_Graph>, _Traits>;

template <typename _Graph, typename _Traits>
depth_first_search(_Traits, _Graph &&, const vertex_t<_Graph> &)
    -> depth_first_search<views::graph_all_t<_Graph>, _Traits>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_depth_first_search_HPP