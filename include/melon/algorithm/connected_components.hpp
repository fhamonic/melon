#ifndef MELON_ALGORITHM_CONNECTED_COMPONENTS_HPP
#define MELON_ALGORITHM_CONNECTED_COMPONENTS_HPP

#include <cassert>
#include <ranges>
#include <vector>

#include "melon/detail/consumable_view.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/views/undirect.hpp"

namespace fhamonic {
namespace melon {

template <undirected_graph _UGraph>
    requires has_incidence<_UGraph> && has_vertex_map<_UGraph>
class connected_components
    : public algorithm_view_interface<connected_components<_UGraph>> {
private:
    using vertex = vertex_t<_UGraph>;
    using cursor =
        std::conditional_t<has_num_vertices<_UGraph>,
                           typename std::vector<vertex>::iterator, int>;

private:
    _UGraph _graph;
    consumable_view<vertices_range_t<_UGraph>> _remaining_vertices;
    std::vector<vertex> _queue;
    cursor _queue_current;
    vertex_map_t<_UGraph, bool> _reached_map;

public:
    template <typename _UG>
    [[nodiscard]] constexpr explicit connected_components(_UG && g) noexcept
        : _graph(views::undirected_graph_all(std::forward<_UG>(g)))
        , _remaining_vertices(vertices(_graph))
        , _queue()
        , _reached_map(create_vertex_map<bool>(_graph, false)) {
        if constexpr(has_num_vertices<_UGraph>) {
            _queue.reserve(num_vertices(_graph));
            _queue_current = _queue.begin();
        } else {
            _queue_current = 0;
        }
        advance();
    }

    [[nodiscard]] constexpr connected_components(const connected_components &) =
        default;
    [[nodiscard]] constexpr connected_components(connected_components &&) =
        default;

    constexpr connected_components & operator=(const connected_components &) =
        default;
    constexpr connected_components & operator=(connected_components &&) =
        default;

    constexpr connected_components & reset() noexcept {
        _remaining_vertices = vertices(_graph);
        _queue.resize(0);
        _reset_current_vertex();
        _reached_map.fill(false);
        return *this;
    }

    [[nodiscard]] constexpr bool finished() noexcept {
        return _remaining_vertices.empty();
    }

    [[nodiscard]] constexpr auto current() noexcept {
        assert(!finished());
        return std::views::all(_queue);
    }

private:
    [[nodiscard]] constexpr bool _finished_component() const noexcept {
        if constexpr(has_num_vertices<_UGraph>) {
            return _queue_current == _queue.end();
        } else {
            return _queue_current == _queue.size();
        }
    }
    [[nodiscard]] constexpr vertex & _current_vertex() noexcept {
        assert(!finished());
        if constexpr(has_num_vertices<_UGraph>) {
            return *_queue_current;
        } else {
            return _queue[_queue_current];
        }
    }
    constexpr void _reset_current_vertex() noexcept {
        assert(!finished());
        if constexpr(has_num_vertices<_UGraph>) {
            _queue_current = _queue.begin();
        } else {
            _queue_current = 0;
        }
    }

public:
    constexpr void advance() noexcept {
        assert(_finished_component());
        assert(!finished());
        while(_reached_map[_remaining_vertices.current()]) {
            _remaining_vertices.advance();
            if(_remaining_vertices.empty()) return;
        }
        _queue.resize(1);
        _reset_current_vertex();
        _reached_map[_current_vertex() = _remaining_vertices.current()] = true;
        while(!_finished_component()) {
            const vertex & u = _current_vertex();
            // for(const auto & w : adjacency(_graph, u)) {
            //     if(_reached_map[w]) continue;
            //     _queue.push_back(w);
            //     _reached_map[w] = true;
            // }
            for(const auto & [a, w] : incidence(_graph, u)) {
                if(_reached_map[w]) continue;
                _queue.push_back(w);
                _reached_map[w] = true;
            }
            ++_queue_current;
        }
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _reached_map[u];
    }
};

template <typename _UGraph>
connected_components(_UGraph &&)
    -> connected_components<views::undirected_graph_all_t<_UGraph>>;

template <graph _Graph>
    requires outward_adjacency_graph<_Graph> && inward_adjacency_graph<_Graph>
constexpr auto weakly_connected_components(_Graph && g) {
    return connected_components(views::undirect(std::forward<_Graph>(g)));
}

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_CONNECTED_COMPONENTS_HPP