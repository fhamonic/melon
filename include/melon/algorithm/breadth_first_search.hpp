#ifndef MELON_ALGORITHM_BFS_HPP
#define MELON_ALGORITHM_BFS_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/detail/constexpr_ternary.hpp"
#include "melon/graph.hpp"
#include "melon/utility/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

struct breadth_first_search_default_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
};

template <outward_adjacency_graph G,
          typename T = breadth_first_search_default_traits>
    requires has_vertex_map<G>
class breadth_first_search {
public:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;
    using traits = T;

    static_assert(!traits::store_pred_arcs || outward_incidence_graph<G>,
                  "storing predecessor arcs requires outward_incidence_graph.");

    struct no_pred_vertices_map {};
    using pred_vertices_map =
        std::conditional<traits::store_pred_vertices, vertex_map_t<G, vertex>,
                         no_pred_vertices_map>::type;
    struct no_pred_arcs_map {};
    using pred_arcs_map =
        std::conditional<traits::store_pred_arcs, vertex_map_t<G, arc>,
                         no_pred_arcs_map>::type;
    struct no_distance_map {};
    using distances_map =
        std::conditional<traits::store_distances, vertex_map_t<G, int>,
                         no_distance_map>::type;

    using cursor =
        std::conditional_t<has_nb_vertices<G>,
                           typename std::vector<vertex>::iterator, int>;

private:
    std::reference_wrapper<const G> _graph;
    std::vector<vertex> _queue;
    cursor _queue_current;

    vertex_map_t<G, bool> _reached_map;

    [[no_unique_address]] pred_vertices_map _pred_vertices_map;
    [[no_unique_address]] pred_arcs_map _pred_arcs_map;
    [[no_unique_address]] distances_map _dist_map;

public:
    [[nodiscard]] constexpr explicit breadth_first_search(const G & g)
        : _graph(g)
        , _queue()
        , _reached_map(create_vertex_map<bool>(g, false))
        , _pred_vertices_map(constexpr_ternary<traits::store_pred_vertices>(
              create_vertex_map<vertex>(g), no_pred_vertices_map{}))
        , _pred_arcs_map(constexpr_ternary<traits::store_pred_arcs>(
              create_vertex_map<arc>(g), no_pred_arcs_map{}))
        , _dist_map(constexpr_ternary<traits::store_distances>(
              create_vertex_map<int>(g), no_distance_map{})) {
        if constexpr(has_nb_vertices<G>) {
            _queue.reserve(nb_vertices(g));
            _queue_current = _queue.begin();
        } else {
            _queue_current = 0;
        }
    }

    [[nodiscard]] constexpr breadth_first_search(const G & g, const vertex & s)
        : breadth_first_search(g) {
        add_source(s);
    }

    [[nodiscard]] constexpr breadth_first_search(const breadth_first_search &) =
        default;
    [[nodiscard]] constexpr breadth_first_search(breadth_first_search &&) =
        default;

    constexpr breadth_first_search & operator=(const breadth_first_search &) =
        default;
    constexpr breadth_first_search & operator=(breadth_first_search &&) =
        default;

    constexpr breadth_first_search & reset() noexcept {
        _queue.resize(0);
        if constexpr(has_nb_vertices<G>) {
            _queue_current = _queue.begin();
        } else {
            _queue_current = 0;
        }
        _reached_map.fill(false);
        return *this;
    }
    constexpr breadth_first_search & add_source(const vertex & s) noexcept {
        assert(!_reached_map[s]);
        _queue.push_back(s);
        _reached_map[s] = true;
        if constexpr(traits::store_pred_vertices) _pred_vertices_map[s] = s;
        if constexpr(traits::store_distances) _dist_map[s] = 0;
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        if constexpr(has_nb_vertices<G>) {
            return _queue_current == _queue.end();
        } else {
            return _queue_current == _queue.size();
        }
    }

    [[nodiscard]] constexpr const vertex & current() const noexcept {
        assert(!finished());
        if constexpr(has_nb_vertices<G>) {
            return *_queue_current;
        } else {
            return _queue[_queue_current];
        }
    }

    constexpr void advance() noexcept {
        assert(!finished());
        const vertex & u = current();
        ++_queue_current;
        if constexpr(traits::store_pred_arcs) {
            for(auto && a : out_arcs(_graph.get(), u)) {
                const vertex & w = arc_target(_graph.get(), a);
                if(_reached_map[w]) continue;
                _queue.push_back(w);
                _reached_map[w] = true;
                _pred_arcs_map[w] = a;
                if constexpr(traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        } else {
            for(auto && w : out_neighbors(_graph.get(), u)) {
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
        requires(traits::store_pred_vertices)
    {
        assert(reached(u));
        return _pred_vertices_map[u];
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const noexcept
        requires(traits::store_pred_arcs)
    {
        assert(reached(u));
        return _pred_arcs_map[u];
    }
    [[nodiscard]] constexpr int dist(const vertex & u) const noexcept
        requires(traits::store_distances)
    {
        assert(reached(u));
        return _dist_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_BFS_HPP
