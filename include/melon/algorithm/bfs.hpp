#ifndef MELON_ALGORITHM_BFS_HPP
#define MELON_ALGORITHM_BFS_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/utils/traversal_algorithm_behavior.hpp"
#include "melon/utils/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

template <typename GR, std::underlying_type_t<TraversalAlgorithmBehavior> BH =
                           TraversalAlgorithmBehavior::TRACK_NONE>
class Bfs {
public:
    using vertex_t = GR::vertex_t;
    using arc_t = GR::arc_t;

    using ReachedMap = typename GR::vertex_map<bool>;

    static constexpr bool track_predecessor_vertices =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_NODES);
    static constexpr bool track_predecessor_arcs =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_ARCS);
    static constexpr bool track_distances =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_DISTANCES);

    using PredverticesMap = std::conditional<track_predecessor_vertices,
                                             typename GR::vertex_map<vertex_t>,
                                             std::monostate>::type;
    using PredarcsMap =
        std::conditional<track_predecessor_arcs, typename GR::vertex_map<arc_t>,
                         std::monostate>::type;
    using DistancesMap =
        std::conditional<track_distances, typename GR::vertex_map<std::size_t>,
                         std::monostate>::type;

private:
    const GR & _graph;
    std::vector<vertex_t> _queue;
    std::vector<vertex_t>::iterator _queue_current;

    ReachedMap _reached_map;

    PredverticesMap _pred_vertices_map;
    PredarcsMap _pred_arcs_map;
    DistancesMap _dist_map;

public:
    Bfs(const GR & g)
        : _graph(g), _queue(), _reached_map(g.nb_vertices(), false) {
        _queue.reserve(g.nb_vertices());
        _queue_current = _queue.begin();
        if constexpr(track_predecessor_vertices)
            _pred_vertices_map.resize(g.nb_vertices());
        if constexpr(track_predecessor_arcs)
            _pred_arcs_map.resize(g.nb_vertices());
        if constexpr(track_distances) _dist_map.resize(g.nb_vertices());
    }

    Bfs & reset() noexcept {
        _queue.resize(0);
        _reached_map.fill(false);
        return *this;
    }
    Bfs & add_source(vertex_t s) noexcept {
        assert(!_reached_map[s]);
        push_node(s);
        if constexpr(track_predecessor_vertices) _pred_vertices_map[s] = s;
        if constexpr(track_distances) _dist_map[s] = 0;
        return *this;
    }

    bool empty_queue() const noexcept { return _queue_current == _queue.end(); }

private:
    void push_node(vertex_t u) noexcept {
        _queue.push_back(u);
        _reached_map[u] = true;
    }
    vertex_t pop_node() noexcept {
        const vertex_t u = *_queue_current;
        ++_queue_current;
        return u;
    }

public:
    vertex_t next_vertex() noexcept {
        const vertex_t u = pop_node();
        for(arc_t a : _graph.out_arcs(u)) {
            const vertex_t w = _graph.target(a);
            if(reached(w)) continue;
            push_node(w);
            if constexpr(track_predecessor_vertices) _pred_vertices_map[w] = u;
            if constexpr(track_predecessor_arcs) _pred_arcs_map[w] = a;
            if constexpr(track_distances) _dist_map[w] = _dist_map[u] + 1;
        }
        return u;
    }

    void run() noexcept {
        while(!empty_queue()) next_vertex();
    }
    auto begin() noexcept { return traversal_iterator(*this); }
    auto end() noexcept { return traversal_end_sentinel(); }

    bool reached(const vertex_t u) const noexcept { return _reached_map[u]; }

    vertex_t pred_vertex(const vertex_t u) const noexcept
        requires(track_predecessor_vertices) {
        assert(reached(u));
        return _pred_vertices_map[u];
    }
    arc_t pred_arc(const vertex_t u) const noexcept
        requires(track_predecessor_arcs) {
        assert(reached(u));
        return _pred_arcs_map[u];
    }
    std::size_t dist(const vertex_t u) const noexcept
        requires(track_distances) {
        assert(reached(u));
        return _dist_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_BFS_HPP
