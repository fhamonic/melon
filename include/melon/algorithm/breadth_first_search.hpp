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
    static constexpr bool store_traversal_range = false;
};

namespace __detail {
template <typename _Graph, typename _Traits>
concept enable_branchless_bfs =
    has_num_vertices<_Graph> &&
    std::is_trivially_copyable_v<vertex_t<_Graph>> &&
    (!_Traits::store_pred_vertices && !_Traits::store_pred_arcs &&
     !_Traits::store_distances);
}

template <typename _Graph,
          typename _Traits = breadth_first_search_default_traits>
struct breadth_first_search;

template <outward_adjacency_graph _Graph, typename _Traits>
    requires has_vertex_map<_Graph> &&
             (!__detail::enable_branchless_bfs<_Graph, _Traits>)
class breadth_first_search<_Graph, _Traits>
    : public algorithm_view_interface<breadth_first_search<_Graph, _Traits>> {
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
    [[no_unique_address]] std::conditional_t<_Traits::store_traversal_range,
                                             cursor, std::monostate>
        _queue_traversal_begin;
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

    template <typename... _Args>
    [[nodiscard]] constexpr breadth_first_search(_Traits, _Args &&... args)
        : breadth_first_search(std::forward<_Args>(args)...) {}

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
        if constexpr(_Traits::store_traversal_range)
            _queue_traversal_begin = _queue_current;
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
    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _reached_map[u];
    }
    [[nodiscard]] constexpr auto reached_map() const noexcept {
        return views::mapping_all(_reached_map);
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
    [[nodiscard]] constexpr auto traversal() const noexcept
        requires(_Traits::store_traversal_range)
    {
        if constexpr(has_num_vertices<_Graph>) {
            return std::ranges::subrange(_queue_traversal_begin,
                                         _queue_current);
        } else {
            return std::ranges::subrange(
                _queue.begin() + _queue_traversal_begin,
                _queue.begin() + _queue_current);
        }
    }
};

template <outward_adjacency_graph _Graph, typename _Traits>
    requires has_vertex_map<_Graph> &&
             __detail::enable_branchless_bfs<_Graph, _Traits>
class breadth_first_search<_Graph, _Traits>
    : public algorithm_view_interface<breadth_first_search<_Graph, _Traits>> {
private:
    using vertex = vertex_t<_Graph>;

    _Graph _graph;
    std::unique_ptr<vertex[]> _queue;
    vertex * _queue_traversal_begin;
    vertex * _queue_traversal_end;
    vertex * _queue_current;
    vertex_map_t<_Graph, bool> _reached_map;

public:
    template <typename _G>
    [[nodiscard]] constexpr explicit breadth_first_search(_G && g)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _queue(std::make_unique_for_overwrite<vertex[]>(num_vertices(_graph) +
                                                          1))
        , _queue_traversal_begin(_queue.get())
        , _queue_traversal_end(_queue.get())
        , _queue_current(_queue.get())
        , _reached_map(create_vertex_map<bool>(_graph, false)) {}

    template <typename _G>
    [[nodiscard]] constexpr breadth_first_search(_G && g, const vertex & s)
        : breadth_first_search(std::forward<_G>(g)) {
        add_source(s);
    }

    template <typename... _Args>
    [[nodiscard]] constexpr breadth_first_search(_Traits, _Args &&... args)
        : breadth_first_search(std::forward<_Args>(args)...) {}

    [[nodiscard]] constexpr breadth_first_search(const breadth_first_search & o)
        : _graph(o._graph)
        , _queue(std::make_unique_for_overwrite<vertex[]>(num_vertices(_graph) +
                                                          1ul))
        , _queue_traversal_begin(_queue.get() +
                                 (o._queue_traversal_begin - o._queue.get()))
        , _queue_traversal_end(_queue.get() +
                               (o._queue_traversal_end - o._queue.get()))
        , _queue_current(_queue.get() + (o._queue_current - o._queue.get()))
        , _reached_map(o._reached_map) {
        std::copy(o._queue_traversal_begin, o._queue_traversal_end,
                  _queue_traversal_begin);
    }
    [[nodiscard]] constexpr breadth_first_search(breadth_first_search &&) =
        default;

    constexpr breadth_first_search & operator=(const breadth_first_search & o) {
        _graph = o._graph;
        _queue =
            std::make_unique_for_overwrite<vertex[]>(num_vertices(_graph) + 1);
        _queue_traversal_begin =
            _queue.get() + (o._queue_traversal_begin - o._queue.get());
        _queue_current = _queue.get() + (o._queue_current - o._queue.get());
        _queue_traversal_end =
            _queue.get() + (o._queue_traversal_end - o._queue.get());
        _reached_map = o._reached_map;
        std::copy(o._queue_traversal_begin, o._queue_traversal_end,
                  _queue_traversal_begin);
    }
    constexpr breadth_first_search & operator=(breadth_first_search &&) =
        default;

    constexpr breadth_first_search & reset() noexcept {
        _queue_traversal_begin = _queue_current = _queue_traversal_end =
            _queue.get();
        _reached_map.fill(false);
        return *this;
    }
    constexpr breadth_first_search & add_source(const vertex & s) noexcept {
        assert(!_reached_map[s]);
        _queue_traversal_begin = _queue_current;
        *_queue_traversal_end = s;
        ++_queue_traversal_end;
        _reached_map[s] = true;
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _queue_current == _queue_traversal_end;
    }

    [[nodiscard]] constexpr const vertex & current() const noexcept {
        assert(!finished());
        return *_queue_current;
    }
    constexpr void advance() noexcept {
        assert(!finished());
        const vertex & u = current();
        ++_queue_current;
        for(auto && w : out_neighbors(_graph, u)) {
            *_queue_traversal_end = w;
            _queue_traversal_end += !_reached_map[w];
            _reached_map[w] = true;
        }
    }
    constexpr void run() noexcept {
        while(!finished()) advance();
    }
    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _reached_map[u];
    }
    [[nodiscard]] constexpr auto reached_map() const noexcept {
        return views::mapping_all(_reached_map);
    }
    [[nodiscard]] constexpr auto traversal() const noexcept {
        return std::span(_queue_traversal_begin, _queue_current);
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
