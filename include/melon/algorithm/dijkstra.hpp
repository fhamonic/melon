#pragma once

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
concept dijkstra_traits = semiring<typename Traits::semiring> &&
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
                             maps::element_map<1>, maps::element_map<0>>;

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
// graph_view / mapping_view on the stored members, like the view adaptors
// (transform_view<V> requires view<V>): the constructors always route through
// graph_all / mapping_all, so a non-view member type was a legal spelling
// whose constructor could never run -- and a raw container member silently
// deep-copied. Value ownership is spelled graph_owning_view /
// mapping_owning_view.
template <graph_view Graph, mapping_view<arc_t<Graph>> LengthMap,
          dijkstra_traits Traits = dijkstra_default_traits<
              Graph, mapped_value_t<LengthMap, arc_t<Graph>>>>
    requires outward_incidence_graph<Graph> && has_vertex_map<Graph>
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
    // Constrained on the storability of each argument into its member, so that
    // std::is_constructible answers what construction actually does: the old
    // unconstrained template said yes to a raw length map aimed at a view-typed
    // member and then hard-errored in the mem-initializer, outside the
    // immediate context. The store_* helpers also admit raw storage into
    // explicitly spelled member types, which the unconditional mapping_all
    // wrap used to reject.
    template <graph_for<Graph> G, mapping_for<LengthMap> LM>
    constexpr dijkstra(G && g, LM && lm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _length_map(maps::mapping_all(std::forward<LM>(lm)))
        , _heap(typename Traits::semiring::less_t(),
                create_vertex_map<std::size_t>(_graph))
        , _vertex_status_map(create_vertex_map<vertex_status>(_graph, PRE_HEAP))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _distances_map(_graph) {}

    template <graph_for<Graph> G, mapping_for<LengthMap> LM>
    constexpr dijkstra(G && g, LM && lm, const vertex & s)
        : dijkstra(std::forward<G>(g), std::forward<LM>(lm)) {
        add_source(s);
    }

    // Constrained on the delegate it forwards to, so the tag overload is
    // exactly as constructible as the constructor it names.
    template <typename... Args>
        requires std::constructible_from<dijkstra, Args...>
    constexpr dijkstra(Traits, Args &&... args)
        : dijkstra(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    constexpr dijkstra(const dijkstra &) = delete;
    constexpr dijkstra(dijkstra &&) = default;

    constexpr dijkstra & operator=(const dijkstra &) = delete;
    constexpr dijkstra & operator=(dijkstra &&) = default;

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

    // None of the four below are noexcept: they push into the heap (which
    // allocates) and run the user's length map, semiring and comparator.
    constexpr dijkstra & reset() {
        _heap.clear();
        _vertex_status_map.fill(PRE_HEAP);
        return *this;
    }
    // Strict precondition, like every add_source in the family: the vertex
    // must be untouched. `!= IN_HEAP` used to admit a *settled* vertex, and
    // re-processing one silently corrupts stored paths and distances.
    constexpr dijkstra & add_source(
        const vertex & s, const length_type & dist = Traits::semiring::zero) {
        assert(_vertex_status_map[s] == PRE_HEAP);
        _heap.push(std::make_pair(s, dist));
        _vertex_status_map[s] = IN_HEAP;
        if constexpr(Traits::store_paths) {
            _pred_arcs_map[s].reset();
            if constexpr(!has_arc_source<Graph>) _pred_vertices_map[s] = s;
        }
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_heap.empty())) {
        return _heap.empty();
    }

    // See competing_dijkstras::current(): the noexcept measures the copy the
    // by-value return performs, not just the top() call. This was the one
    // current() in the Dijkstra family carrying no specification at all.
    [[nodiscard]] constexpr traversal_entry current() const
        noexcept(noexcept(traversal_entry(_heap.top()))) {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() {
        assert(!finished());
        const auto [t, st_dist] = _heap.top();
        if constexpr(Traits::store_distances) _distances_map[t] = st_dist;
        _vertex_status_map[t] = POST_HEAP;
        auto && out_arcs_range = melon::out_arcs(_graph, t);
        prefetch_keys_and_values(out_arcs_range, arc_targets_map(_graph),
                                 _length_map);
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

    [[nodiscard]] constexpr bool reached(const vertex & u) const
        noexcept(noexcept(_vertex_status_map[u] != PRE_HEAP)) {
        return _vertex_status_map[u] != PRE_HEAP;
    }
    // A view of the reached state, like breadth_first_search's and
    // depth_first_search's. It refers into the algorithm, as every melon map
    // view refers into what it names: it is valid while this object lives and
    // stays put, exactly the contract mapping_ref_view carries.
    // Derived rather than stored: reachedness here is a status enum, so this
    // hands back a computed map instead of a reference to one.
    [[nodiscard]] constexpr auto reached_map() const & {
        return maps::map([this](const vertex & u) { return reached(u); });
    }
    // The expiring overload has no stored bool map to hand to
    // mapping_owning_view, so it moves the status map into the lambda
    // instead: self-contained, no `this`, it outlives the algorithm like
    // every other extraction. Terminal, like std::move(alg).base(): the
    // member left behind is valid but empty, so no other member may be
    // called afterwards.
    [[nodiscard]] constexpr auto reached_map() && {
        return maps::map(
            [status_map = std::move(_vertex_status_map)](const vertex & u) {
                return status_map[u] != PRE_HEAP;
            });
    }
    [[nodiscard]] constexpr bool visited(const vertex & u) const
        noexcept(noexcept(_vertex_status_map[u] == POST_HEAP)) {
        return _vertex_status_map[u] == POST_HEAP;
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const
        requires(Traits::store_paths)
    {
        assert(reached(u) && _pred_arcs_map[u].has_value());
        return *_pred_arcs_map[u];
    }
    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const
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
    [[nodiscard]] constexpr length_type dist(const vertex & u) const
        noexcept(noexcept(_distances_map[u]))
        requires(Traits::store_distances)
    {
        assert(visited(u));
        return _distances_map[u];
    }
    // A view of the stored distances, reached_map()'s contract: valid while
    // this object lives and stays put. Unlike dist() it cannot assert per
    // read, so vertices not yet visited still hold indeterminate values --
    // read it once the vertices of interest are out.
    [[nodiscard]] constexpr auto dists_map() const & noexcept(
        noexcept(maps::mapping_all(_distances_map._map)))
        requires(Traits::store_distances)
    {
        return maps::mapping_all(_distances_map._map);
    }
    // The expiring overload moves the stored map into a mapping_owning_view,
    // std::views::all's ref-or-owning split. Extraction is terminal, like
    // std::move(alg).base(): the member left behind is valid but empty, so
    // no other member may be called afterwards.
    [[nodiscard]] constexpr auto dists_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_distances_map._map))))
        requires(Traits::store_distances)
    {
        return maps::mapping_all(std::move(_distances_map._map));
    }

