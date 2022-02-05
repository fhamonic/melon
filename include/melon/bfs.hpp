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
    using Node = GR::Node;
    using Arc = GR::Arc;

    using ReachedMap = typename GR::NodeMap<bool>;

    static constexpr bool track_predecessor_nodes =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_NODES);
    static constexpr bool track_predecessor_arcs =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_ARCS);
    static constexpr bool track_distances =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_DISTANCES);

    using PredNodesMap =
        std::conditional<track_predecessor_nodes, typename GR::NodeMap<Node>,
                         std::monostate>::type;
    using PredArcsMap =
        std::conditional<track_predecessor_arcs, typename GR::NodeMap<Arc>,
                         std::monostate>::type;
    using DistancesMap =
        std::conditional<track_distances, typename GR::NodeMap<std::size_t>,
                         std::monostate>::type;

private:
    const GR & _graph;
    std::vector<Node> _queue;
    std::vector<Node>::iterator _queue_current;

    ReachedMap _reached_map;

    PredNodesMap _pred_nodes_map;
    PredArcsMap _pred_arcs_map;
    DistancesMap _dist_map;

public:
    Bfs(const GR & g) : _graph(g), _queue(), _reached_map(g.nb_nodes(), false) {
        _queue.reserve(g.nb_nodes());
        _queue_current = _queue.begin();
        if constexpr(track_predecessor_nodes)
            _pred_nodes_map.resize(g.nb_nodes());
        if constexpr(track_predecessor_arcs) _pred_arcs_map.resize(g.nb_nodes());
        if constexpr(track_distances) _dist_map.resize(g.nb_nodes());
    }

    Bfs & reset() noexcept {
        _queue.resize(0);
        _reached_map.fill(false);
        return *this;
    }
    Bfs & add_source(Node s) noexcept {
        assert(!_reached_map[s]);
        push_node(s);
        if constexpr(track_predecessor_nodes) _pred_nodes_map[s] = s;
        if constexpr(track_distances) _dist_map[s] = 0;
        return *this;
    }

    bool empty_queue() const noexcept { return _queue_current == _queue.end(); }
    
private:
    void push_node(Node u) noexcept {
        _queue.push_back(u);
        _reached_map[u] = true;
    }
    Node pop_node() noexcept {
        Node u = *_queue_current;
        ++_queue_current;
        return u;
    }

public:
    Node next_node() noexcept {
        const Node u = pop_node();
        for(Arc a : _graph.out_arcs(u)) {
            Node w = _graph.target(a);
            if(_reached_map[w]) continue;
            push_node(w);
            if constexpr(track_predecessor_nodes) _pred_nodes_map[w] = u;
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

    bool reached(const Node u) const noexcept { return _reached_map[u]; }

    Node pred_node(const Node u) const noexcept
        requires(track_predecessor_nodes) {
        assert(reached(u));
        return _pred_nodes_map[u];
    }
    Arc pred_arc(const Node u) const noexcept requires(track_predecessor_arcs) {
        assert(reached(u));
        return _pred_arcs_map[u];
    }
    std::size_t dist(const Node u) const noexcept requires(track_distances) {
        assert(reached(u));
        return _dist_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_BFS_HPP
