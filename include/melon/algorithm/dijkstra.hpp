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

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/constexpr_ternary.hpp"
#include "melon/detail/intrusive_view.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"
#include "melon/utility/traversal_iterator.hpp"
#include "melon/utility/value_map.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
template <typename T>
concept dijkstra_trait = semiring<typename T::semiring> &&
    updatable_priority_queue<typename T::heap> && requires() {
    { T::store_distances } -> std::convertible_to<bool>;
    { T::store_paths } -> std::convertible_to<bool>;
};
// clang-format on

template <typename G, typename L>
struct dijkstra_default_traits {
    using semiring = shortest_path_semiring<mapped_value_t<L, arc_t<G>>>;
    struct entry_cmp {
        [[nodiscard]] constexpr bool operator()(
            const auto & e1, const auto & e2) const noexcept {
            return semiring::less(e1.second, e2.second);
        }
    };
    using heap = d_ary_heap<2, vertex_t<G>, mapped_value_t<L, arc_t<G>>,
                            entry_cmp, vertex_map_t<G, std::size_t>>;

    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};

template <outward_incidence_graph G, input_value_map<arc_t<G>> L,
          dijkstra_trait T = dijkstra_default_traits<G, L>>
    requires has_vertex_map<G>
class dijkstra {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;
    using value_t = mapped_value_t<L, arc_t<G>>;
    using traversal_entry = std::pair<vertex, value_t>;
    using traits = T;

    static_assert(std::is_same_v<traversal_entry, typename traits::heap::entry>,
                  "traversal_entry != heap_entry");

    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    using heap = traits::heap;
    using vertex_status_map = vertex_map_t<G, vertex_status>;
    struct no_pred_vertices_map {};
    using pred_vertices_map =
        std::conditional < traits::store_paths && !has_arc_source<G>,
          vertex_map_t<G, vertex>, no_pred_vertices_map> ::type;
    using optional_arc = std::optional<arc>;
    struct no_pred_arcs_map {};
    using pred_arcs_map = std::conditional < traits::store_paths,
          vertex_map_t<G, optional_arc>, no_pred_arcs_map > ::type;
    struct no_distance_map {};
    using distances_map = std::conditional < traits::store_distances,
          vertex_map_t<G, value_t>, no_distance_map > ::type;

private:
    std::reference_wrapper<const G> _graph;
    std::reference_wrapper<const L> _length_map;

    heap _heap;
    vertex_status_map _vertex_status_map;
    [[no_unique_address]] pred_vertices_map _pred_vertices_map;
    [[no_unique_address]] pred_arcs_map _pred_arcs_map;
    [[no_unique_address]] distances_map _distances_map;

public:
    [[nodiscard]] constexpr dijkstra(const G & g, const L & l)
        : _graph(g)
        , _length_map(l)
        , _heap(create_vertex_map<std::size_t>(g))
        , _vertex_status_map(create_vertex_map<vertex_status>(g, PRE_HEAP))
        , _pred_vertices_map(constexpr_ternary < traits::store_paths &&
                             !has_arc_source < G >>
                                 (create_vertex_map<vertex>(g), no_pred_vertices_map{}))
        , _pred_arcs_map(constexpr_ternary<traits::store_paths>(
              create_vertex_map<optional_arc>(g), no_pred_arcs_map{}))
        , _distances_map(constexpr_ternary<traits::store_distances>(
              create_vertex_map<value_t>(g), no_distance_map{})) {}

    [[nodiscard]] constexpr dijkstra(const G & g, const L & l, const vertex & s)
        : dijkstra(g, l) {
        add_source(s);
    }

    [[nodiscard]] constexpr dijkstra(const dijkstra & bin) = default;
    [[nodiscard]] constexpr dijkstra(dijkstra && bin) = default;

    constexpr dijkstra & operator=(const dijkstra &) = default;
    constexpr dijkstra & operator=(dijkstra &&) = default;

    dijkstra & set_length_map(const L & l) noexcept {
        _length_map = std::ref(l);
        return *this;
    }

