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
#include "melon/detail/intrusive_iterator_base.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"
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

template <typename _Graph, typename _ValueType>
struct dijkstra_default_traits {
    using semiring = shortest_path_semiring<_ValueType>;
    using heap =
        updatable_d_ary_heap<2, std::pair<vertex_t<_Graph>, _ValueType>,
                             typename semiring::less_t,
                             vertex_map_t<_Graph, std::size_t>,
                             views::element_map<1>, views::element_map<0>>;

    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};

template <outward_incidence_graph _Graph,
          input_mapping<arc_t<_Graph>> _LengthMap, dijkstra_trait _Traits>
    requires has_vertex_map<_Graph>
class dijkstra : public algorithm_view_interface<
                     dijkstra<_Graph, _LengthMap, _Traits>> {
private:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;

    using length_type = mapped_value_t<_LengthMap, arc_t<_Graph>>;
    using traversal_entry = std::pair<vertex, length_type>;

    using heap = _Traits::heap;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    static_assert(std::is_same_v<typename heap::value_type,
                                 std::pair<vertex, length_type>>,
                  "dijkstras requires heap entries type.");

private:
    _Graph _graph;
    _LengthMap _length_map;
    heap _heap;
    vertex_map_t<_Graph, vertex_status> _vertex_status_map;

    [[no_unique_address]] vertex_map_if<_Traits::store_paths &&
                                            !has_arc_source<_Graph>,
                                        _Graph, vertex> _pred_vertices_map;
    [[no_unique_address]] vertex_map_if<_Traits::store_paths, _Graph,
                                        std::optional<arc>> _pred_arcs_map;
    [[no_unique_address]] vertex_map_if<_Traits::store_distances, _Graph,
                                        double> _distances_map;

public:
    template <typename _G, typename _M>
    [[nodiscard]] constexpr dijkstra(_G && g, _M && l)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _length_map(views::mapping_all(std::forward<_M>(l)))
        , _heap(typename _Traits::semiring::less_t(),
                create_vertex_map<std::size_t>(_graph))
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
        const length_type & dist = _Traits::semiring::zero) noexcept {
        assert(_vertex_status_map[s] != IN_HEAP);
        _heap.push(std::make_pair(s, dist));
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
                const length_type new_dist =
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
                _heap.push(std::make_pair(
                    w, _Traits::semiring::plus(st_dist, _length_map[a])));
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
    [[nodiscard]] constexpr length_type current_dist(
        const vertex & u) const noexcept
        requires(_Traits::store_distances)
    {
        assert(reached(u) && !visited(u));
        return _heap.priority(u);
    }
    [[nodiscard]] constexpr length_type dist(const vertex & u) const noexcept
        requires(_Traits::store_distances)
    {
        assert(visited(u));
        return _distances_map[u];
    }

private:
    class path_iterator : public intrusive_iterator_base<dijkstra, vertex> {
    public:
        using value_type = arc;
        using reference = arc;
        using intrusive_iterator_base<dijkstra,
                                      vertex>::intrusive_iterator_base;

        constexpr const reference operator*() const {
            return this->_structure->_pred_arcs_map[this->_cursor].value();
        }
        constexpr path_iterator & operator++() noexcept {
            this->_cursor = this->_structure->pred_vertex(this->_cursor);
            return *this;
        }
        constexpr path_iterator operator++(int) noexcept {
            path_iterator it(*this);
            operator++();
            return it;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const path_iterator & it, std::default_sentinel_t) noexcept {
            return !it._structure->_pred_arcs_map[it._cursor].has_value();
        }
    };

public:
    [[nodiscard]] constexpr auto path_to(const vertex & t) const noexcept
        requires(_Traits::store_paths)
    {
        assert(reached(t));
        return std::ranges::subrange(path_iterator(this, t),
                                     std::default_sentinel);
    }
};

template <typename _Graph, typename _LengthMap,
          typename _Traits = dijkstra_default_traits<
              _Graph, mapped_value_t<_LengthMap, arc_t<_Graph>>>>
dijkstra(_Graph &&, _LengthMap &&)
    -> dijkstra<views::graph_all_t<_Graph>, views::mapping_all_t<_LengthMap>,
                _Traits>;

template <typename _Graph, typename _LengthMap,
          typename _Traits = dijkstra_default_traits<
              _Graph, mapped_value_t<_LengthMap, arc_t<_Graph>>>>
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
