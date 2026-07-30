#pragma once

#include <cassert>
#include <memory>
#include <ranges>
#include <span>
#include <vector>

#include "melon/detail/borrowed_graph.hpp"
#include "melon/detail/consumable_view.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/views/undirect.hpp"

namespace melon {

template <undirected_graph_view UGraph>
    requires has_incidence<UGraph> && has_vertex_map<UGraph>
class connected_components
    : public algorithm_view_interface<connected_components<UGraph>> {
private:
    using vertex = vertex_t<UGraph>;
    using cursor = std::conditional_t<has_num_vertices<UGraph>,
                                      typename std::vector<vertex>::iterator,
                                      typename std::vector<vertex>::size_type>;

private:
    UGraph _graph;
    consumable_input_view<vertices_range_t<UGraph>> _remaining_vertices;
    std::vector<vertex> _queue;
    cursor _queue_current;
    vertex_map_t<UGraph, bool> _reached_map;

public:
    template <typename UG>
        requires detail::not_self<UG, connected_components> &&
                     undirected_graph_for<UG, UGraph>
    constexpr explicit connected_components(UG && g)
        : _graph(views::undirected_graph_all(std::forward<UG>(g)))
        , _remaining_vertices(vertices(_graph))
        , _queue()
        , _reached_map(create_vertex_map<bool>(_graph, false)) {
        if constexpr(has_num_vertices<UGraph>) {
            _queue.reserve(num_vertices(_graph));
            _queue_current = _queue.begin();
        } else {
            _queue_current = 0;
        }
        // Guarded: advance() reads _remaining_vertices.current(), so on a graph
        // with no vertices the unconditional call read past the end of an empty
        // view (an assertion failure in a debug build, silent UB in a release
        // one). A vertex-less graph is finished() from the start.
        if(!finished()) advance();
    }

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    // Hand-written for the same reason as depth_first_search's move: a
    // memberwise move leaves the cached cursor's range pointing at the
    // moved-from object's _graph member. The queue's buffer transfers, so
    // _queue_current needs nothing.
    constexpr connected_components(const connected_components &) = delete;
    constexpr connected_components(connected_components && o)
        : _graph(std::move(o._graph))
        // Not a memberwise move: the cursor's move constructor reseeks
        // through the *old* range, whose predicate reads the graph member
        // moved away one line up. Built from the new graph's range and the
        // old consumed count instead.
        , _remaining_vertices([&]() -> decltype(_remaining_vertices) {
            if constexpr(!borrowed_graph<UGraph> &&
                         !std::ranges::borrowed_range<vertices_range_t<UGraph>>)
                return decltype(_remaining_vertices)(
                    vertices(_graph), o._remaining_vertices.consumed());
            else
                return std::move(o._remaining_vertices);
        }())
        , _queue(std::move(o._queue))
        , _queue_current(std::move(o._queue_current))
        , _reached_map(std::move(o._reached_map)) {}

    constexpr connected_components & operator=(const connected_components &) =
        delete;
    // See the move constructor.
    constexpr connected_components & operator=(connected_components && o) {
        if(this == std::addressof(o)) return *this;
        _graph = std::move(o._graph);
        if constexpr(!borrowed_graph<UGraph> &&
                     !std::ranges::borrowed_range<vertices_range_t<UGraph>>)
            _remaining_vertices = decltype(_remaining_vertices)(
                vertices(_graph), o._remaining_vertices.consumed());
        else
            _remaining_vertices = std::move(o._remaining_vertices);
        _queue = std::move(o._queue);
        _queue_current = std::move(o._queue_current);
        _reached_map = std::move(o._reached_map);
        return *this;
    }

    // The graph the algorithm runs over. An algorithm owns its view rather
    // than adapting it, so this is the std::ranges::owning_view shape --
    // references, ref-qualified -- and not the filter_view shape the graph
    // *views* use. Returning a copy here would also put traversal_forest back
    // where it started: it reaches its sources through base(), and an owned
    // graph view is move-only.
    [[nodiscard]] constexpr UGraph & base() & noexcept { return _graph; }
    [[nodiscard]] constexpr const UGraph & base() const & noexcept {
        return _graph;
    }
    [[nodiscard]] constexpr UGraph && base() && noexcept {
        return std::move(_graph);
    }
    [[nodiscard]] constexpr const UGraph && base() const && noexcept {
        return std::move(_graph);
    }

    // Restores the constructor's state, first component included: without the
    // advance() the queue stayed empty while finished() still said false, so
    // current() named nothing. _reset_current_vertex() asserts !finished(), so
    // it belongs inside the same guard as the advance().
    constexpr connected_components & reset() {
        _remaining_vertices = vertices(_graph);
        _queue.resize(0);
        _reached_map.fill(false);
        if(!finished()) {
            _reset_current_vertex();
            advance();
        }
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_remaining_vertices.empty())) {
        return _remaining_vertices.empty();
    }

    // The component is a window onto _queue, which the next advance()
    // rewrites -- hand it out read-only, and as std::span<const vertex>, the
    // type breadth_first_search::traversal() and the other component
    // algorithms' current() already settled on.
    [[nodiscard]] constexpr std::span<const vertex> current() const noexcept {
        assert(!finished());
        return std::span<const vertex>(_queue.data(), _queue.size());
    }

private:
    [[nodiscard]] constexpr bool _finished_component() const noexcept {
        if constexpr(has_num_vertices<UGraph>) {
            return _queue_current == _queue.end();
        } else {
            return _queue_current == _queue.size();
        }
    }
    [[nodiscard]] constexpr vertex & _current_vertex() noexcept {
        assert(!finished());
        if constexpr(has_num_vertices<UGraph>) {
            return *_queue_current;
        } else {
            return _queue[_queue_current];
        }
    }
    constexpr void _reset_current_vertex() noexcept {
        assert(!finished());
        if constexpr(has_num_vertices<UGraph>) {
            _queue_current = _queue.begin();
        } else {
            _queue_current = 0;
        }
    }

public:
    constexpr void advance() {
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
            // By reference only where _queue is reserved to num_vertices and so
            // cannot reallocate; without that reserve the push_backs below
            // would leave this reference dangling. Same shape as
            // breadth_first_search::advance().
            std::conditional_t<has_num_vertices<UGraph>, const vertex &, vertex>
                u = _current_vertex();
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

    [[nodiscard]] constexpr bool reached(const vertex & u) const
        noexcept(noexcept(_reached_map[u])) {
        return _reached_map[u];
    }
    // A view of the reached state, like breadth_first_search's and
    // depth_first_search's. It refers into the algorithm, as every melon map
    // view refers into what it names: it is valid while this object lives and
    // stays put, exactly the contract mapping_ref_view carries.
    [[nodiscard]] constexpr auto reached_map() const
        noexcept(noexcept(maps::mapping_all(_reached_map))) {
        return maps::mapping_all(_reached_map);
    }
};

template <typename UGraph>
connected_components(UGraph &&)
    -> connected_components<views::undirected_graph_all_t<UGraph>>;

// Constrained on what views::undirect actually needs -- the *incidence*
// concepts -- not on adjacency. The two are independent: a graph with
// adjacency but no incidence passed this constraint and then hard-errored
// inside undirect, and one with incidence but no adjacency was rejected
// although it works.
template <graph Graph>
    requires outward_incidence_graph<Graph> && inward_incidence_graph<Graph> &&
             has_vertex_map<Graph>
constexpr auto weakly_connected_components(Graph && g) {
    return connected_components(views::undirect(std::forward<Graph>(g)));
}

}  // namespace melon
