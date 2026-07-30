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
concept network_voronoi_traits = semiring<typename Traits::semiring> &&
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
                             maps::element_map<1>, maps::element_map<0>>;

    static constexpr bool store_cluster_adjacency = false;
};

// Multi-source Dijkstra partitioning the graph into Voronoi cells: every
// vertex reachable from the kernels given to set_kernels() is assigned to the
// nearest one. Iterating yields (vertex, (distance, kernel)) pairs in order of
// increasing distance, each vertex exactly once, where the second member
// identifies the cell the vertex fell into. Same non-negativity requirement as
// melon::dijkstra.
// O((m + n) log n) with the default binary heap.
template <graph_view Graph, mapping_view<arc_t<Graph>> LengthMap,
          network_voronoi_traits Traits = network_voronoi_default_traits<
              Graph, mapped_value_t<LengthMap, arc_t<Graph>>>>
    requires outward_incidence_graph<Graph> && has_vertex_map<Graph>
class network_voronoi : public algorithm_view_interface<
                            network_voronoi<Graph, LengthMap, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    using cluster_id_t = typename Traits::cluster_id_t;
    using entry_t = typename Traits::entry;
    using entry_cmp = typename Traits::entry_cmp;

    using length_type = mapped_value_t<LengthMap, arc_t<Graph>>;

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
        requires graph_storable_as<G, Graph> &&
                     mapping_storable_as<M, LengthMap>
    constexpr network_voronoi(G && g, M && l)
        : _graph(detail::store_graph<Graph>(std::forward<G>(g)))
        , _length_map(detail::store_mapping<LengthMap>(std::forward<M>(l)))
        , _heap(_entry_cmp, create_vertex_map<std::size_t>(_graph))
        , _vertex_status_map(
              create_vertex_map<vertex_status>(_graph, PRE_HEAP)) {}

    template <typename G, typename M, typename K>
        requires graph_storable_as<G, Graph> &&
                 mapping_storable_as<M, LengthMap>
    constexpr network_voronoi(G && g, M && l, K && k)
        : network_voronoi(std::forward<G>(g), std::forward<M>(l)) {
        set_kernels(std::forward<K>(k));
    }

    template <typename... Args>
        requires std::constructible_from<network_voronoi, Args...>
    constexpr network_voronoi(Traits, Args &&... args)
        : network_voronoi(std::forward<Args>(args)...) {}

    constexpr network_voronoi(const network_voronoi &) = default;
    constexpr network_voronoi(network_voronoi &&) = default;

    constexpr network_voronoi & operator=(const network_voronoi &) = default;
    constexpr network_voronoi & operator=(network_voronoi &&) = default;

    // The graph the algorithm runs over. An algorithm owns its view rather
    // than adapting it, so this is the std::ranges::owning_view shape --
    // references, ref-qualified -- and not the filter_view shape the graph
    // *views* use. Returning a copy here would also put traversal_forest back
    // where it started: it reaches its sources through base(), and an owned
    // graph view is move-only.
    [[nodiscard]] constexpr Graph & base() & noexcept { return _graph; }
    [[nodiscard]] constexpr const Graph & base() const & noexcept {
        return _graph;
    }
    [[nodiscard]] constexpr Graph && base() && noexcept {
        return std::move(_graph);
    }
    [[nodiscard]] constexpr const Graph && base() const && noexcept {
        return std::move(_graph);
    }

    constexpr network_voronoi & reset() {
        _heap.clear();
        _vertex_status_map.fill(PRE_HEAP);
        return *this;
    }
    template <std::ranges::range K>
    constexpr network_voronoi & set_kernels(K && kernels) {
        assert(_heap.empty());
        for(auto && k : kernels) {
            assert(_vertex_status_map[k] != IN_HEAP);
            _heap.push(std::make_pair(k, entry_t{Traits::semiring::zero, k}));
            _vertex_status_map[k] = IN_HEAP;
        }
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_heap.empty())) {
        return _heap.empty();
    }

    // See competing_dijkstras::current(): the noexcept measures the copy the
    // return performs, not just the top() call.
    [[nodiscard]] constexpr auto current() const
        noexcept(noexcept(typename heap::value_type(_heap.top()))) {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() {
        assert(!finished());
        const auto [t, st_dist] = _heap.top();
        _vertex_status_map[t] = POST_HEAP;
        auto && out_arcs_range = melon::out_arcs(_graph, t);
        prefetch_keys_and_values(out_arcs_range, arc_targets_map(_graph),
                                 _length_map);
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

    [[nodiscard]] constexpr bool reached(const vertex & u) const
        noexcept(noexcept(_vertex_status_map[u] != PRE_HEAP)) {
        return _vertex_status_map[u] != PRE_HEAP;
    }
    // A view of the reached state, like breadth_first_search's and
    // depth_first_search's. It refers into the algorithm, as every melon map
    // view refers into what it names: it is valid while this object lives and
    // stays put, exactly the contract mapping_ref_view carries.
    // See dijkstra::reached_map: computed, not stored.
    [[nodiscard]] constexpr auto reached_map() const {
        return maps::map([this](const vertex & u) { return reached(u); });
    }
    [[nodiscard]] constexpr bool visited(const vertex & u) const
        noexcept(noexcept(_vertex_status_map[u] == POST_HEAP)) {
        return _vertex_status_map[u] == POST_HEAP;
    }
};

// No Traits parameter: the class template's own default computes it, so the
// deduced type and the explicitly written `network_voronoi<G, LM>` agree.
template <typename Graph, typename LengthMap>
network_voronoi(Graph &&, LengthMap &&)
    -> network_voronoi<views::graph_all_t<Graph>,
                       maps::mapping_all_t<LengthMap>>;

template <typename Graph, typename LengthMap, typename Traits>
network_voronoi(Traits, Graph &&, LengthMap &&)
    -> network_voronoi<views::graph_all_t<Graph>,
                       maps::mapping_all_t<LengthMap>, Traits>;

template <typename Graph, typename LengthMap, typename Kernels>
network_voronoi(Graph &&, LengthMap &&,
                Kernels &&) -> network_voronoi<views::graph_all_t<Graph>,
                                               maps::mapping_all_t<LengthMap>>;

template <typename Graph, typename LengthMap, typename Kernels, typename Traits>
network_voronoi(Traits, Graph &&, LengthMap &&, Kernels &&)
    -> network_voronoi<views::graph_all_t<Graph>,
                       maps::mapping_all_t<LengthMap>, Traits>;

}  // namespace melon
