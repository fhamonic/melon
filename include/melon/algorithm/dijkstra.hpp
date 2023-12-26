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
#include "melon/detail/intrusive_view.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/views/graph_view.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
template <typename _Traits>
concept dijkstra_trait = semiring<typename _Traits::semiring> &&
    updatable_priority_queue<typename _Traits::heap> && requires() {
    { _Traits::store_distances } -> std::convertible_to<bool>;
    { _Traits::store_paths } -> std::convertible_to<bool>;
};
// clang-format on

template <typename _Graph, typename _LengthMap>
struct dijkstra_default_traits {
    using semiring =
        shortest_path_semiring<mapped_value_t<_LengthMap, arc_t<_Graph>>>;
    struct entry_cmp {
        [[nodiscard]] constexpr bool operator()(
            const auto & e1, const auto & e2) const noexcept {
            return semiring::less(e1.second, e2.second);
        }
    };
    using heap = d_ary_heap<2, vertex_t<_Graph>,
                            mapped_value_t<_LengthMap, arc_t<_Graph>>,
                            entry_cmp, vertex_map_t<_Graph, std::size_t>>;

    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};

template <outward_incidence_graph _Graph,
          input_mapping<arc_t<_Graph>> _LengthMap, dijkstra_trait _Traits>
    requires has_vertex_map<_Graph>
class dijkstra {
private:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;
    using value_t = mapped_value_t<_LengthMap, arc_t<_Graph>>;
    using traversal_entry = std::pair<vertex, value_t>;

    static_assert(
        std::is_same_v<traversal_entry, typename _Traits::heap::entry>,
        "traversal_entry != heap_entry");

    using heap = _Traits::heap;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

private:
    _Graph _graph;
    _LengthMap _length_map;
    heap _heap;
    vertex_map_t<_Graph, vertex_status> _vertex_status_map;

    [[no_unique_address]] vertex_map_if<
        _Traits::store_paths && !has_arc_source<_Graph>, _Graph, vertex>
        _pred_vertices_map;
    [[no_unique_address]] vertex_map_if<_Traits::store_paths, _Graph,
                                        std::optional<arc>>
        _pred_arcs_map;
    [[no_unique_address]] vertex_map_if<_Traits::store_distances, _Graph,
                                        double>
        _distances_map;

public:
    template <typename _G, typename _M>
    [[nodiscard]] constexpr dijkstra(_G && g, _M && l)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _length_map(views::mapping_all(std::forward<_M>(l)))
        , _heap(create_vertex_map<std::size_t>(_graph))
        , _vertex_status_map(create_vertex_map<vertex_status>(_graph, PRE_HEAP))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _distances_map(_graph) {}

    template <typename _G, typename _M>
    [[nodiscard]] constexpr dijkstra(_G && g, _M && l, const vertex & s)
        : dijkstra(std::forward<_G>(g), std::forward<_M>(l)) {
        add_source(s);
    }

    template <typename... _Args>
    [[nodiscard]] constexpr dijkstra(_Traits, _Args &&... args)
        : dijkstra(std::forward<_Args>(args)...) {}

    [[nodiscard]] constexpr dijkstra(const dijkstra &) = default;
    [[nodiscard]] constexpr dijkstra(dijkstra &&) = default;

    constexpr dijkstra & operator=(const dijkstra &) = default;
    constexpr dijkstra & operator=(dijkstra &&) = default;

