#ifndef MELON_DFS_HPP
#define MELON_DFS_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <stack>
#include <type_traits>  // underlying_type, conditional
#include <variant>      // monostate
#include <vector>

namespace fhamonic {
namespace melon {

// TODO ranges , requires out_arcs : borrowed_range
template <typename GR, std::underlying_type_t<NodeSeachBehavior> BH =
                           NodeSeachBehavior::TRACK_NONE>
class DFS {
public:
    using Node = GR::Node;
    using Arc = GR::Arc;

    using ReachedMap = typename GR::NodeMap<bool>;

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

    using OutArcRange = decltype(std::declval<GR>().out_arcs(Node()));
    using OutArcIt = decltype(std::declval<OutArcRange>().begin());
    using OutArcItEnd = decltype(std::declval<OutArcRange>().end());

    static_assert(std::ranges::borrowed_range<OutArcRange>);
    std::vector<std::pair<OutArcIt, OutArcItEnd>> stack;

    ReachedMap reached_map;

    PredNodesMap pred_nodes_map;
    PredArcsMap pred_arcs_map;

public:
    DFS(const GR & g) : graph(g), stack(), reached_map(g.nb_nodes(), false) {
        stack.reserve(g.nb_nodes());
        if constexpr(track_predecessor_nodes)
            pred_nodes_map.resize(g.nb_nodes());
        if constexpr(track_predecessor_arcs) pred_arcs_map.resize(g.nb_nodes());
    }

    DFS & reset() noexcept {
        stack.resize(0);
        reached_map.fill(false);
        return *this;
    }
    DFS & addSource(Node s) noexcept {
        assert(!reached_map[s]);
        pushNode(s);
        if constexpr(track_predecessor_nodes) pred_nodes_map[s] = s;
        return *this;
    }

    bool emptyQueue() const noexcept { return stack.empty(); }
    void pushNode(Node u) noexcept {
        OutArcRange r = graph.out_arcs(u);
        stack.emplace_back(r.begin(), r.end());
        reached_map[u] = true;
    }
    bool reached(const Node u) const noexcept { return reached_map[u]; }

private:
    void advance_iterators() {
        assert(!stack.empty());
        do {
            while(stack.back().first != stack.back().second) {
                if(!reached(graph.target(*stack.back().first))) return;
                ++stack.back().first;
            }
            stack.pop_back();
        } while(!stack.empty());
    }

public:
    std::pair<Arc, Node> processNextNode() noexcept {
        Arc a = *stack.back().first;
        Node u = graph.target(a);
        pushNode(u);
        // if constexpr(track_predecessor_nodes) pred_nodes_map[u] = u;
        if constexpr(track_predecessor_arcs) pred_arcs_map[u] = a;
        advance_iterators();
        return std::make_pair(a, u);
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
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DFS_HPP