#ifndef MELON_ALGORITHM_DIJKSTA_HPP
#define MELON_ALGORITHM_DIJKSTA_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/concepts/graph.hpp"
#include "melon/concepts/key_value_map.hpp"
#include "melon/concepts/priority_queue.hpp"
#include "melon/concepts/semiring.hpp"
#include "melon/data_structures/d_ary_heap.hpp"
#include "melon/utils/constexpr_ternary.hpp"
#include "melon/utils/prefetch.hpp"
#include "melon/utils/semirings.hpp"
#include "melon/utils/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
namespace concepts {
template <typename T>
concept dijkstra_trait = semiring<typename T::semiring> &&
    updatable_priority_queue<typename T::heap> && requires() {
    { T::store_pred_vertices } -> std::convertible_to<bool>;
    { T::store_pred_arcs } -> std::convertible_to<bool>;
    { T::store_distances } -> std::convertible_to<bool>;
};
}  // namespace concepts
// clang-format on

template <typename G, typename L>
struct dijkstra_default_traits {
    using semiring = shortest_path_semiring<mapped_value_t<L, vertex_t<G>>>;
    using heap =
        d_ary_heap<2, vertex_t<G>, mapped_value_t<L, vertex_t<G>>,
                   decltype(semiring::less), vertex_map_t<G, std::size_t>>;

    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
};

template <concepts::incidence_list_graph G, concepts::map_of<vertex_t<G>> L,
          concepts::dijkstra_trait T = dijkstra_default_traits<G, L>>
    requires concepts::has_vertex_map<G>
class dijkstra {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;
    using value_t = mapped_value_t<L, vertex_t<G>>;
    using traversal_entry = std::pair<vertex, value_t>;
    using traits = T;

    static_assert(std::is_same_v<traversal_entry, typename traits::heap::entry>,
                  "traversal_entry != heap_entry");

    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    using heap = traits::heap;
    using vertex_status_map = vertex_map_t<G, vertex_status>;
    using pred_vertices_map =
        std::conditional<traits::store_pred_vertices, vertex_map_t<G, vertex>,
                         std::monostate>::type;
    using pred_arcs_map =
        std::conditional<traits::store_pred_arcs, vertex_map_t<G, arc>,
                         std::monostate>::type;
    using distances_map =
        std::conditional<traits::store_distances, vertex_map_t<G, value_t>,
                         std::monostate>::type;

private:
    const G & _graph;
    const L & _length_map;

    heap _heap;
    vertex_status_map _vertex_status_map;
    pred_vertices_map _pred_vertices_map;
    pred_arcs_map _pred_arcs_map;
    distances_map _distances_map;

public:
    dijkstra(const G & g, const L & l)
        : _graph(g)
        , _length_map(l)
        , _heap(g.template create_vertex_map<std::size_t>())
        , _vertex_status_map(
              g.template create_vertex_map<vertex_status>(PRE_HEAP))
        , _pred_vertices_map(constexpr_ternary<traits::store_pred_vertices>(
              g.template create_vertex_map<vertex>(), std::monostate{}))
        , _pred_arcs_map(constexpr_ternary<traits::store_pred_arcs>(
              g.template create_vertex_map<arc>(), std::monostate{}))
        , _distances_map(constexpr_ternary<traits::store_distances>(
              g.template create_vertex_map<value_t>(), std::monostate{})) {}

    dijkstra(const G & g, const L & l, const vertex & s) : dijkstra(g, l) {
        add_source(s);
    }

    dijkstra & reset() noexcept {
        _heap.clear();
        for(const vertex & u : _graph.vertices())
            _vertex_status_map[u] = PRE_HEAP;
        return *this;
    }
    dijkstra & add_source(
        const vertex & s,
        const value_t & dist = traits::semiring::zero) noexcept {
        assert(_vertex_status_map[s] != IN_HEAP);
        _heap.push(s, dist);
        _vertex_status_map[s] = IN_HEAP;
        if constexpr(traits::store_pred_vertices) _pred_vertices_map[s] = s;
        return *this;
    }

    bool empty_queue() const noexcept { return _heap.empty(); }

    traversal_entry next_entry() noexcept {
        const traversal_entry & p = _heap.top();
        _vertex_status_map[p.first] = POST_HEAP;
        const auto & out_arcs = _graph.out_arcs(p.first);
        prefetch_range(out_arcs);
        prefetch_mapped_values(out_arcs, _graph.targets_map());
        prefetch_mapped_values(out_arcs, _length_map);
        _heap.pop();
        for(const arc & a : out_arcs) {
            const vertex & w = _graph.target(a);
            const vertex_status & w_status = _vertex_status_map[w];
            if(w_status == IN_HEAP) {
                const value_t new_dist =
                    traits::semiring::plus(p.second, _length_map[a]);
                if(traits::semiring::less(new_dist, _heap.priority(w))) {
                    _heap.promote(w, new_dist);
                    if constexpr(traits::store_pred_vertices)
                        _pred_vertices_map[w] = p.first;
                    if constexpr(traits::store_pred_arcs) _pred_arcs_map[w] = a;
                }
            } else if(w_status == PRE_HEAP) {
                _heap.push(w, traits::semiring::plus(p.second, _length_map[a]));
                _vertex_status_map[w] = IN_HEAP;
                if constexpr(traits::store_pred_vertices)
                    _pred_vertices_map[w] = p.first;
                if constexpr(traits::store_pred_arcs) _pred_arcs_map[w] = a;
            }
        }
        if constexpr(traits::store_distances)
            _distances_map[p.first] = p.second;
        return p;
    }

    void run() noexcept {
        while(!empty_queue()) next_entry();
    }
    auto begin() noexcept { return traversal_iterator(*this); }
    auto end() noexcept { return traversal_end_sentinel(); }

    vertex pred_vertex(const vertex & u) const noexcept
        requires(traits::store_pred_vertices)
    {
        assert(_vertex_status_map[u] != PRE_HEAP);
        return _pred_vertices_map[u];
    }
    arc pred_arc(const vertex & u) const noexcept
        requires(traits::store_pred_arcs)
    {
        assert(_vertex_status_map[u] != PRE_HEAP);
        return _pred_arcs_map[u];
    }
    value_t dist(const vertex & u) const noexcept
        requires(traits::store_distances)
    {
        assert(_vertex_status_map[u] == POST_HEAP);
        return _distances_map[u];
    }
    bool reached(const vertex & u) const noexcept {
        return _vertex_status_map[u] != PRE_HEAP;
    }
    bool visited(const vertex & u) const noexcept {
        return _vertex_status_map[u] == POST_HEAP;
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_DIJKSTA_HPP