    constexpr dijkstra & reset() noexcept {
        _heap.clear();
        _vertex_status_map.fill(PRE_HEAP);
        return *this;
    }
    constexpr dijkstra & add_source(
        const vertex & s,
        const value_t & dist = _Traits::semiring::zero) noexcept {
        assert(_vertex_status_map[s] != IN_HEAP);
        _heap.push(s, dist);
        _vertex_status_map[s] = IN_HEAP;
        if constexpr(_Traits::store_paths) {
            _pred_arcs_map[s].reset();
            if constexpr(!has_arc_source<_Graph>) _pred_vertices_map[s] = s;
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
        if constexpr(_Traits::store_distances) _distances_map[t] = st_dist;
        _vertex_status_map[t] = POST_HEAP;
        auto && out_arcs_range = melon::out_arcs(_graph, t);
        prefetch_range(out_arcs_range);
        prefetch_mapped_values(out_arcs_range, arc_targets_map(_graph));
        prefetch_mapped_values(out_arcs_range, _length_map);
        _heap.pop();
        for(const arc & a : out_arcs_range) {
            const vertex & w = melon::arc_target(_graph, a);
            const vertex_status & w_status = _vertex_status_map[w];
            if(w_status == IN_HEAP) {
                const value_t new_dist =
                    _Traits::semiring::plus(st_dist, _length_map[a]);
                if(_Traits::semiring::less(new_dist, _heap.priority(w))) {
                    _heap.promote(w, new_dist);
                    if constexpr(_Traits::store_paths) {
                        _pred_arcs_map[w].emplace(a);
                        if constexpr(!has_arc_source<_Graph>)
                            _pred_vertices_map[w] = t;
                    }
                }
            } else if(w_status == PRE_HEAP) {
                _heap.push(w, _Traits::semiring::plus(st_dist, _length_map[a]));
                _vertex_status_map[w] = IN_HEAP;
                if constexpr(_Traits::store_paths) {
                    _pred_arcs_map[w].emplace(a);
                    if constexpr(!has_arc_source<_Graph>)
                        _pred_vertices_map[w] = t;
                }
            }
        }
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }
    [[nodiscard]] constexpr auto begin() noexcept {
        return algorithm_iterator(*this);
    }
    [[nodiscard]] constexpr auto end() noexcept {
        return algorithm_end_sentinel();
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _vertex_status_map[u] != PRE_HEAP;
    }
    [[nodiscard]] constexpr bool visited(const vertex & u) const noexcept {
        return _vertex_status_map[u] == POST_HEAP;
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const noexcept
        requires(_Traits::store_paths)
    {
        assert(reached(u));
        return _pred_arcs_map[u].value();
    }
    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const noexcept
        requires(_Traits::store_paths)
    {
        assert(reached(u) && _pred_arcs_map[u].has_value());
        if constexpr(has_arc_source<_Graph>)
            return melon::arc_source(_graph, pred_arc(u));
        else
            return _pred_vertices_map[u];
    }
    [[nodiscard]] constexpr value_t current_dist(
        const vertex & u) const noexcept
        requires(_Traits::store_distances)
    {
        assert(reached(u) && !visited(u));
        return _heap.priority(u);
    }
    [[nodiscard]] constexpr value_t dist(const vertex & u) const noexcept
        requires(_Traits::store_distances)
    {
        assert(visited(u));
        return _distances_map[u];
    }

    [[nodiscard]] constexpr auto path_to(const vertex & t) const noexcept
        requires(_Traits::store_paths)
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

template <typename _Graph, typename _LengthMap,
          typename _Traits = dijkstra_default_traits<_Graph, _LengthMap>>
dijkstra(_Graph &&, _LengthMap &&)
    -> dijkstra<views::graph_all_t<_Graph>, views::mapping_all_t<_LengthMap>,
                _Traits>;

template <typename _Graph, typename _LengthMap,
          typename _Traits = dijkstra_default_traits<_Graph, _LengthMap>>
dijkstra(_Graph &&, _LengthMap &&, const vertex_t<_Graph> &)
    -> dijkstra<views::graph_all_t<_Graph>, views::mapping_all_t<_LengthMap>,
                _Traits>;

template <typename _Graph, typename _LengthMap, typename _Traits>
dijkstra(_Traits, _Graph &&, _LengthMap &&)
    -> dijkstra<views::graph_all_t<_Graph>, views::mapping_all_t<_LengthMap>,
                _Traits>;

template <typename _Graph, typename _LengthMap, typename _Traits>
dijkstra(_Traits, _Graph &&, _LengthMap &&, const vertex_t<_Graph> &)
    -> dijkstra<views::graph_all_t<_Graph>, views::mapping_all_t<_LengthMap>,
                _Traits>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_DIJKSTA_HPP