private:
    class path_iterator : public intrusive_iterator_base<dijkstra, vertex> {
    public:
        using value_type = arc;
        using reference = arc;
        using intrusive_iterator_base<dijkstra,
                                      vertex>::intrusive_iterator_base;

        // Returns a plain prvalue, not a `const` one: a const prvalue
        // inhibits moves and makes std::iterator_traits disagree with the
        // `reference` typedef right above, which is a
        // std::indirectly_readable hazard.
        constexpr reference operator*() const {
            return this->_structure->_pred_arcs_map[this->_cursor].value();
        }
        constexpr path_iterator & operator++() {
            this->_cursor = this->_structure->pred_vertex(this->_cursor);
            return *this;
        }
        constexpr path_iterator operator++(int) {
            path_iterator it(*this);
            operator++();
            return it;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const path_iterator & it, std::default_sentinel_t) {
            return !it._structure->_pred_arcs_map[it._cursor].has_value();
        }
    };

public:
    [[nodiscard]] constexpr auto path_to(const vertex & t) const
        noexcept(noexcept(std::ranges::subrange(path_iterator(this, t),
                                                std::default_sentinel)))
        requires(Traits::store_paths)
    {
        assert(reached(t));
        return std::ranges::subrange(path_iterator(this, t),
                                     std::default_sentinel);
    }
};

// No Traits parameter: the class template's own default computes it, so the
// deduced type and the explicitly written `dijkstra<G, LM>` agree.
template <typename Graph, typename LengthMap>
dijkstra(Graph &&, LengthMap &&)
    -> dijkstra<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>>;

template <typename Graph, typename LengthMap>
dijkstra(Graph &&, LengthMap &&, const vertex_t<Graph> &)
    -> dijkstra<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>>;

template <typename Graph, typename LengthMap, typename Traits>
dijkstra(Traits, Graph &&,
         LengthMap &&) -> dijkstra<views::graph_all_t<Graph>,
                                   maps::mapping_all_t<LengthMap>, Traits>;

template <typename Graph, typename LengthMap, typename Traits>
dijkstra(Traits, Graph &&, LengthMap &&, const vertex_t<Graph> &)
    -> dijkstra<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>,
                Traits>;

}  // namespace melon