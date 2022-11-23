#ifndef MELON_ALGORITHM_depth_first_search_HPP
#define MELON_ALGORITHM_depth_first_search_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <stack>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/concepts/graph_concepts.hpp"
#include "melon/utils/constexpr_ternary.hpp"
#include "melon/utils/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

struct dfs_default_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
};

// TODO ranges , requires out_arcs : borrowed_range
template <concepts::incidence_list_graph G, typename T = dfs_default_traits>
class depth_first_search {
public:
    using vertex_t = G::vertex_t;
    using arc_t = G::arc_t;
    using traits = T;
    using reached_map = typename G::vertex_map<bool>;

    using pred_vertices_map = std::conditional<traits::store_pred_vertices,
                                               typename G::vertex_map<vertex_t>,
                                               std::monostate>::type;
    using pred_arcs_map =
        std::conditional<traits::store_pred_arcs, typename G::vertex_map<arc_t>,
                         std::monostate>::type;

private:
    const G & _graph;

    using out_arcs_range = decltype(std::declval<G>().out_arcs(vertex_t()));
    using out_arcs_it = decltype(std::declval<out_arcs_range>().begin());
    using out_arcs_sentinel = decltype(std::declval<out_arcs_range>().end());

    static_assert(std::ranges::borrowed_range<out_arcs_range>);
    std::vector<std::pair<out_arcs_it, out_arcs_sentinel>> _stack;

    reached_map _reached_map;

    pred_vertices_map _pred_vertices_map;
    pred_arcs_map _pred_arcs_map;

public:
    depth_first_search(const G & g)
        : _graph(g)
        , _stack()
        , _reached_map(g.nb_vertices(), false)
        , _pred_vertices_map(constexpr_ternary<traits::store_pred_vertices>(
              g.nb_vertices(), std::monostate{}))
        , _pred_arcs_map(constexpr_ternary<traits::store_pred_arcs>(
              g.nb_vertices(), std::monostate{})) {
        _stack.reserve(g.nb_vertices());
    }

    depth_first_search & reset() noexcept {
        _stack.resize(0);
        _reached_map.fill(false);
        return *this;
    }
    depth_first_search & add_source(vertex_t s) noexcept {
        assert(!_reached_map[s]);
        push_node(s);
        if constexpr(traits::store_pred_vertices) _pred_vertices_map[s] = s;
        return *this;
    }

    bool empty_queue() const noexcept { return _stack.empty(); }
    void push_node(vertex_t u) noexcept {
        out_arcs_range r = _graph.out_arcs(u);
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
    std::pair<arc_t, vertex_t> next_entry() noexcept {
        const arc_t a = *_stack.back().first;
        const vertex_t u = _graph.target(a);
        push_node(u);
        // if constexpr(traits::store_pred_vertices) _pred_vertices_map[u] = u;
        if constexpr(traits::store_pred_arcs) _pred_arcs_map[u] = a;
        advance_iterators();
        return std::make_pair(a, u);
    }

    void run() noexcept {
        while(!empty_queue()) next_entry();
    }
    auto begin() noexcept { return traversal_iterator(*this); }
    auto end() noexcept { return traversal_end_sentinel(); }

    bool reached(const vertex_t u) const noexcept { return _reached_map[u]; }
    vertex_t pred_vertex(const vertex_t u) const noexcept
        requires(traits::store_pred_vertices) {
        assert(reached(u));
        return _pred_vertices_map[u];
    }
    arc_t pred_arc(const vertex_t u) const noexcept
        requires(traits::store_pred_arcs) {
        assert(reached(u));
        return _pred_arcs_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_depth_first_search_HPP