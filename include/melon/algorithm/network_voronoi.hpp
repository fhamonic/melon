#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/fill.hpp"
#include "melon/detail/intrusive_iterator_base.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/maps/element.hpp"
#include "melon/numeric/semiring.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

template <typename Traits>
concept network_voronoi_traits =
    semiring<typename Traits::semiring> &&
    updatable_priority_queue<typename Traits::heap> && requires {
        { Traits::store_cluster_adjacency } -> std::convertible_to<bool>;
        { Traits::store_distances } -> std::convertible_to<bool>;
        { Traits::store_clusters } -> std::convertible_to<bool>;
    };

template <has_vertex_map Graph, typename ValueType>
struct network_voronoi_default_traits {
    using semiring = shortest_path_semiring<ValueType>;
    // The graph's own vertex type, not a hardcoded unsigned int: a cluster id
    // names the kernel vertex, so a narrower id truncates a wider vertex
    // handle silently -- std::pair's forwarding constructor bypasses the
    // braced-init narrowing check, so no warning fires.
    using cluster_id_t = vertex_t<Graph>;
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
                             vertex_map_t<Graph, std::size_t>, maps::element<1>,
                             maps::element<0>>;

    static constexpr bool store_cluster_adjacency = false;
    static constexpr bool store_distances = false;
    static constexpr bool store_clusters = false;
};

// Multi-source Dijkstra partitioning the graph into Voronoi cells: every
// vertex reachable from the kernels given to set_kernels() is assigned to the
// nearest one. Iterating yields (vertex, (distance, kernel)) pairs in order of
// increasing distance, each vertex exactly once, where the second member
// identifies the cell the vertex fell into.
// Same precondition as melon::dijkstra, uncheckable by any concept: an arc
// length must never improve a distance when combined. Each vertex settles
// once, so a violation silently assigns it to the wrong cell.
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

    [[no_unique_address]]
    detail::vertex_map_if<Traits::store_distances, Graph, length_type>
        _distances_map;
    [[no_unique_address]]
    detail::vertex_map_if<Traits::store_clusters, Graph, cluster_id_t>
        _clusters_map;

