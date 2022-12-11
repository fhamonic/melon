#ifndef MELON_ALGORITHM_BFS_HPP
#define MELON_ALGORITHM_BFS_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/concepts/graph.hpp"
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
requires(concepts::outward_incidence_graph<G> ||
         concepts::outward_adjacency_graph<G>) &&
    concepts::has_vertex_map<G> class breadth_first_search {
public:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;
    using traits = T;

    static_assert(
        !(concepts::outward_adjacency_graph<G> && traits::store_pred_arcs),
        "traversal on outward_adjacency_list cannot access predecessor arcs.");

    using reached_map = vertex_map_t<G, bool>;
    using pred_vertices_map =
        std::conditional<traits::store_pred_vertices, vertex_map_t<G, vertex>,
                         std::monostate>::type;
    using pred_arcs_map =
        std::conditional<traits::store_pred_arcs, vertex_map_t<G, arc>,
                         std::monostate>::type;
    using distances_map =
        std::conditional<traits::store_distances, vertex_map_t<G, int>,
                         std::monostate>::type;

private:
    std::reference_wrapper<const G> _graph;
    std::vector<vertex> _queue;
    std::vector<vertex>::iterator _queue_current;

    reached_map _reached_map;
    pred_vertices_map _pred_vertices_map;
    pred_arcs_map _pred_arcs_map;
    distances_map _dist_map;

public:
    [[nodiscard]] constexpr explicit breadth_first_search(const G & g)
        : _graph(g)
        , _queue()
        , _reached_map(g.template create_vertex_map<bool>(false))
        , _pred_vertices_map(constexpr_ternary<traits::store_pred_vertices>(
              g.template create_vertex_map<vertex>(), std::monostate{}))
        , _pred_arcs_map(constexpr_ternary<traits::store_pred_arcs>(
              g.template create_vertex_map<arc>(), std::monostate{}))
        , _dist_map(constexpr_ternary<traits::store_distances>(
              g.template create_vertex_map<int>(), std::monostate{})) {
        _queue.reserve(g.nb_vertices());
        _queue_current = _queue.begin();
    }

    [[nodiscard]] constexpr breadth_first_search(const G & g, const vertex & s)
        : breadth_first_search(g) {
        add_source(s);
    }

    [[nodiscard]] constexpr breadth_first_search(
        const breadth_first_search & bin) = default;
    [[nodiscard]] constexpr breadth_first_search(breadth_first_search && bin) =
        default;

    constexpr breadth_first_search & operator=(const breadth_first_search &) =
        default;
    constexpr breadth_first_search & operator=(breadth_first_search &&) =
        default;

    constexpr breadth_first_search & reset() noexcept {
        _queue.resize(0);
        _queue_current = _queue.begin();
        _reached_map.fill(false);
        return *this;
    }
    constexpr breadth_first_search & add_source(vertex s) noexcept {
        assert(!_reached_map[s]);
        _queue.push_back(s);
        _reached_map[s] = true;
        if constexpr(traits::store_pred_vertices) _pred_vertices_map[s] = s;
        if constexpr(traits::store_distances) _dist_map[s] = 0;
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _queue_current == _queue.end();
    }

    [[nodiscard]] constexpr vertex current() const noexcept {
        assert(!finished());
        return *_queue_current;
    }

    constexpr void advance() noexcept {
        assert(!finished());
        const vertex & u = *_queue_current;
        ++_queue_current;
        if constexpr(concepts::outward_incidence_graph<G>) {
            for(auto && a : _graph.get().out_arcs(u)) {
                const vertex & w = _graph.get().target(a);
                if(_reached_map[w]) continue;
                _queue.push_back(w);
                _reached_map[w] = true;
                if constexpr(traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(traits::store_pred_arcs) _pred_arcs_map[w] = a;
                if constexpr(traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        } else {  // i.e., concepts::outward_adjacency_graph<G>
            for(auto && w : _graph.get().out_neighbors(u)) {
                if(_reached_map[w]) continue;
                _queue.push_back(w);
                _reached_map[w] = true;
                if constexpr(traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        }
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }
    [[nodiscard]] constexpr auto begin() noexcept {
        return traversal_iterator(*this);
    }
    [[nodiscard]] constexpr auto end() const noexcept {
        return traversal_end_sentinel();
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _reached_map[u];
    }

    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const noexcept
        requires(traits::store_pred_vertices) {
        assert(reached(u));
        return _pred_vertices_map[u];
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const noexcept
        requires(traits::store_pred_arcs) {
        assert(reached(u));
        return _pred_arcs_map[u];
    }
    [[nodiscard]] constexpr int dist(const vertex & u) const noexcept
        requires(traits::store_distances) {
        assert(reached(u));
        return _dist_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_BFS_HPP