    constexpr dijkstra & reset() noexcept {
        _heap.clear();
        _vertex_status_map.fill(PRE_HEAP);
        return *this;
    }
    constexpr dijkstra & add_source(
        const vertex & s,
        const value_t & dist = traits::semiring::zero) noexcept {
        assert(_vertex_status_map[s] != IN_HEAP);
        _heap.push(s, dist);
        _vertex_status_map[s] = IN_HEAP;
        if constexpr(traits::store_paths) {
            _pred_arcs_map[s].reset();
            if constexpr(!has_arc_source<G>) _pred_vertices_map[s] = s;
        }
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _heap.empty();
    }

    [[nodiscard]] constexpr traversal_entry current() const noexcept {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() noexcept {
        assert(!finished());
        const auto [t, st_dist] = _heap.top();
        if constexpr(traits::store_distances) _distances_map[t] = st_dist;
        _vertex_status_map[t] = POST_HEAP;
        auto && out_arcs_range = melon::out_arcs(_graph.get(), t);
        prefetch_range(out_arcs_range);
        prefetch_mapped_values(out_arcs_range, arc_targets_map(_graph.get()));
        prefetch_mapped_values(out_arcs_range, _length_map.get());
        _heap.pop();
        for(const arc & a : out_arcs_range) {
            const vertex & w = melon::arc_target(_graph.get(), a);
            const vertex_status & w_status = _vertex_status_map[w];
            if(w_status == IN_HEAP) {
                const value_t new_dist =
                    traits::semiring::plus(st_dist, _length_map.get()[a]);
                if(traits::semiring::less(new_dist, _heap.priority(w))) {
                    _heap.promote(w, new_dist);
                    if constexpr(traits::store_paths) {
                        _pred_arcs_map[w].emplace(a);
                        if constexpr(!has_arc_source<G>)
                            _pred_vertices_map[w] = t;
                    }
                }
            } else if(w_status == PRE_HEAP) {
                _heap.push(
                    w, traits::semiring::plus(st_dist, _length_map.get()[a]));
                _vertex_status_map[w] = IN_HEAP;
                if constexpr(traits::store_paths) {
                    _pred_arcs_map[w].emplace(a);
                    if constexpr(!has_arc_source<G>) _pred_vertices_map[w] = t;
                }
            }
        }
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }
    [[nodiscard]] constexpr auto begin() noexcept {
        return traversal_iterator(*this);
    }
    [[nodiscard]] constexpr auto end() noexcept {
        return traversal_end_sentinel();
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _vertex_status_map[u] != PRE_HEAP;
    }
    [[nodiscard]] constexpr bool visited(const vertex & u) const noexcept {
        return _vertex_status_map[u] == POST_HEAP;
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const noexcept
        requires(traits::store_paths)
    {
        assert(reached(u));
        return _pred_arcs_map[u].value();
    }
    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const noexcept
        requires(traits::store_paths)
    {
        assert(reached(u) && _pred_arcs_map[u].has_value());
        if constexpr(has_arc_source<G>)
            return melon::arc_source(_graph.get(), pred_arc(u));
        else
            return _pred_vertices_map[u];
    }
    [[nodiscard]] constexpr value_t current_dist(
        const vertex & u) const noexcept
        requires(traits::store_distances)
    {
        assert(reached(u) && !visited(u));
        return _heap.priority(u);
    }
    [[nodiscard]] constexpr value_t dist(const vertex & u) const noexcept
        requires(traits::store_distances)
    {
        assert(visited(u));
        return _distances_map[u];
    }

    [[nodiscard]] constexpr auto path_to(const vertex & t) const noexcept
        requires(traits::store_paths)
    {
        assert(reached(t));
        return intrusive_view(
            static_cast<vertex>(t),
            [this](const vertex & v) -> arc {
                return _pred_arcs_map[v].value();
            },
            [this](const vertex & v) -> vertex { return pred_vertex(v); },
            [this](const vertex & v) -> bool {
                return _pred_arcs_map[v].has_value();
            });
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_DIJKSTA_HPP
