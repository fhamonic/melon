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
concept dijkstra_trait = semiring<typename Traits::semiring> &&
    updatable_priority_queue<typename Traits::heap> && requires() {
    { Traits::store_distances } -> std::convertible_to<bool>;
    { Traits::store_paths } -> std::convertible_to<bool>;
};
// clang-format on

template <typename Graph, typename ValueType>
struct dijkstra_default_traits {
    using semiring = shortest_path_semiring<ValueType>;
    using heap =
        updatable_d_ary_heap<2, std::pair<vertex_t<Graph>, ValueType>,
                             typename semiring::less_t,
                             vertex_map_t<Graph, std::size_t>,
                             views::element_map<1>, views::element_map<0>>;

    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};

// Requires arc lengths that never improve a distance when combined:
// non-negative lengths under the default shortest_path_semiring, factors in
// [0, 1] under most_reliable_path_semiring. Like every Dijkstra this settles
// each vertex once and never revisits it, so a violating length silently
// yields a wrong distance instead of an error -- it is a property of the
// mapped values, which no concept can check.
// O((m + n) log n) with the default binary heap.
template <outward_incidence_graph Graph, input_mapping<arc_t<Graph>> LengthMap,
          dijkstra_trait Traits>
    requires has_vertex_map<Graph>
class dijkstra
    : public algorithm_view_interface<dijkstra<Graph, LengthMap, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    using length_type = mapped_value_t<LengthMap, arc_t<Graph>>;
    using traversal_entry = std::pair<vertex, length_type>;

    using heap = Traits::heap;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    static_assert(std::is_same_v<typename heap::value_type,
                                 std::pair<vertex, length_type>>,
                  "dijkstras requires heap entries type.");

private:
    Graph _graph;
    LengthMap _length_map;
    heap _heap;
    vertex_map_t<Graph, vertex_status> _vertex_status_map;

    [[no_unique_address]] vertex_map_if<Traits::store_paths &&
                                            !has_arc_source<Graph>,
                                        Graph, vertex> _pred_vertices_map;
    [[no_unique_address]] vertex_map_if<Traits::store_paths, Graph,
                                        std::optional<arc>> _pred_arcs_map;
    [[no_unique_address]] vertex_map_if<Traits::store_distances, Graph,
                                        length_type> _distances_map;

