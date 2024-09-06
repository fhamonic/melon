#ifndef MELON_ALGORITHM_BFS_HPP
#define MELON_ALGORITHM_BFS_HPP

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

struct breadth_first_search_default_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
};

template <outward_adjacency_graph _Graph,
          typename _Traits = breadth_first_search_default_traits>
    requires has_vertex_map<_Graph>
class breadth_first_search {
private:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;

    static_assert(!_Traits::store_pred_arcs || outward_incidence_graph<_Graph>,
                  "storing predecessor arcs requires outward_incidence_graph.");

    using cursor =
        std::conditional_t<has_num_vertices<_Graph>,
                           typename std::vector<vertex>::iterator, int>;

private:
    _Graph _graph;
    std::vector<vertex> _queue;
    cursor _queue_current;
    vertex_map_t<_Graph, bool> _reached_map;

    [[no_unique_address]] vertex_map_if<_Traits::store_pred_vertices, _Graph,
                                        vertex> _pred_vertices_map;
    [[no_unique_address]] vertex_map_if<_Traits::store_pred_arcs, _Graph, arc>
        _pred_arcs_map;
    [[no_unique_address]] vertex_map_if<_Traits::store_distances, _Graph, int>
        _dist_map;

public:
    template <typename _G>
    [[nodiscard]] constexpr explicit breadth_first_search(_G && g)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _queue()
        , _reached_map(create_vertex_map<bool>(_graph, false))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _dist_map(_graph) {
        if constexpr(has_num_vertices<_Graph>) {
            _queue.reserve(num_vertices(_graph));
            _queue_current = _queue.begin();
        } else {
            _queue_current = 0;
        }
    }

    template <typename _G>
    [[nodiscard]] constexpr breadth_first_search(_G && g, const vertex & s)
        : breadth_first_search(std::forward<_G>(g)) {
        add_source(s);
    }

    [[nodiscard]] constexpr breadth_first_search(const breadth_first_search &) =
        default;
    [[nodiscard]] constexpr breadth_first_search(breadth_first_search &&) =
        default;

    constexpr breadth_first_search & operator=(const breadth_first_search &) =
        default;
    constexpr breadth_first_search & operator=(breadth_first_search &&) =
        default;

    constexpr breadth_first_search & reset() noexcept {
        _queue.resize(0);
        if constexpr(has_num_vertices<_Graph>) {
            _queue_current = _queue.begin();
        } else {
            _queue_current = 0;
        }
        _reached_map.fill(false);
        return *this;
    }
    constexpr breadth_first_search & add_source(const vertex & s) noexcept {
        assert(!_reached_map[s]);
        _queue.push_back(s);
        _reached_map[s] = true;
        if constexpr(_Traits::store_pred_vertices) _pred_vertices_map[s] = s;
        if constexpr(_Traits::store_distances) _dist_map[s] = 0;
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        if constexpr(has_num_vertices<_Graph>) {
            return _queue_current == _queue.end();
        } else {
            return _queue_current == _queue.size();
        }
    }

    [[nodiscard]] constexpr const vertex & current() const noexcept {
        assert(!finished());
        if constexpr(has_num_vertices<_Graph>) {
            return *_queue_current;
        } else {
            return _queue[_queue_current];
        }
    }

    constexpr void advance() noexcept {
        assert(!finished());
        const vertex & u = current();
        ++_queue_current;
        if constexpr(_Traits::store_pred_arcs) {
            for(auto && a : out_arcs(_graph, u)) {
                const vertex & w = arc_target(_graph, a);
                if(_reached_map[w]) continue;
                _queue.push_back(w);
                _reached_map[w] = true;
                _pred_arcs_map[w] = a;
                if constexpr(_Traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(_Traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        } else {
            for(auto && w : out_neighbors(_graph, u)) {
                if(_reached_map[w]) continue;
                _queue.push_back(w);
                _reached_map[w] = true;
                if constexpr(_Traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(_Traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        }
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }
    [[nodiscard]] constexpr auto begin() noexcept {
        return algorithm_iterator(*this);
    }
    [[nodiscard]] constexpr auto end() const noexcept {
        return algorithm_end_sentinel();
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

template <typename _Graph,
          typename _Traits = breadth_first_search_default_traits>
breadth_first_search(_Graph &&)
    -> breadth_first_search<views::graph_all_t<_Graph>, _Traits>;

template <typename _Graph,
          typename _Traits = breadth_first_search_default_traits>
breadth_first_search(_Graph &&, const vertex_t<_Graph> &)
    -> breadth_first_search<views::graph_all_t<_Graph>, _Traits>;

template <typename _Graph, typename _Traits>
breadth_first_search(_Traits, _Graph &&)
    -> breadth_first_search<views::graph_all_t<_Graph>, _Traits>;

template <typename _Graph, typename _Traits>
breadth_first_search(_Traits, _Graph &&, const vertex_t<_Graph> &)
    -> breadth_first_search<views::graph_all_t<_Graph>, _Traits>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_BFS_HPP
