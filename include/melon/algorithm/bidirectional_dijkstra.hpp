#ifndef MELON_ALGORITHM_BIDIRECTIONAL_DIJKSTA_HPP
#define MELON_ALGORITHM_BIDIRECTIONAL_DIJKSTA_HPP

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
#include "melon/detail/intrusive_view.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
template <typename _Traits>
concept bidirectional_dijkstra_trait = semiring<typename _Traits::semiring> &&
    updatable_priority_queue<typename _Traits::heap> && requires() {
    { _Traits::store_path } -> std::convertible_to<bool>;
};
// clang-format on

template <typename _Graph, typename _ValueType>
struct bidirectional_dijkstra_default_traits {
    using semiring = shortest_path_semiring<_ValueType>;
    using heap =
        updatable_d_ary_heap<2, std::pair<vertex_t<_Graph>, _ValueType>,
                             typename semiring::less_t,
                             vertex_map_t<_Graph, std::size_t>,
                             views::get_map<1>, views::get_map<0>>;

    static constexpr bool store_path = true;
};

template <outward_incidence_graph _Graph,
          input_mapping<arc_t<_Graph>> _LengthMap,
          bidirectional_dijkstra_trait _Traits>
    requires inward_incidence_graph<_Graph> && has_vertex_map<_Graph>
class bidirectional_dijkstra {
private:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;
    using length_type = mapped_value_t<_LengthMap, vertex>;

    using heap = _Traits::heap;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    static_assert(std::is_same_v<typename heap::value_type,
                                 std::pair<vertex, length_type>>,
                  "bidirectional_dijkstra requires heap entries type.");

    using optional_arc = std::optional<arc>;
    struct no_forward_pred_arcs_map {};
    using forward_pred_arcs_map =
        vertex_map_if<_Traits::store_path, _Graph, optional_arc,
                      no_forward_pred_arcs_map>;
    struct no_reverse_pred_arcs_map {};
    using reverse_pred_arcs_map =
        vertex_map_if<_Traits::store_path, _Graph, optional_arc,
                      no_reverse_pred_arcs_map>;
    struct no_optional_midpoint {};
    using optional_midpoint =
        std::conditional_t<_Traits::store_path, std::optional<vertex>,
                           no_optional_midpoint>;

private:
    _Graph _graph;
    _LengthMap _length_map;
    heap _forward_heap;
    heap _reverse_heap;
    vertex_map_t<_Graph, std::pair<vertex_status, vertex_status>>
        _vertex_status_map;

    [[no_unique_address]] forward_pred_arcs_map _forward_pred_arcs_map;
    [[no_unique_address]] reverse_pred_arcs_map _reverse_pred_arcs_map;
    [[no_unique_address]] optional_midpoint _midpoint;

public:
    template <typename _G, typename _M>
    [[nodiscard]] constexpr bidirectional_dijkstra(_G && g, _M && l)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _length_map(views::mapping_all(std::forward<_M>(l)))
        , _forward_heap(typename _Traits::semiring::less_t(),
                        create_vertex_map<std::size_t>(_graph))
        , _reverse_heap(typename _Traits::semiring::less_t(),
                        create_vertex_map<std::size_t>(_graph))
        , _vertex_status_map(_graph.template create_vertex_map<
                             std::pair<vertex_status, vertex_status>>(
              std::make_pair(PRE_HEAP, PRE_HEAP)))
        , _forward_pred_arcs_map(_graph)
        , _reverse_pred_arcs_map(_graph) {}

    template <typename _G, typename _M>
    [[nodiscard]] constexpr bidirectional_dijkstra(_G && g, _M && l,
                                                   const vertex & s,
                                                   const vertex & t)
        : bidirectional_dijkstra(std::forward<_G>(g), std::forward<_M>(l)) {
        add_source(s);
        add_target(t);
    }

