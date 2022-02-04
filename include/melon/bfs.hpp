#ifndef MELON_BFS_HPP
#define MELON_BFS_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <type_traits>  // underlying_type, conditional
#include <variant>      // monostate
#include <vector>

#include "melon/node_search_behavior.hpp"

namespace fhamonic {
namespace melon {

template <typename GR, std::underlying_type_t<NodeSeachBehavior> BH =
                           NodeSeachBehavior::TRACK_NONE>
class BFS {
public:
    using Node = GR::Node;
    using Arc = GR::Arc;

    using ReachedMap = typename GR::NodeMap<bool>;

    static constexpr bool track_predecessor_nodes =
        static_cast<bool>(BH & NodeSeachBehavior::TRACK_PRED_NODES);
    static constexpr bool track_predecessor_arcs =
        static_cast<bool>(BH & NodeSeachBehavior::TRACK_PRED_ARCS);
    static constexpr bool track_distances =
        static_cast<bool>(BH & NodeSeachBehavior::TRACK_DISTANCES);

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
    const GR & graph;
    std::vector<Node> queue;
    std::vector<Node>::iterator queue_current;

    ReachedMap reached_map;

    PredNodesMap pred_nodes_map;
    PredArcsMap pred_arcs_map;
    DistancesMap dist_map;

public:
    BFS(const GR & g) : graph(g), queue(), reached_map(g.nb_nodes(), false) {
        queue.reserve(g.nb_nodes());
        queue_current = queue.begin();
        if constexpr(track_predecessor_nodes)
            pred_nodes_map.resize(g.nb_nodes());
        if constexpr(track_predecessor_arcs) pred_arcs_map.resize(g.nb_nodes());
        if constexpr(track_distances) dist_map.resize(g.nb_nodes());
    }

    BFS & reset() noexcept {
        queue.resize(0);
        reached_map.fill(false);
        return *this;
    }
    BFS & addSource(Node s) noexcept {
        assert(!reached_map[s]);
        pushNode(s);
        if constexpr(track_predecessor_nodes) pred_nodes_map[s] = s;
        if constexpr(track_distances) dist_map[s] = 0;
        return *this;
    }

    bool emptyQueue() const noexcept { return queue_current == queue.end(); }
    void pushNode(Node u) noexcept {
        queue.push_back(u);
        reached_map[u] = true;
    }
    Node popNode() noexcept {
        Node u = *queue_current;
        ++queue_current;
        return u;
    }
    bool reached(const Node u) const noexcept { return reached_map[u]; }

    Node processNextNode() noexcept {
        const Node u = popNode();
        for(Arc a : graph.out_arcs(u)) {
            Node w = graph.target(a);
            if(reached_map[w]) continue;
            pushNode(w);
            if constexpr(track_predecessor_nodes) pred_nodes_map[w] = u;
            if constexpr(track_predecessor_arcs) pred_arcs_map[w] = a;
            if constexpr(track_distances) dist_map[w] = dist_map[u] + 1;
        }
        return u;
    }

    void run() noexcept {
        while(!emptyQueue()) {
            processNextNode();
        }
    }

    Node pred_node(const Node u) const noexcept
        requires(track_predecessor_nodes) {
        assert(reached(u));
        return pred_nodes_map[u];
    }
    Arc pred_arc(const Node u) const noexcept requires(track_predecessor_arcs) {
        assert(reached(u));
        return pred_arcs_map[u];
    }
    std::size_t dist(const Node u) const noexcept requires(track_distances) {
        assert(reached(u));
        return dist_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_BFS_HPP