public:
    template <typename G, typename M>
    [[nodiscard]] constexpr dijkstra(G && g, M && l)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _length_map(views::mapping_all(std::forward<M>(l)))
        , _heap(typename Traits::semiring::less_t(),
                create_vertex_map<std::size_t>(_graph))
        , _vertex_status_map(create_vertex_map<vertex_status>(_graph, PRE_HEAP))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _distances_map(_graph) {}

    template <typename G, typename M>
    [[nodiscard]] constexpr dijkstra(G && g, M && l, const vertex & s)
        : dijkstra(std::forward<G>(g), std::forward<M>(l)) {
        add_source(s);
    }

    template <typename... Args>
    [[nodiscard]] constexpr dijkstra(Traits, Args &&... args)
        : dijkstra(std::forward<Args>(args)...) {}

    [[nodiscard]] constexpr dijkstra(const dijkstra &) = default;
    [[nodiscard]] constexpr dijkstra(dijkstra &&) = default;

    constexpr dijkstra & operator=(const dijkstra &) = default;
    constexpr dijkstra & operator=(dijkstra &&) = default;

    // None of the four below are noexcept: they push into the heap (which
    // allocates) and run the user's length map, semiring and comparator.
    constexpr dijkstra & reset() {
        _heap.clear();
        _vertex_status_map.fill(PRE_HEAP);
        return *this;
    }
    constexpr dijkstra & add_source(
        const vertex & s, const length_type & dist = Traits::semiring::zero) {
        assert(_vertex_status_map[s] != IN_HEAP);
        _heap.push(std::make_pair(s, dist));
        _vertex_status_map[s] = IN_HEAP;
        if constexpr(Traits::store_paths) {
            _pred_arcs_map[s].reset();
            if constexpr(!has_arc_source<Graph>) _pred_vertices_map[s] = s;
        }
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _heap.empty();
    }

    [[nodiscard]] constexpr traversal_entry current() const {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() {
        assert(!finished());
        const auto [t, st_dist] = _heap.top();
        if constexpr(Traits::store_distances) _distances_map[t] = st_dist;
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
                    Traits::semiring::plus(st_dist, _length_map[a]);
                if(Traits::semiring::less(new_dist, _heap.priority(w))) {
                    _heap.promote(w, new_dist);
                    if constexpr(Traits::store_paths) {
                        _pred_arcs_map[w].emplace(a);
                        if constexpr(!has_arc_source<Graph>)
                            _pred_vertices_map[w] = t;
                    }
                }
            } else if(w_status == PRE_HEAP) {
                _heap.push(std::make_pair(
                    w, Traits::semiring::plus(st_dist, _length_map[a])));
                _vertex_status_map[w] = IN_HEAP;
                if constexpr(Traits::store_paths) {
                    _pred_arcs_map[w].emplace(a);
                    if constexpr(!has_arc_source<Graph>)
                        _pred_vertices_map[w] = t;
                }
            }
        }
    }

    constexpr void run() {
        while(!finished()) advance();
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _vertex_status_map[u] != PRE_HEAP;
    }
    [[nodiscard]] constexpr bool visited(const vertex & u) const noexcept {
        return _vertex_status_map[u] == POST_HEAP;
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const noexcept
        requires(Traits::store_paths)
    {
        assert(reached(u));
        return _pred_arcs_map[u].value();
    }
    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const noexcept
        requires(Traits::store_paths)
    {
        assert(reached(u) && _pred_arcs_map[u].has_value());
        if constexpr(has_arc_source<Graph>)
            return melon::arc_source(_graph, pred_arc(u));
        else
            return _pred_vertices_map[u];
    }
    // Reads the heap, not _distances_map, so it does not need store_distances.
    [[nodiscard]] constexpr length_type current_dist(const vertex & u) const {
        assert(reached(u) && !visited(u));
        return _heap.priority(u);
    }
    [[nodiscard]] constexpr length_type dist(const vertex & u) const noexcept
        requires(Traits::store_distances)
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
        requires(Traits::store_paths)
    {
        assert(reached(t));
        return std::ranges::subrange(path_iterator(this, t),
                                     std::default_sentinel);
    }
};

template <
    typename Graph, typename LengthMap,
    typename Traits = dijkstra_default_traits<
        Graph, mapped_value_t<views::mapping_all_t<LengthMap>, arc_t<Graph>>>>
dijkstra(Graph &&,
         LengthMap &&) -> dijkstra<views::graph_all_t<Graph>,
                                   views::mapping_all_t<LengthMap>, Traits>;

template <
    typename Graph, typename LengthMap,
    typename Traits = dijkstra_default_traits<
        Graph, mapped_value_t<views::mapping_all_t<LengthMap>, arc_t<Graph>>>>
dijkstra(Graph &&, LengthMap &&, const vertex_t<Graph> &)
    -> dijkstra<views::graph_all_t<Graph>, views::mapping_all_t<LengthMap>,
                Traits>;

template <typename Graph, typename LengthMap, typename Traits>
dijkstra(Traits, Graph &&,
         LengthMap &&) -> dijkstra<views::graph_all_t<Graph>,
                                   views::mapping_all_t<LengthMap>, Traits>;

template <typename Graph, typename LengthMap, typename Traits>
dijkstra(Traits, Graph &&, LengthMap &&, const vertex_t<Graph> &)
    -> dijkstra<views::graph_all_t<Graph>, views::mapping_all_t<LengthMap>,
                Traits>;

}  // namespace melon