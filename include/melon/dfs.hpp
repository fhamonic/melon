#ifndef MELON_DFS_HPP
#define MELON_DFS_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <type_traits>  // underlying_type, conditional
#include <variant>      // monostate
#include <stack>
#include <vector>

#include "melon/node_search_behavior.hpp"

namespace fhamonic {
namespace melon {

template <typename GR, std::underlying_type_t<NodeSeachBehavior> BH =
                           NodeSeachBehavior::TRACK_NONE>
class DFS {
public:
    using Node = GR::Node;
    using Arc = GR::Arc;

    using VisitedMap = typename GR::NodeMap<bool>;

    static constexpr bool track_predecessor_nodes =
        static_cast<bool>(BH & NodeSeachBehavior::TRACK_PRED_NODES);
    static constexpr bool track_predecessor_arcs =
        static_cast<bool>(BH & NodeSeachBehavior::TRACK_PRED_ARCS);

    using PredNodesMap =
        std::conditional<track_predecessor_nodes, typename GR::NodeMap<Node>,
                         std::monostate>::type;
    using PredArcsMap =
        std::conditional<track_predecessor_arcs, typename GR::NodeMap<Arc>,
                         std::monostate>::type;

private:
    const GR & graph;
    std::vector<Node> stack;

    VisitedMap stacked_map;

    PredNodesMap pred_nodes_map;
    PredArcsMap pred_arcs_map;

public:
    DFS(const GR & g)
        : graph(g)
        , stack()
        , stacked_map(g.nb_nodes(), false) {
        if constexpr(track_predecessor_nodes)
            pred_nodes_map.resize(g.nb_nodes());
        if constexpr(track_predecessor_arcs) pred_arcs_map.resize(g.nb_nodes());
    }

    DFS & reset() noexcept {
        stack.clear();
        std::ranges::fill(stacked_map, false);
        return *this;
    }
    DFS & addSource(Node s) noexcept {
        assert(!stacked_map[s]);
        pushNode(s);
        if constexpr(track_predecessor_nodes) pred_nodes_map[s] = s;
        return *this;
    }

    bool emptyQueue() const noexcept { return stack.empty(); }
    void pushNode(Node u) noexcept {
        stack.push_back(u);
        stacked_map[u] = true;
    }
    Node popNode() noexcept {
        Node u = stack.back();
        stack.pop_back();
        return u;
    }

    Node processNextNode() noexcept {
        const Node u = popNode();
        for(Arc a : graph.out_arcs(u)) {
            Node w = graph.target(a);
            if(stacked_map[w]) continue;
            pushNode(w);
            if constexpr(track_predecessor_nodes) pred_nodes_map[w] = u;
            if constexpr(track_predecessor_arcs) pred_arcs_map[w] = a;
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
        assert(stacked_map[u]);
        return pred_nodes_map[u];
    }
    Arc pred_arc(const Node u) const noexcept requires(track_predecessor_arcs) {
        assert(stacked_map[u]);
        return pred_arcs_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DFS_HPP