    bidirectional_dijkstra & reset() noexcept {
        _forward_heap.clear();
        _reverse_heap.clear();
        _vertex_status_map.fill(std::make_pair(PRE_HEAP, PRE_HEAP));
        _midpoint.reset();
        return *this;
    }
    bidirectional_dijkstra & add_source(
        const vertex & s,
        const length_type dist = _Traits::semiring::zero) noexcept {
        assert(_vertex_status_map[s].first == PRE_HEAP);
        _forward_heap.push(std::make_pair(s, dist));
        _vertex_status_map[s].first = IN_HEAP;
        if constexpr(_Traits::store_path) _forward_pred_arcs_map[s].reset();
        return *this;
    }
    bidirectional_dijkstra & add_target(
        const vertex & t,
        const length_type dist = _Traits::semiring::zero) noexcept {
        assert(_vertex_status_map[t].second == PRE_HEAP);
        _reverse_heap.push(std::make_pair(t, dist));
        _vertex_status_map[t].second = IN_HEAP;
        if constexpr(_Traits::store_path) _reverse_pred_arcs_map[t].reset();
        return *this;
    }

public:
    constexpr length_type run() noexcept {
        length_type st_dist = _Traits::semiring::infty;
        while(!_forward_heap.empty() && !_reverse_heap.empty()) {
            const auto && [u1, u1_dist] = _forward_heap.top();
            const auto && [u2, u2_dist] = _reverse_heap.top();
            if(_Traits::semiring::less(
                   st_dist, _Traits::semiring::plus(u1_dist, u2_dist)))
                break;
            if(_Traits::semiring::less(u1_dist, u2_dist)) {
                const auto & out_arcs_range = out_arcs(_graph, u1);
                prefetch_range(out_arcs_range);
                prefetch_mapped_values(out_arcs_range, arc_targets_map(_graph));
                prefetch_mapped_values(out_arcs_range, _length_map);
                _vertex_status_map[u1].first = POST_HEAP;
                _forward_heap.pop();
                for(const arc a : out_arcs_range) {
                    const vertex w = arc_target(_graph, a);
                    auto [w_forward_status, w_reverse_status] =
                        _vertex_status_map[w];
                    if(w_forward_status == IN_HEAP) {
                        const length_type new_w_dist =
                            _Traits::semiring::plus(u1_dist, _length_map[a]);
                        if(_Traits::semiring::less(new_w_dist,
                                                   _forward_heap.priority(w))) {
                            _forward_heap.promote(w, new_w_dist);
                            if(w_reverse_status == IN_HEAP) {
                                const length_type new_st_dist =
                                    new_w_dist + _reverse_heap.priority(w);
                                if(_Traits::semiring::less(new_st_dist,
                                                           st_dist)) {
                                    st_dist = new_st_dist;
                                    _midpoint.emplace(w);
                                }
                            }
                            if constexpr(_Traits::store_path)
                                _forward_pred_arcs_map[w].emplace(a);
                        }
                    } else if(w_forward_status == PRE_HEAP) {
                        const length_type new_w_dist =
                            _Traits::semiring::plus(u1_dist, _length_map[a]);
                        _forward_heap.push(std::make_pair(w, new_w_dist));
                        _vertex_status_map[w].first = IN_HEAP;
                        if(w_reverse_status == IN_HEAP) {
                            const length_type new_st_dist =
                                new_w_dist + _reverse_heap.priority(w);
                            if(_Traits::semiring::less(new_st_dist, st_dist)) {
                                st_dist = new_st_dist;
                                _midpoint.emplace(w);
                            }
                        }
                        if constexpr(_Traits::store_path)
                            _forward_pred_arcs_map[w].emplace(a);
                    }
                }
            } else {
                const auto & in_arcs_range = in_arcs(_graph, u2);
                prefetch_range(in_arcs_range);
                prefetch_mapped_values(in_arcs_range, arc_sources_map(_graph));
                prefetch_mapped_values(in_arcs_range, _length_map);
                _vertex_status_map[u2].second = POST_HEAP;
                _reverse_heap.pop();
                for(const arc a : in_arcs_range) {
                    const vertex w = arc_source(_graph, a);
                    auto [w_forward_status, w_reverse_status] =
                        _vertex_status_map[w];
                    if(w_reverse_status == IN_HEAP) {
                        const length_type new_w_dist =
                            _Traits::semiring::plus(u2_dist, _length_map[a]);
                        if(_Traits::semiring::less(new_w_dist,
                                                   _reverse_heap.priority(w))) {
                            _reverse_heap.promote(w, new_w_dist);
                            if(w_forward_status == IN_HEAP) {
                                const length_type new_st_dist =
                                    new_w_dist + _forward_heap.priority(w);
                                if(_Traits::semiring::less(new_st_dist,
                                                           st_dist)) {
                                    st_dist = new_st_dist;
                                    _midpoint.emplace(w);
                                }
                            }
                            if constexpr(_Traits::store_path)
                                _reverse_pred_arcs_map[w].emplace(a);
                        }
                    } else if(w_reverse_status == PRE_HEAP) {
                        const length_type new_w_dist =
                            _Traits::semiring::plus(u2_dist, _length_map[a]);
                        _reverse_heap.push(std::make_pair(w, new_w_dist));
                        _vertex_status_map[w].second = IN_HEAP;
                        if(w_forward_status == IN_HEAP) {
                            const length_type new_st_dist =
                                new_w_dist + _forward_heap.priority(w);
                            if(_Traits::semiring::less(new_st_dist, st_dist)) {
                                st_dist = new_st_dist;
                                _midpoint.emplace(w);
                            }
                        }
                        if constexpr(_Traits::store_path)
                            _reverse_pred_arcs_map[w].emplace(a);
                    }
                }
            }
        }
        return st_dist;
    }

    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const noexcept
        requires(_Traits::store_path)
    {
        assert(_vertex_status_map[u].first != PRE_HEAP);
        return _forward_pred_arcs_map[u].value();
    }
    [[nodiscard]] constexpr arc succ_arc(const vertex & u) const noexcept
        requires(_Traits::store_path)
    {
        assert(_vertex_status_map[u].second != PRE_HEAP);
        return _reverse_pred_arcs_map[u].value();
    }
    [[nodiscard]] constexpr bool path_found() const noexcept
        requires(_Traits::store_path)
    {
        return _midpoint.has_value();
    }
    [[nodiscard]] constexpr auto path() const noexcept
        requires(_Traits::store_path)
    {
        assert(path_found());
        return std::ranges::views::concat(
            intrusive_view(
                _forward_pred_arcs_map[_midpoint.value()],
                [](const std::optional<arc> & oa) -> arc { return oa.value(); },
                [this](const std::optional<arc> & oa) -> optional_arc {
                    return _forward_pred_arcs_map[arc_source(_graph,
                                                             oa.value())];
                },
                [](const std::optional<arc> & oa) -> bool {
                    return oa.has_value();
                }),
            intrusive_view(
                _reverse_pred_arcs_map[_midpoint.value()],
                [](const std::optional<arc> & oa) -> arc { return oa.value(); },
                [this](const std::optional<arc> & oa) -> optional_arc {
                    return _reverse_pred_arcs_map[arc_target(_graph,
                                                             oa.value())];
                },
                [](const std::optional<arc> & oa) -> bool {
                    return oa.has_value();
                }));
    }
};

