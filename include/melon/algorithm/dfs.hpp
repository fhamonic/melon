#ifndef MELON_ALGORITHM_DFS_HPP
#define MELON_ALGORITHM_DFS_HPP

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
    using vertex = GR::vertex;
    using arc = GR::arc;

    using ReachedMap = typename GR::vertex_map<bool>;

    static constexpr bool track_predecessor_vertices =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_NODES);
    static constexpr bool track_predecessor_arcs =
        static_cast<bool>(BH & TraversalAlgorithmBehavior::TRACK_PRED_ARCS);

    using PredverticesMap =
        std::conditional<track_predecessor_vertices, typename GR::vertex_map<vertex>,
                         std::monostate>::type;
    using PredarcsMap =
        std::conditional<track_predecessor_arcs, typename GR::vertex_map<arc>,
                         std::monostate>::type;

private:
    const GR & _graph;

    using OutarcRange = decltype(std::declval<GR>().out_arcs(vertex()));
    using OutarcIt = decltype(std::declval<OutarcRange>().begin());
    using OutarcItEnd = decltype(std::declval<OutarcRange>().end());

    static_assert(std::ranges::borrowed_range<OutarcRange>);
    std::vector<std::pair<OutarcIt, OutarcItEnd>> _stack;

    ReachedMap _reached_map;

    PredverticesMap _pred_vertices_map;
    PredarcsMap _pred_arcs_map;

public:
    Dfs(const GR & g) : _graph(g), _stack(), _reached_map(g.nb_vertices(), false) {
        _stack.reserve(g.nb_vertices());
        if constexpr(track_predecessor_vertices)
            _pred_vertices_map.resize(g.nb_vertices());
        if constexpr(track_predecessor_arcs) _pred_arcs_map.resize(g.nb_vertices());
    }

    Dfs & reset() noexcept {
        _stack.resize(0);
        _reached_map.fill(false);
        return *this;
    }
    Dfs & add_source(vertex s) noexcept {
        assert(!_reached_map[s]);
        push_node(s);
        if constexpr(track_predecessor_vertices) _pred_vertices_map[s] = s;
        return *this;
    }

    bool empty_queue() const noexcept { return _stack.empty(); }
    void push_node(vertex u) noexcept {
        OutarcRange r = _graph.out_arcs(u);
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
    std::pair<arc, vertex> next_node() noexcept {
        const arc a = *_stack.back().first;
        const vertex u = _graph.target(a);
        push_node(u);
        // if constexpr(track_predecessor_vertices) _pred_vertices_map[u] = u;
        if constexpr(track_predecessor_arcs) _pred_arcs_map[u] = a;
        advance_iterators();
        return std::make_pair(a, u);
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
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_DFS_HPP