public:
    // ---- Construction -------------------------------------------------------

    template <graph_for<Graph> G, mapping_for<LengthMap> LM>
    constexpr network_voronoi(G && g, LM && lm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _length_map(maps::mapping_all(std::forward<LM>(lm)))
        , _heap(_entry_cmp, create_vertex_map<std::size_t>(_graph))
        , _vertex_status_map(create_vertex_map<vertex_status>(_graph, PRE_HEAP))
        , _distances_map(_graph)
        , _clusters_map(_graph) {}

    template <graph_for<Graph> G, mapping_for<LengthMap> LM,
              std::ranges::range KR>
    constexpr network_voronoi(G && g, LM && lm, KR && kr)
        : network_voronoi(std::forward<G>(g), std::forward<LM>(lm)) {
        set_kernels(std::forward<KR>(kr));
    }

    template <typename... Args>
        requires std::constructible_from<network_voronoi, Args...>
    constexpr network_voronoi(Traits, Args &&... args)
        : network_voronoi(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    constexpr network_voronoi(const network_voronoi &) = delete;
    constexpr network_voronoi(network_voronoi &&) = default;

    constexpr network_voronoi & operator=(const network_voronoi &) = delete;
    constexpr network_voronoi & operator=(network_voronoi &&) = default;

    // ---- Base access --------------------------------------------------------

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

    // ---- Setup --------------------------------------------------------------

    constexpr network_voronoi & reset() {
        _heap.clear();
        detail::fill(_vertex_status_map, vertices(_graph), PRE_HEAP);
        return *this;
    }
    // Strict precondition: the kernels must be untouched, so seed before
    // iterating and reset() in between. Re-seeding a settled vertex silently
    // corrupts the cell assignment, and neither weaker check catches it: a
    // completed sweep leaves the heap empty and every reached vertex
    // POST_HEAP, so `!= IN_HEAP` and the emptiness assert both pass.
    template <std::ranges::input_range KR>
        requires std::convertible_to<std::ranges::range_value_t<KR>,
                                     vertex_t<Graph>>
    constexpr network_voronoi & set_kernels(KR && kernels) {
        assert(_heap.empty());
        for(auto && k : kernels) {
            // One conversion to vertex up front: the constraint only promises
            // convertibility, so pushing `k` itself would let a wider id type
            // flow unconverted into the heap entry.
            const vertex v = static_cast<vertex>(k);
            assert(_vertex_status_map[v] == PRE_HEAP);
            _heap.push(std::make_pair(v, entry_t{Traits::semiring::zero, v}));
            _vertex_status_map[v] = IN_HEAP;
        }
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_heap.empty())) {
        return _heap.empty();
    }

    // The noexcept measures the copy the by-value return performs, not just the
    // top() call.
    [[nodiscard]] constexpr auto current() const
        noexcept(noexcept(typename heap::value_type(_heap.top()))) {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() {
        assert(!finished());
        const auto [t, st_dist] = _heap.top();
        _vertex_status_map[t] = POST_HEAP;
        if constexpr(Traits::store_distances) _distances_map[t] = st_dist.first;
        if constexpr(Traits::store_clusters) _clusters_map[t] = st_dist.second;
        auto && out_arcs_range = melon::out_arcs(_graph, t);
        detail::prefetch_keys_and_values(out_arcs_range,
                                         arc_targets_map(_graph), _length_map);
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

    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] constexpr bool reached(const vertex & u) const
        noexcept(noexcept(_vertex_status_map[u] != PRE_HEAP)) {
        return _vertex_status_map[u] != PRE_HEAP;
    }
    // Refers into the algorithm, like every melon map view: valid while this
    // object lives and stays put, mapping_ref_view's contract.
    [[nodiscard]] constexpr auto reached_map() const & {
        return maps::function([this](const vertex & u) { return reached(u); });
    }
    // Terminal, like std::move(alg).base() -- the member left behind is valid
    // but empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto reached_map() && {
        return maps::function(
            [status_map = std::move(_vertex_status_map)](const vertex & u) {
                return status_map[u] != PRE_HEAP;
            });
    }
    [[nodiscard]] constexpr bool visited(const vertex & u) const
        noexcept(noexcept(_vertex_status_map[u] == POST_HEAP)) {
        return _vertex_status_map[u] == POST_HEAP;
    }
    // Iteration yields each (vertex, (distance, kernel)) once and then forgets
    // it, so per-vertex lookups need the maps these traits enable.
    [[nodiscard]] constexpr length_type dist(const vertex & u) const
        noexcept(noexcept(_distances_map[u]))
        requires(Traits::store_distances)
    {
        assert(visited(u));
        return _distances_map[u];
    }
    [[nodiscard]] constexpr cluster_id_t cluster(const vertex & u) const
        noexcept(noexcept(_clusters_map[u]))
        requires(Traits::store_clusters)
    {
        assert(visited(u));
        return _clusters_map[u];
    }
    // reached_map()'s contract: valid while this object lives and stays put.
    // Unlike dist() and cluster() they cannot assert per read, so vertices not
    // yet visited still hold indeterminate values -- read them once the
    // vertices of interest are out.
    [[nodiscard]] constexpr auto dists_map() const & noexcept(
        noexcept(maps::mapping_all(_distances_map._map)))
        requires(Traits::store_distances)
    {
        return maps::mapping_all(_distances_map._map);
    }
    [[nodiscard]] constexpr auto clusters_map() const & noexcept(
        noexcept(maps::mapping_all(_clusters_map._map)))
        requires(Traits::store_clusters)
    {
        return maps::mapping_all(_clusters_map._map);
    }
    // Terminal, like std::move(alg).base(): the member left behind is valid but
    // empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto dists_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_distances_map._map))))
        requires(Traits::store_distances)
    {
        return maps::mapping_all(std::move(_distances_map._map));
    }
    [[nodiscard]] constexpr auto clusters_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_clusters_map._map))))
        requires(Traits::store_clusters)
    {
        return maps::mapping_all(std::move(_clusters_map._map));
    }
};

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
