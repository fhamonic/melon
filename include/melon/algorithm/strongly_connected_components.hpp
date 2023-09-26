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
#include "melon/graph.hpp"
#include "melon/utility/traversal_iterator.hpp"
#include "melon/detail/intrusive_view.hpp"

namespace fhamonic {
namespace melon {

struct scc_default_traits {
    static constexpr bool store_component_num = false;
};

// TODO ranges , requires out_neighbors : borrowed_range
template <outward_incidence_graph G, typename T = scc_default_traits>
    requires(outward_incidence_graph<G> || outward_adjacency_graph<G>) &&
            has_vertex_map<G>
class strongly_connected_components {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;
    using traits = T;
    using reached_map = vertex_map_t<G, bool>;

    struct no_component_num_map {};
    using component_num_map =
        std::conditional<traits::store_component_num, vertex_map_t<G, unsigned int>,
                         no_component_num_map>::type;

    std::reference_wrapper<const G> _graph;
    std::vector<vertex> _stack;

    reached_map _reached_map;
    
    [[no_unique_address]] component_num_map _component_num_map;

public:
    [[nodiscard]] constexpr explicit depth_first_search(const G & g) noexcept
        : _graph(g)
        , _stack()
        , _reached_map(create_vertex_map<bool>(g, false))
        , _component_num_map(constexpr_ternary<traits::store_component_num>(
              create_vertex_map<arc>(g), no_component_num_map{})) {
        _stack.reserve(g.nb_vertices());
    }

    [[nodiscard]] constexpr depth_first_search(const G & g,
                                               const vertex & s) noexcept
        : depth_first_search(g) {
        add_source(s);
    }

    [[nodiscard]] constexpr depth_first_search(const depth_first_search & bin) =
        default;
    [[nodiscard]] constexpr depth_first_search(depth_first_search && bin) =
        default;

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
        _stack.push_back(s);
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
        return _stack.back();
    }

    constexpr void advance() noexcept {
        assert(!finished());
        const vertex u = _stack.back();
        _stack.pop_back();
        if constexpr(outward_incidence_graph<G>) {
            for(auto && a : out_arcs(_graph.get(), u)) {
                const vertex & w = arc_target(_graph.get(), a);
                if(_reached_map[w]) continue;
                _stack.push_back(w);
                _reached_map[w] = true;
                if constexpr(traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(traits::store_pred_arcs) _pred_arcs_map[w] = a;
                if constexpr(traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        } else {  // i.e., outward_adjacency_graph<G>
            for(auto && w : out_neighbors(_graph.get(), u)) {
                if(_reached_map[w]) continue;
                _stack.push_back(w);
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
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_depth_first_search_HPP