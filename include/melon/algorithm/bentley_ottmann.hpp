#ifndef MELON_ALGORITHM_BENTLEY_OTTMANN_HPP
#define MELON_ALGORITHM_BENTLEY_OTTMANN_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/planar_map.hpp"

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

template <typename G>
struct bentley_ottman_default_traits {
    using coords_t = vertex_coordinates_t<G>;
    struct sweepline_cmp {
        [[nodiscard]] constexpr bool operator()(
            const coords_t & p1, const coords_t & p2) const noexcept {
            if(std::get<1>(e1.second) == std::get<1>(p2.second))
                return std::get<0>(p1.second) < std::get<0>(p2.second);
            return std::get<1>(p1.second) < std::get<1>(p2.second);
        }
    };
    using heap = d_ary_heap<2, coords_t, std::vector<arc_t<G>>, sweepline_cmp>;
    using intersection_fn = [](const coords_t & A, const coords_t & B,
                               const coords_t & C,
                               const coords_t & D) -> std::optional<coords_t> {
        const double a1 = B.second - A.second;
        const double b1 = A.first - B.first;
        const double c1 = a1 * (A.first) + b1 * (A.second);
        // Line CD represented as a2x + b2y = c2
        const double a2 = D.second - C.second;
        const double b2 = C.first - D.first;
        const double c2 = a2 * (C.first) + b2 * (C.second);

        const double determinant = a1 * b2 - a2 * b1;
        if(determinant == 0) return std::nullopt;

        const double x = (b2 * c1 - b1 * c2) / determinant;
        const double y = (a1 * c2 - a2 * c1) / determinant;
        return coords_t{x, y};
    };

    template <drawable_graph G>
        requires has_vertex_map<G>
    class bentley_ottman {
    private:
        using vertex = vertex_t<G>;
        using arc = arc_t<G>;
        using value_t = mapped_value_t<L, arc_t<G>>;
        using traversal_entry = std::pair<vertex, value_t>;
        using traits = T;

        static_assert(
            std::is_same_v<traversal_entry, typename traits::heap::entry>,
            "traversal_entry != heap_entry");

        enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

        using heap = traits::heap;
        using vertex_status_map = vertex_map_t<G, vertex_status>;
        using pred_vertices_map =
            std::conditional<traits::store_paths && !has_arc_source<G>,
                             vertex_map_t<G, vertex>, std::monostate>::type;
        using optional_arc = std::optional<arc>;
        using pred_arcs_map =
            std::conditional<traits::store_paths, vertex_map_t<G, optional_arc>,
                             std::monostate>::type;
        using distances_map =
            std::conditional<traits::store_distances, vertex_map_t<G, value_t>,
                             std::monostate>::type;

    private:
        std::reference_wrapper<const G> _graph;

        heap _heap;
        vertex_status_map _vertex_status_map;
        pred_vertices_map _pred_vertices_map;
        pred_arcs_map _pred_arcs_map;
        distances_map _distances_map;

    public:
        [[nodiscard]] constexpr bentley_ottman(const G & g, const L & l)
            : _graph(g)
            , _length_map(l)
            , _heap(create_vertex_map<std::size_t>(g))
            , _vertex_status_map(create_vertex_map<vertex_status>(g, PRE_HEAP))
            , _pred_vertices_map(
                  constexpr_ternary < traits::store_paths &&
                  !has_arc_source < G >>
                      (create_vertex_map<vertex>(g), std::monostate{}))
            , _pred_arcs_map(constexpr_ternary<traits::store_paths>(
                  create_vertex_map<optional_arc>(g), std::monostate{}))
            , _distances_map(constexpr_ternary<traits::store_distances>(
                  create_vertex_map<value_t>(g), std::monostate{})) {}

        [[nodiscard]] constexpr bentley_ottman(const G & g, const L & l,
                                               const vertex & s)
            : bentley_ottman(g, l) {
            add_source(s);
        }

        [[nodiscard]] constexpr bentley_ottman(const bentley_ottman & bin) =
            default;
        [[nodiscard]] constexpr bentley_ottman(bentley_ottman && bin) = default;

        constexpr bentley_ottman & operator=(const bentley_ottman &) = default;
        constexpr bentley_ottman & operator=(bentley_ottman &&) = default;

        bentley_ottman & set_length_map(const L & l) noexcept {
            _length_map = std::ref(l);
            return *this;
        }

        constexpr bentley_ottman & reset() noexcept {
            _heap.clear();
            _vertex_status_map.fill(PRE_HEAP);
            return *this;
        }
        constexpr bentley_ottman & add_source(
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
            prefetch_mapped_values(out_arcs_range,
                                   arc_targets_map(_graph.get()));
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
                    _heap.push(w, traits::semiring::plus(st_dist,
                                                         _length_map.get()[a]));
                    _vertex_status_map[w] = IN_HEAP;
                    if constexpr(traits::store_paths) {
                        _pred_arcs_map[w].emplace(a);
                        if constexpr(!has_arc_source<G>)
                            _pred_vertices_map[w] = t;
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
        [[nodiscard]] constexpr vertex pred_vertex(
            const vertex & u) const noexcept
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

#endif  // MELON_ALGORITHM_BENTLEY_OTTMANN_HPP
