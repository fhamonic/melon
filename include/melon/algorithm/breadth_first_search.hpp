#ifndef MELON_ALGORITHM_BFS_HPP
#define MELON_ALGORITHM_BFS_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/concepts/graph_concepts.hpp"
#include "melon/utils/constexpr_ternary.hpp"
#include "melon/utils/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

struct breadth_first_search_default_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
};

template <concepts::graph G, typename T = breadth_first_search_default_traits>
requires(concepts::incidence_list_graph<G> ||
         concepts::adjacency_list_graph<G>) &&
    concepts::has_vertex_map<G> class breadth_first_search {
public:
    using vertex_t = G::vertex_t;
    using arc_t = G::arc_t;
    using traits = T;

    static_assert(
        !(concepts::adjacency_list_graph<G> && traits::store_pred_arcs),
        "traversal on adjacency_list_graph cannot access predecessor arcs.");

    using reached_map = graph_vertex_map<G, bool>;
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
    std::vector<vertex_t> _queue;
    std::vector<vertex_t>::iterator _queue_current;

    reached_map _reached_map;
    pred_vertices_map _pred_vertices_map;
    pred_arcs_map _pred_arcs_map;
    distances_map _dist_map;

public:
    explicit breadth_first_search(const G & g)
        : _graph(g)
        , _queue()
        , _reached_map(g.template create_vertex_map<bool>(false))
        , _pred_vertices_map(constexpr_ternary<traits::store_pred_vertices>(
              g.template create_vertex_map<vertex_t>(), std::monostate{}))
        , _pred_arcs_map(constexpr_ternary<traits::store_pred_arcs>(
              g.template create_vertex_map<arc_t>(), std::monostate{}))
        , _dist_map(constexpr_ternary<traits::store_distances>(
              g.template create_vertex_map<int>(), std::monostate{})) {
        _queue.reserve(g.nb_vertices());
        _queue_current = _queue.begin();
    }

    breadth_first_search(const G & g, const vertex_t & s)
        : breadth_first_search(g) {
        add_source(s);
    }

    breadth_first_search & reset() noexcept {
        _queue.resize(0);
        _reached_map.fill(false);
        return *this;
    }
    breadth_first_search & add_source(vertex_t s) noexcept {
        assert(!_reached_map[s]);
        _queue.push_back(s);
        _reached_map[s] = true;
        if constexpr(traits::store_pred_vertices) _pred_vertices_map[s] = s;
        if constexpr(traits::store_distances) _dist_map[s] = 0;
        return *this;
    }

    bool empty_queue() const noexcept { return _queue_current == _queue.end(); }

    const vertex_t & next_entry() noexcept {
        const vertex_t & u = *_queue_current;
        ++_queue_current;
        if constexpr(concepts::incidence_list_graph<G>) {
            for(auto && a : _graph.out_arcs(u)) {
                const vertex_t & w = _graph.target(a);
                if(reached(w)) continue;
                _queue.push_back(w);
                _reached_map[w] = true;
                if constexpr(traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(traits::store_pred_arcs) _pred_arcs_map[w] = a;
                if constexpr(traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        } else {  // i.e., concepts::adjacency_list_graph<G>
            for(auto && w : _graph.out_neighbors(u)) {
                if(reached(w)) continue;
                _queue.push_back(w);
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
    int dist(const vertex_t & u) const noexcept
        requires(traits::store_distances) {
        assert(reached(u));
        return _dist_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_BFS_HPP
