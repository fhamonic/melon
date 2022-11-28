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
    static constexpr bool store_distances = false;
};

// TODO ranges , requires out_neighbors : borrowed_range
template <concepts::incidence_list_graph G, typename T = dfs_default_traits>
    requires(concepts::incidence_list_graph<G> ||
             concepts::adjacency_list_graph<G>) &&
            concepts::has_vertex_map<G>
class depth_first_search {
public:
    using vertex_t = G::vertex_t;
    using arc_t = G::arc_t;
    using traits = T;
    using reached_map = graph_vertex_map<G, bool>;

    static_assert(
        !(concepts::adjacency_list_graph<G> && traits::store_pred_arcs),
        "traversal on adjacency_list_graph cannot access predecessor arcs.");

    using pred_vertices_map =
        std::conditional<traits::store_pred_vertices,
                         graph_vertex_map<G, vertex_t>, std::monostate>::type;
    using pred_arcs_map =
        std::conditional<traits::store_pred_arcs, graph_vertex_map<G, arc_t>,
                         std::monostate>::type;
    using distances_map =
        std::conditional<traits::store_distances, graph_vertex_map<G, int>,
                         std::monostate>::type;

private:
    const G & _graph;
    std::vector<vertex_t> _stack;

    reached_map _reached_map;
    pred_vertices_map _pred_vertices_map;
    pred_arcs_map _pred_arcs_map;
    distances_map _dist_map;

public:
    explicit depth_first_search(const G & g) noexcept
        : _graph(g)
        , _stack()
        , _reached_map(g.template create_vertex_map<bool>(false))
        , _pred_vertices_map(constexpr_ternary<traits::store_pred_vertices>(
              g.template create_vertex_map<vertex_t>(), std::monostate{}))
        , _pred_arcs_map(constexpr_ternary<traits::store_pred_arcs>(
              g.template create_vertex_map<arc_t>(), std::monostate{}))
        , _dist_map(constexpr_ternary<traits::store_distances>(
              g.template create_vertex_map<int>(), std::monostate{})) {
        _stack.reserve(g.nb_vertices());
    }

    depth_first_search(const G & g, const vertex_t & s) noexcept
        : depth_first_search(g) {
        add_source(s);
    }

    depth_first_search & reset() noexcept {
        _stack.resize(0);
        _reached_map.fill(false);
        return *this;
    }
    depth_first_search & add_source(const vertex_t & s) noexcept {
        assert(!_reached_map[s]);
        _stack.push_back(s);
        _reached_map[s] = true;
        if constexpr(traits::store_pred_vertices) _pred_vertices_map[s] = s;
        if constexpr(traits::store_distances) _dist_map[s] = 0;
        return *this;
    }

    bool empty_queue() const noexcept { return _stack.empty(); }

public:
    vertex_t next_entry() noexcept {
        assert(!_stack.empty());
        const vertex_t u = _stack.back();
        _stack.pop_back();
        if constexpr(concepts::incidence_list_graph<G>) {
            for(auto && a : _graph.out_arcs(u)) {
                const vertex_t & w = _graph.target(a);
                if(_reached_map[w]) continue;
                _stack.push_back(w);
                _reached_map[w] = true;
                if constexpr(traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(traits::store_pred_arcs) _pred_arcs_map[w] = a;
                if constexpr(traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        } else {  // i.e., concepts::adjacency_list_graph<G>
            for(auto && w : _graph.out_neighbors(u)) {
                if(_reached_map[w]) continue;
                _stack.push_back(w);
                _reached_map[w] = true;
                if constexpr(traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        }
        return u;
    }

    void run() noexcept {
        while(!empty_queue()) next_entry();
    }
    auto begin() noexcept { return traversal_iterator(*this); }
    auto end() noexcept { return traversal_end_sentinel(); }

    bool reached(const vertex_t & u) const noexcept { return _reached_map[u]; }
    vertex_t pred_vertex(const vertex_t & u) const noexcept
        requires(traits::store_pred_vertices) {
        assert(reached(u));
        return _pred_vertices_map[u];
    }
    arc_t pred_arc(const vertex_t & u) const noexcept
        requires(traits::store_pred_arcs) {
        assert(reached(u));
        return _pred_arcs_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_depth_first_search_HPP