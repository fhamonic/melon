#ifndef MELON_ALGORITHM_TOPOLOGICAL_SORT_HPP
#define MELON_ALGORITHM_TOPOLOGICAL_SORT_HPP

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
    using remaining_in_degree_map = vertex_map_t<G, unsigned int>;
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
    remaining_in_degree_map _remaining_in_degree_map;
    pred_vertices_map _pred_vertices_map;
    pred_arcs_map _pred_arcs_map;
    distances_map _dist_map;

public:
    [[nodiscard]] constexpr explicit breadth_first_search(const G & g)
        : _graph(g)
        , _queue()
        , _reached_map(create_vertex_map<bool>(g, false))
        , _remaining_in_degree_map(create_vertex_map<unsigned int>(
              g, std::numeric_limits<unsigned int>::max()))
        , _pred_vertices_map(constexpr_ternary<traits::store_pred_vertices>(
              create_vertex_map<vertex>(g), std::monostate{}))
        , _pred_arcs_map(constexpr_ternary<traits::store_pred_arcs>(
              create_vertex_map<arc>(g), std::monostate{}))
        , _dist_map(constexpr_ternary<traits::store_distances>(
              create_vertex_map<int>(g), std::monostate{})) {
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

public:
    constexpr breadth_first_search & reset() noexcept {
        _queue.resize(0);
        _queue_current = _queue.begin();
        _reached_map.fill(false);
        _remaining_in_degree_map.fill(std::numeric_limits<unsigned int>::max());
        return *this;
    }
    constexpr breadth_first_search & add_source(vertex s) noexcept {
        assert(!_reached_map[s]);
        _queue.push_back(s);
        _reached_map[s] = true;
        _remaining_in_degree_map[s] = 0;
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
            for(auto && a : out_arcs(_graph.get(), u)) {
                const vertex & w = target(_graph.get(), a);
                if(!_reached_map[w]) {
                    // _remaining_in_degree_map[w] = in_degree(_graph.get(), w);
                    _reached_map[w] = true;
                }
                if(--_remaining_in_degree_map[w] > 0) continue;
                _queue.push_back(w);
                if constexpr(traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(traits::store_pred_arcs) _pred_arcs_map[w] = a;
                if constexpr(traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        } else {  // i.e., concepts::outward_adjacency_graph<G>
            for(auto && w : out_neighbors(_graph.get(), u)) {
                if(!_reached_map[w]) {
                    // _remaining_in_degree_map[w] = in_degree(_graph.get(), w);
                    _reached_map[w] = true;
                }
                if(--_remaining_in_degree_map[w] > 0) continue;
                _queue.push_back(w);
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

#endif  // MELON_ALGORITHM_TOPOLOGICAL_SORT_HPP
