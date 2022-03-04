#ifndef MELON_BFS_HPP
#define MELON_BFS_HPP

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/utils/traversal_algorithm_behavior.hpp"
#include "melon/utils/traversal_algorithm_iterator.hpp"

namespace fhamonic {
namespace melon {

template <typename GR, std::underlying_type_t<TraversalAlgorithmBehavior> BH =
                           TraversalAlgorithmBehavior::TRACK_NONE>
class Bfs {
public:
    using vertex = GR::vertex;
    using arc = GR::arc;

    using ReachedMap = typename GR::vertexMap<bool>;

    static constexpr bool track_predecessor_vertices =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_NODES);
    static constexpr bool track_predecessor_arcs =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_ARCS);
    static constexpr bool track_distances =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_DISTANCES);

    using PredverticesMap =
        std::conditional<track_predecessor_vertices, typename GR::vertexMap<vertex>,
                         std::monostate>::type;
    using PredarcsMap =
        std::conditional<track_predecessor_arcs, typename GR::vertexMap<arc>,
                         std::monostate>::type;
    using DistancesMap =
        std::conditional<track_distances, typename GR::vertexMap<std::size_t>,
                         std::monostate>::type;

private:
    const GR & _graph;
    std::vector<vertex> _queue;
    std::vector<vertex>::iterator _queue_current;

    ReachedMap _reached_map;

    PredverticesMap _pred_vertices_map;
    PredarcsMap _pred_arcs_map;
    DistancesMap _dist_map;

public:
    Bfs(const GR & g) : _graph(g), _queue(), _reached_map(g.nb_vertices(), false) {
        _queue.reserve(g.nb_vertices());
        _queue_current = _queue.begin();
        if constexpr(track_predecessor_vertices)
            _pred_vertices_map.resize(g.nb_vertices());
        if constexpr(track_predecessor_arcs) _pred_arcs_map.resize(g.nb_vertices());
        if constexpr(track_distances) _dist_map.resize(g.nb_vertices());
    }

    Bfs & reset() noexcept {
        _queue.resize(0);
        _reached_map.fill(false);
        return *this;
    }
    Bfs & add_source(vertex s) noexcept {
        assert(!_reached_map[s]);
        push_node(s);
        if constexpr(track_predecessor_vertices) _pred_vertices_map[s] = s;
        if constexpr(track_distances) _dist_map[s] = 0;
        return *this;
    }

    bool empty_queue() const noexcept { return _queue_current == _queue.end(); }
    
private:
    void push_node(vertex u) noexcept {
        _queue.push_back(u);
        _reached_map[u] = true;
    }
    vertex pop_node() noexcept {
        const vertex u = *_queue_current;
        ++_queue_current;
        return u;
    }

public:
    vertex next_node() noexcept {
        const vertex u = pop_node();
        for(arc a : _graph.out_arcs(u)) {
            const vertex w = _graph.target(a);
            if(reached(w)) continue;
            push_node(w);
            if constexpr(track_predecessor_vertices) _pred_vertices_map[w] = u;
            if constexpr(track_predecessor_arcs) _pred_arcs_map[w] = a;
            if constexpr(track_distances) _dist_map[w] = _dist_map[u] + 1;
        }
        return u;
    }

    void run() noexcept {
        while(!empty_queue()) next_node();
    }
    auto begin() noexcept { return traversal_algorithm_iterator(*this); }
    auto end() noexcept { return traversal_algorithm_end_iterator(); }

    bool reached(const vertex u) const noexcept { return _reached_map[u]; }

    vertex pred_node(const vertex u) const noexcept
        requires(track_predecessor_vertices) {
        assert(reached(u));
        return _pred_vertices_map[u];
    }
    arc pred_arc(const vertex u) const noexcept requires(track_predecessor_arcs) {
        assert(reached(u));
        return _pred_arcs_map[u];
    }
    std::size_t dist(const vertex u) const noexcept requires(track_distances) {
        assert(reached(u));
        return _dist_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_BFS_HPP
