#ifndef MELON_ALGORITHM_TOPOLOGICAL_SORT_HPP
#define MELON_ALGORITHM_TOPOLOGICAL_SORT_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/detail/map_if.hpp"
#include "melon/graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace fhamonic {
namespace melon {

struct topological_sort_default_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
};

template <graph _Graph, typename _Traits = topological_sort_default_traits>
    requires outward_incidence_graph<_Graph> && has_vertex_map<_Graph>
class topological_sort {
public:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;

    static_assert(
        !(outward_adjacency_graph<_Graph> && _Traits::store_pred_arcs),
        "traversal on outward_adjacency_list cannot access predecessor arcs.");

    using reached_map = vertex_map_t<_Graph, bool>;
    using remaining_in_degree_map = vertex_map_t<_Graph, std::size_t>;

private:
    _Graph _graph;
    std::vector<vertex> _queue;
    std::vector<vertex>::iterator _queue_current;
    reached_map _reached_map;
    remaining_in_degree_map _remaining_in_degree_map;

    [[no_unique_address]] vertex_map_if<_Traits::store_pred_vertices, _Graph,
                                        vertex> _pred_vertices_map;
    [[no_unique_address]] vertex_map_if<_Traits::store_pred_arcs, _Graph, arc>
        _pred_arcs_map;
    [[no_unique_address]] vertex_map_if<_Traits::store_distances, _Graph, int>
        _dist_map;

    constexpr void push_start_vertices() noexcept {
        _queue.resize(0);
        _queue_current = _queue.begin();
        _reached_map.fill(false);
        if(has_in_degree<_Graph>) {
            for(auto && u : vertices(_graph)) {
                _remaining_in_degree_map[u] = in_degree(_graph, u);
                if(_remaining_in_degree_map[u] == 0) {
                    _queue.push_back(u);
                }
            }
        } else {
            _remaining_in_degree_map.fill(0);
            for(auto && u : vertices(_graph)) {
                for(auto && a : out_arcs(_graph, u)) {
                    const vertex & w = arc_target(_graph, a);
                    ++_remaining_in_degree_map[w];
                }
            }
            for(auto && u : vertices(_graph)) {
                if(_remaining_in_degree_map[u] == 0) {
                    _queue.push_back(u);
                }
            }
        }
        if constexpr(_Traits::store_distances) _dist_map.fill(0);
    }

public:
    template <typename _G>
    [[nodiscard]] constexpr explicit topological_sort(_G && g)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _queue()
        , _reached_map(create_vertex_map<bool>(g, false))
        , _remaining_in_degree_map(create_vertex_map<long unsigned int>(
              g, std::numeric_limits<unsigned int>::max()))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _dist_map(_graph) {
        _queue.reserve(num_vertices(g));
        push_start_vertices();
    }

    [[nodiscard]] constexpr topological_sort(const topological_sort & bin) =
        default;
    [[nodiscard]] constexpr topological_sort(topological_sort && bin) = default;

    constexpr topological_sort & operator=(const topological_sort &) = default;
    constexpr topological_sort & operator=(topological_sort &&) = default;

public:
    constexpr topological_sort & reset() noexcept {
        _queue.resize(0);
        _queue_current = _queue.begin();
        _reached_map.fill(false);
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _queue_current == _queue.end();
    }

    [[nodiscard]] constexpr vertex current() const noexcept {
        assert(!finished());
        return *_queue_current;
    }

    constexpr void advance() noexcept {
        assert(!finished());
        const vertex & u = *_queue_current;
        ++_queue_current;
        for(auto && a : out_arcs(_graph, u)) {
            const vertex & w = arc_target(_graph, a);
            if(--_remaining_in_degree_map[w] > 0) continue;
            _queue.push_back(w);
            if constexpr(_Traits::store_pred_vertices)
                _pred_vertices_map[w] = u;
            if constexpr(_Traits::store_pred_arcs) _pred_arcs_map[w] = a;
            if constexpr(_Traits::store_distances)
                _dist_map[w] = _dist_map[u] + 1;
        }
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
    [[nodiscard]] constexpr int dist(const vertex & u) const noexcept
        requires(_Traits::store_distances)
    {
        assert(reached(u));
        return _dist_map[u];
    }
};

template <typename _Graph, typename _Traits = topological_sort_default_traits>
topological_sort(_Graph &&)
    -> topological_sort<views::graph_all_t<_Graph>, _Traits>;

template <typename _Graph, typename _Traits>
topological_sort(_Traits, _Graph &&)
    -> topological_sort<views::graph_all_t<_Graph>, _Traits>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_TOPOLOGICAL_SORT_HPP
