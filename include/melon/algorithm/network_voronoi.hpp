#pragma once

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

namespace melon {

// clang-format off
template <typename Traits>
concept network_voronoi_trait = semiring<typename Traits::semiring> &&
    updatable_priority_queue<typename Traits::heap> && requires() {
    { Traits::store_cluster_adjacency } -> std::convertible_to<bool>;
};
// clang-format on

template <typename Graph, typename ValueType>
struct network_voronoi_default_traits {
    using semiring = shortest_path_semiring<ValueType>;
    using cluster_id_t = unsigned int;
    using entry = std::pair<ValueType, cluster_id_t>;
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
        updatable_d_ary_heap<2, std::pair<vertex_t<Graph>, entry>, entry_cmp,
                             vertex_map_t<Graph, std::size_t>,
                             views::element_map<1>, views::element_map<0>>;

    static constexpr bool store_cluster_adjacency = false;
};

// Multi-source Dijkstra partitioning the graph into Voronoi cells: every
// vertex reachable from the kernels given to set_kernels() is assigned to the
// nearest one. Iterating yields (vertex, (distance, kernel)) pairs in order of
// increasing distance, each vertex exactly once, where the second member
// identifies the cell the vertex fell into. Same non-negativity requirement as
// melon::dijkstra.
// O((m + n) log n) with the default binary heap.
template <outward_incidence_graph Graph, input_mapping<arc_t<Graph>> LengthMap,
          network_voronoi_trait Traits>
    requires has_vertex_map<Graph>
class network_voronoi : public algorithm_view_interface<
                            network_voronoi<Graph, LengthMap, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    using cluster_id_t = typename Traits::cluster_id_t;
    using entry_t = typename Traits::entry;
    using entry_cmp = typename Traits::entry_cmp;

    using length_type = mapped_value_t<LengthMap, arc_t<Graph>>;
    using traversal_entry = std::pair<vertex, length_type>;

    using heap = Traits::heap;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    static_assert(
        std::is_same_v<typename Traits::heap::value_type,
                       std::pair<vertex, std::pair<length_type, cluster_id_t>>>,
        "network_voronoi requires matching value_type with heap.");

private:
    Graph _graph;
    LengthMap _length_map;
    heap _heap;
    vertex_map_t<Graph, vertex_status> _vertex_status_map;
    [[no_unique_address]] entry_cmp _entry_cmp;

public:
    template <typename G, typename M>
    [[nodiscard]] constexpr network_voronoi(G && g, M && l)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _length_map(views::mapping_all(std::forward<M>(l)))
        , _heap(_entry_cmp, create_vertex_map<std::size_t>(_graph))
        , _vertex_status_map(
              create_vertex_map<vertex_status>(_graph, PRE_HEAP)) {}

    template <typename G, typename M, typename K>
    [[nodiscard]] constexpr network_voronoi(G && g, M && l, K && k)
        : network_voronoi(std::forward<G>(g), std::forward<M>(l)) {
        set_kernels(std::forward<K>(k));
    }

    template <typename... Args>
    [[nodiscard]] constexpr network_voronoi(Traits, Args &&... args)
        : network_voronoi(std::forward<Args>(args)...) {}

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
            _heap.push(std::make_pair(k, entry_t{Traits::semiring::zero, k}));
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
                Traits::semiring::plus(st_dist.first, _length_map[a]),
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

template <
    typename Graph, typename LengthMap,
    typename Traits = network_voronoi_default_traits<
        Graph, mapped_value_t<views::mapping_all_t<LengthMap>, arc_t<Graph>>>>
network_voronoi(Graph &&, LengthMap &&)
    -> network_voronoi<views::graph_all_t<Graph>,
                       views::mapping_all_t<LengthMap>, Traits>;

template <typename Graph, typename LengthMap, typename Traits>
network_voronoi(Traits, Graph &&, LengthMap &&)
    -> network_voronoi<views::graph_all_t<Graph>,
                       views::mapping_all_t<LengthMap>, Traits>;

template <
    typename Graph, typename LengthMap, typename Kernels,
    typename Traits = network_voronoi_default_traits<
        Graph, mapped_value_t<views::mapping_all_t<LengthMap>, arc_t<Graph>>>>
network_voronoi(Graph &&, LengthMap &&, Kernels &&)
    -> network_voronoi<views::graph_all_t<Graph>,
                       views::mapping_all_t<LengthMap>, Traits>;

template <typename Graph, typename LengthMap, typename Kernels, typename Traits>
network_voronoi(Traits, Graph &&, LengthMap &&, Kernels &&)
    -> network_voronoi<views::graph_all_t<Graph>,
                       views::mapping_all_t<LengthMap>, Traits>;

}  // namespace melon