template <typename _Graph, typename _LengthMap,
          typename _Traits = bidirectional_dijkstra_default_traits<
              _Graph, mapped_value_t<_LengthMap, arc_t<_Graph>>>>
bidirectional_dijkstra(_Graph &&, _LengthMap &&)
    -> bidirectional_dijkstra<views::graph_all_t<_Graph>,
                              views::mapping_all_t<_LengthMap>, _Traits>;

template <typename _Graph, typename _LengthMap,
          typename _Traits = bidirectional_dijkstra_default_traits<
              _Graph, mapped_value_t<_LengthMap, arc_t<_Graph>>>>
bidirectional_dijkstra(_Graph &&, _LengthMap &&, const vertex_t<_Graph> &,
                       const vertex_t<_Graph> &)
    -> bidirectional_dijkstra<views::graph_all_t<_Graph>,
                              views::mapping_all_t<_LengthMap>, _Traits>;

template <typename _Graph, typename _LengthMap, typename _Traits>
bidirectional_dijkstra(_Traits, _Graph &&, _LengthMap &&)
    -> bidirectional_dijkstra<views::graph_all_t<_Graph>,
                              views::mapping_all_t<_LengthMap>, _Traits>;

template <typename _Graph, typename _LengthMap, typename _Traits>
bidirectional_dijkstra(_Traits, _Graph &&, _LengthMap &&,
                       const vertex_t<_Graph> &, const vertex_t<_Graph> &)
    -> bidirectional_dijkstra<views::graph_all_t<_Graph>,
                              views::mapping_all_t<_LengthMap>, _Traits>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_BIDIRECTIONAL_DIJKSTA_HPP
