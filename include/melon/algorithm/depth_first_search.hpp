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

#include "melon/detail/constexpr_ternary.hpp"
#include "melon/detail/consumable_range.hpp"
#include "melon/graph.hpp"
#include "melon/utility/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

struct dfs_default_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
};

template <outward_adjacency_graph G, typename T = dfs_default_traits>
    requires has_vertex_map<G>
class depth_first_search {
private:
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

    std::reference_wrapper<const G> _graph;

    using stack_range =
        std::conditional<traits::store_pred_arcs, out_arcs_range_t<G>,
                         out_neighbors_range_t<G>>::type;

    std::vector<std::pair<vertex, consumable_range<stack_range>>> _stack;

    vertex_map_t<G, bool> _reached_map;

    [[no_unique_address]] pred_vertices_map _pred_vertices_map;
    [[no_unique_address]] pred_arcs_map _pred_arcs_map;
    [[no_unique_address]] distances_map _dist_map;

public:
    [[nodiscard]] constexpr explicit depth_first_search(const G & g) noexcept
        : _graph(g)
        , _stack()
        , _reached_map(create_vertex_map<bool>(g, false))
        , _pred_vertices_map(constexpr_ternary<traits::store_pred_vertices>(
              create_vertex_map<vertex>(g), no_pred_vertices_map{}))
        , _pred_arcs_map(constexpr_ternary<traits::store_pred_arcs>(
              create_vertex_map<arc>(g), no_pred_arcs_map{}))
        , _dist_map(constexpr_ternary<traits::store_distances>(
              create_vertex_map<int>(g), no_distance_map{})) {
        if constexpr(has_nb_vertices<G>) {
            _stack.reserve(nb_vertices(g));
        }
    }

    [[nodiscard]] constexpr depth_first_search(const G & g,
                                               const vertex & s) noexcept
        : depth_first_search(g) {
        add_source(s);
    }

    [[nodiscard]] constexpr depth_first_search(const depth_first_search &) =
        default;
    [[nodiscard]] constexpr depth_first_search(depth_first_search &&) = default;

    constexpr depth_first_search & operator=(const depth_first_search &) =
        default;
    constexpr depth_first_search & operator=(depth_first_search &&) = default;

    constexpr depth_first_search & reset() noexcept {
        _stack.resize(0);
        _reached_map.fill(false);
        return *this;
    }
    constexpr depth_first_search & add_source(const vertex & s) noexcept {
        assert(!_reached_map[s]);
        if constexpr(traits::store_pred_arcs)
            _stack.emplace_back(s, out_arcs(_graph.get(), s));
        else
            _stack.emplace_back(s, out_neighbors(_graph.get(), s));
        _reached_map[s] = true;
        if constexpr(traits::store_pred_vertices) _pred_vertices_map[s] = s;
        if constexpr(traits::store_distances) _dist_map[s] = 0;
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _stack.empty();
    }

    [[nodiscard]] constexpr vertex current() const noexcept {
        assert(!finished());
        return _stack.back().first;
    }

    constexpr void advance() noexcept {
        assert(!finished());
        do {
            if constexpr(traits::store_pred_arcs) {
                for(auto & remaining_arcs = _stack.back().second;
                    !remaining_arcs.empty(); remaining_arcs.advance()) {
                    auto a = remaining_arcs.current();
                    auto w = arc_target(_graph.get(), a);
                    if(_reached_map[w]) continue;
                    _reached_map[w] = true;
                    _pred_arcs_map[w] = a;
                    if constexpr(traits::store_pred_vertices)
                        _pred_vertices_map[w] = _stack.back().first;
                    if constexpr(traits::store_distances)
                        _dist_map[w] = _dist_map[_stack.back().first] + 1;
                    _stack.emplace_back(w, out_arcs(_graph.get(), w));
                    return;
                }

            } else {
                for(auto & remaining_neighbors = _stack.back().second;
                    !remaining_neighbors.empty();
                    remaining_neighbors.advance()) {
                    auto w = remaining_neighbors.current();
                    if(_reached_map[w]) continue;
                    _reached_map[w] = true;
                    if constexpr(traits::store_pred_vertices)
                        _pred_vertices_map[w] = _stack.back().first;
                    if constexpr(traits::store_distances)
                        _dist_map[w] = _dist_map[_stack.back().first] + 1;
                    _stack.emplace_back(w, out_neighbors(_graph.get(), w));
                    return;
                }
            }
            _stack.pop_back();
        } while(!_stack.empty());
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
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_depth_first_search_HPP