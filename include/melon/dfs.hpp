#ifndef MELON_DFS_HPP
#define MELON_DFS_HPP

#include <algorithm>
#include <ranges>
#include <stack>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/utils/traversal_algorithm_behavior.hpp"
#include "melon/utils/traversal_algorithm_iterator.hpp"

namespace fhamonic {
namespace melon {

// TODO ranges , requires out_arcs : borrowed_range
template <typename GR, std::underlying_type_t<TraversalAlgorithmBehavior> BH =
                           TraversalAlgorithmBehavior::TRACK_NONE>
class Dfs {
public:
    using Node = GR::Node;
    using Arc = GR::Arc;

    using ReachedMap = typename GR::NodeMap<bool>;

    static constexpr bool track_predecessor_nodes =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_NODES);
    static constexpr bool track_predecessor_arcs =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_ARCS);

    using PredNodesMap =
        std::conditional<track_predecessor_nodes, typename GR::NodeMap<Node>,
                         std::monostate>::type;
    using PredArcsMap =
        std::conditional<track_predecessor_arcs, typename GR::NodeMap<Arc>,
                         std::monostate>::type;

private:
    const GR & _graph;

    using OutArcRange = decltype(std::declval<GR>().out_arcs(Node()));
    using OutArcIt = decltype(std::declval<OutArcRange>().begin());
    using OutArcItEnd = decltype(std::declval<OutArcRange>().end());

    static_assert(std::ranges::borrowed_range<OutArcRange>);
    std::vector<std::pair<OutArcIt, OutArcItEnd>> _stack;

    ReachedMap _reached_map;

    PredNodesMap _pred_nodes_map;
    PredArcsMap _pred_arcs_map;

public:
    Dfs(const GR & g) : _graph(g), _stack(), _reached_map(g.nb_nodes(), false) {
        _stack.reserve(g.nb_nodes());
        if constexpr(track_predecessor_nodes)
            _pred_nodes_map.resize(g.nb_nodes());
        if constexpr(track_predecessor_arcs) _pred_arcs_map.resize(g.nb_nodes());
    }

    Dfs & reset() noexcept {
        _stack.resize(0);
        _reached_map.fill(false);
        return *this;
    }
    Dfs & add_source(Node s) noexcept {
        assert(!_reached_map[s]);
        push_node(s);
        if constexpr(track_predecessor_nodes) _pred_nodes_map[s] = s;
        return *this;
    }

    bool empty_queue() const noexcept { return _stack.empty(); }
    void push_node(Node u) noexcept {
        OutArcRange r = _graph.out_arcs(u);
        _stack.emplace_back(r.begin(), r.end());
        _reached_map[u] = true;
    }

private:
    void advance_iterators() {
        assert(!_stack.empty());
        do {
            while(_stack.back().first != _stack.back().second) {
                if(!reached(_graph.target(*_stack.back().first))) return;
                ++_stack.back().first;
            }
            _stack.pop_back();
        } while(!_stack.empty());
    }

public:
    std::pair<Arc, Node> next_node() noexcept {
        const Arc a = *_stack.back().first;
        const Node u = _graph.target(a);
        push_node(u);
        // if constexpr(track_predecessor_nodes) _pred_nodes_map[u] = u;
        if constexpr(track_predecessor_arcs) _pred_arcs_map[u] = a;
        advance_iterators();
        return std::make_pair(a, u);
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
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DFS_HPP