#ifndef MELON_ALGORITHM_NETWORK_VORONOI_HPP
#define MELON_ALGORITHM_NETWORK_VORONOI_HPP

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
concept network_voronoi_trait = semiring<typename _Traits::semiring> &&
    updatable_priority_queue<typename _Traits::heap> && requires() {
    { _Traits::store_cluter_adjacency } -> std::convertible_to<bool>;
};
// clang-format on

template <typename _Graph, typename _ValueType>
struct network_voronoi_default_traits {
    using semiring = shortest_path_semiring<_ValueType>;
    using cluster_id_t = unsigned int;
    using entry = std::pair<_ValueType, cluster_id_t>;
    struct entry_cmp {
        [[nodiscard]] constexpr bool operator()(const entry & e1,
                                                const entry & e2) const {
            if(e1.first == e2.first) {
                return e1.second < e2.second;
            }
            return semiring::less(e1.first, e2.first);
        }
    };
    using heap =
        updatable_d_ary_heap<2, std::pair<vertex_t<_Graph>, entry>, entry_cmp,
                             vertex_map_t<_Graph, std::size_t>,
                             views::element_map<1>, views::element_map<0>>;

    static constexpr bool store_cluter_adjacency = false;
};

template <outward_incidence_graph _Graph,
          input_mapping<arc_t<_Graph>> _LengthMap,
          network_voronoi_trait _Traits>
    requires has_vertex_map<_Graph>
class network_voronoi : public algorithm_view_interface<
                            network_voronoi<_Graph, _LengthMap, _Traits>> {
private:
    using vertex = vertex_t<_Graph>;
    using arc = arc_t<_Graph>;

    using cluster_id_t = typename _Traits::cluster_id_t;
    using entry_t = typename _Traits::entry;
    using entry_cmp = typename _Traits::entry_cmp;

    using length_type = mapped_value_t<_LengthMap, arc_t<_Graph>>;
    using traversal_entry = std::pair<vertex, length_type>;

    using heap = _Traits::heap;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    static_assert(
        std::is_same_v<typename _Traits::heap::value_type,
                       std::pair<vertex, std::pair<length_type, cluster_id_t>>>,
        "network_voronoi requires matching value_type with heap.");

private:
    _Graph _graph;
    _LengthMap _length_map;
    heap _heap;
    vertex_map_t<_Graph, vertex_status> _vertex_status_map;
    [[no_unique_address]] entry_cmp _entry_cmp;

public:
    template <typename _G, typename _M>
    [[nodiscard]] constexpr network_voronoi(_G && g, _M && l)
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _length_map(views::mapping_all(std::forward<_M>(l)))
        , _heap(_entry_cmp, create_vertex_map<std::size_t>(_graph))
        , _vertex_status_map(
              create_vertex_map<vertex_status>(_graph, PRE_HEAP)) {}

    template <typename _G, typename _M, typename _K>
    [[nodiscard]] constexpr network_voronoi(_G && g, _M && l, _K && k)
        : network_voronoi(std::forward<_G>(g), std::forward<_M>(l)) {
        set_kernels(std::forward<_K>(k));
    }

    template <typename... _Args>
    [[nodiscard]] constexpr network_voronoi(_Traits, _Args &&... args)
        : network_voronoi(std::forward<_Args>(args)...) {}

    [[nodiscard]] constexpr network_voronoi(const network_voronoi &) = default;
    [[nodiscard]] constexpr network_voronoi(network_voronoi &&) = default;

    constexpr network_voronoi & operator=(const network_voronoi &) = default;
    constexpr network_voronoi & operator=(network_voronoi &&) = default;

    constexpr network_voronoi & reset() noexcept {
        _heap.clear();
        _vertex_status_map.fill(PRE_HEAP);
        return *this;
    }
    template <std::ranges::range K>
    constexpr network_voronoi & set_kernels(K && kernels) noexcept {
        assert(_heap.empty());
        for(auto && k : kernels) {
            assert(_vertex_status_map[k] != IN_HEAP);
            _heap.push(std::make_pair(k, entry_t{_Traits::semiring::zero, k}));
            _vertex_status_map[k] = IN_HEAP;
        }
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _heap.empty();
    }

    [[nodiscard]] constexpr auto current() const noexcept {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() noexcept {
        assert(!finished());
        const auto [t, st_dist] = _heap.top();
        _vertex_status_map[t] = POST_HEAP;
        auto && out_arcs_range = melon::out_arcs(_graph, t);
        prefetch_range(out_arcs_range);
        prefetch_mapped_values(out_arcs_range, arc_targets_map(_graph));
        prefetch_mapped_values(out_arcs_range, _length_map);
        _heap.pop();
        for(const arc & a : out_arcs_range) {
            const vertex & w = melon::arc_target(_graph, a);
            const vertex_status & w_status = _vertex_status_map[w];
            const entry_t new_dist = {
                _Traits::semiring::plus(st_dist.first, _length_map[a]),
                st_dist.second};
            if(w_status == IN_HEAP) {
                if(_entry_cmp(new_dist, _heap.priority(w))) {
                    _heap.promote(w, new_dist);
                }
            } else if(w_status == PRE_HEAP) {
                _heap.push(std::make_pair(w, new_dist));
                _vertex_status_map[w] = IN_HEAP;
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
};

template <typename _Graph, typename _LengthMap,
          typename _Traits = network_voronoi_default_traits<
              _Graph, mapped_value_t<_LengthMap, arc_t<_Graph>>>>
network_voronoi(_Graph &&, _LengthMap &&)
    -> network_voronoi<views::graph_all_t<_Graph>,
                       views::mapping_all_t<_LengthMap>, _Traits>;

template <typename _Graph, typename _LengthMap, typename _Traits>
network_voronoi(_Traits, _Graph &&, _LengthMap &&)
    -> network_voronoi<views::graph_all_t<_Graph>,
                       views::mapping_all_t<_LengthMap>, _Traits>;

template <typename _Graph, typename _LengthMap, typename _Kernels,
          typename _Traits = network_voronoi_default_traits<
              _Graph, mapped_value_t<_LengthMap, arc_t<_Graph>>>>
network_voronoi(_Graph &&, _LengthMap &&, _Kernels &&)
    -> network_voronoi<views::graph_all_t<_Graph>,
                       views::mapping_all_t<_LengthMap>, _Traits>;

template <typename _Graph, typename _LengthMap, typename _Kernels,
          typename _Traits>
network_voronoi(_Traits, _Graph &&, _LengthMap &&, _Kernels &&)
    -> network_voronoi<views::graph_all_t<_Graph>,
                       views::mapping_all_t<_LengthMap>, _Traits>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_NETWORK_VORONOI_HPP
