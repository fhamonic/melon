#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/detail/intrusive_iterator_base.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/no_unique_address.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace melon {

template <typename Traits>
concept topological_sort_traits = requires {
    { Traits::store_ranks } -> std::convertible_to<bool>;
    { Traits::store_critical_paths } -> std::convertible_to<bool>;
};

struct topological_sort_default_traits {
    static constexpr bool store_ranks = false;
    static constexpr bool store_critical_paths = false;
};

// has_num_vertices is a real requirement, not an accident: the constructor
// reserves num_vertices(_graph) and _queue_current is an iterator into _queue
// whose stability depends on that reserve. Spelled out so a graph without it
// fails the constraint instead of hard-erroring inside the constructor.
template <graph_view Graph,
          topological_sort_traits Traits = topological_sort_default_traits>
    requires outward_incidence_graph<Graph> && has_num_vertices<Graph>
class topological_sort
    : public algorithm_view_interface<topological_sort<Graph, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;
    using reached_map_t = vertex_map_t<Graph, bool>;
    using remaining_in_degree_map_t = vertex_map_t<Graph, std::size_t>;

private:
    Graph _graph;
    std::vector<vertex> _queue;
    std::vector<vertex>::iterator _queue_current;
    reached_map_t _reached_map;
    remaining_in_degree_map_t _remaining_in_degree_map;

    MELON_NO_UNIQUE_ADDRESS detail::vertex_map_if<
        Traits::store_critical_paths && !has_arc_source<Graph>, Graph, vertex>
        _pred_vertices_map;
    MELON_NO_UNIQUE_ADDRESS detail::vertex_map_if<Traits::store_critical_paths,
                                                  Graph, std::optional<arc>>
        _pred_arcs_map;
    MELON_NO_UNIQUE_ADDRESS
    detail::vertex_map_if<Traits::store_ranks, Graph, int>
        _rank_map;

    constexpr void push_start_vertices() {
        _queue.resize(0);
        _reached_map.fill(false);
        if constexpr(has_in_degree<Graph>) {
            for(auto && u : vertices(_graph)) {
                _remaining_in_degree_map[u] = in_degree(_graph, u);
                if(_remaining_in_degree_map[u] == 0) {
                    _queue.push_back(u);
                    _reached_map[u] = true;
                    if constexpr(Traits::store_critical_paths) {
                        _pred_arcs_map[u].reset();
                        if constexpr(!has_arc_source<Graph>)
                            _pred_vertices_map[u] = u;
                    }
                }
            }
        } else {
            _remaining_in_degree_map.fill(0);
            for(auto && u : vertices(_graph)) {
                for(auto && a : out_arcs(_graph, u)) {
                    const vertex & w = arc_target(_graph, a);
                    ++_remaining_in_degree_map[w];
                }
            }
            for(auto && u : vertices(_graph)) {
                if(_remaining_in_degree_map[u] == 0) {
                    _queue.push_back(u);
                    _reached_map[u] = true;
                    if constexpr(Traits::store_critical_paths) {
                        _pred_arcs_map[u].reset();
                        if constexpr(!has_arc_source<Graph>)
                            _pred_vertices_map[u] = u;
                    }
                }
            }
        }
        if constexpr(Traits::store_ranks) _rank_map.fill(0);
        // Taken after the push_backs: an iterator grabbed while the vector is
        // empty sits at the insertion point, which push_back formally
        // invalidates -- checked-iterator builds flag the traversal.
        _queue_current = _queue.begin();
    }

public:
    // ---- Construction -------------------------------------------------------

    template <typename G>
        requires detail::not_self<G, topological_sort> && graph_for<G, Graph> &&
                     has_vertex_map<Graph>
    constexpr explicit topological_sort(G && g)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _queue()
        , _reached_map(create_vertex_map<bool>(_graph, false))
        , _remaining_in_degree_map(
              create_vertex_map<std::size_t>(_graph, std::size_t{0}))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _rank_map(_graph) {
        _queue.reserve(num_vertices(_graph));
        push_start_vertices();
    }

    template <typename... Args>
        requires std::constructible_from<topological_sort, Args...>
    constexpr topological_sort(Traits, Args &&... args)
        : topological_sort(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    // Moves stay defaulted: _queue_current is an iterator into _queue, whose
    // buffer transfers with the move, and the constructor's reserve() keeps it
    // stable across the push_backs. A copy cannot be defaulted -- it would hand
    // the new object an iterator into the source's buffer, which the copy then
    // walks off the end of.
    constexpr topological_sort(const topological_sort &) = delete;
    constexpr topological_sort(topological_sort &&) = default;

    constexpr topological_sort & operator=(const topological_sort &) = delete;
    constexpr topological_sort & operator=(topological_sort &&) = default;

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

public:
    // ---- Setup --------------------------------------------------------------

    constexpr topological_sort & reset() {
        push_start_vertices();
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_queue_current == _queue.end())) {
        return _queue_current == _queue.end();
    }

    // The noexcept measures the copy the by-value return performs, not just the
    // dereference.
    [[nodiscard]] constexpr vertex current() const
        noexcept(noexcept(vertex(*_queue_current))) {
        assert(!finished());
        return *_queue_current;
    }

    constexpr void advance() {
        assert(!finished());
        const vertex & u = *_queue_current;
        ++_queue_current;
        for(auto && a : out_arcs(_graph, u)) {
            const vertex & w = arc_target(_graph, a);
            if(--_remaining_in_degree_map[w] > 0) continue;
            _queue.push_back(w);
            _reached_map[w] = true;
            if constexpr(Traits::store_ranks) _rank_map[w] = _rank_map[u] + 1;
            if constexpr(Traits::store_critical_paths) {
                _pred_arcs_map[w].emplace(a);
                if constexpr(!has_arc_source<Graph>) _pred_vertices_map[w] = u;
            }
        }
    }

    // ---- Queries ------------------------------------------------------------

    // Whether the graph carried no directed cycle. Precondition: finished() --
    // a vertex on a cycle, or behind one, keeps a positive remaining in-degree
    // forever, so the count only settles once nothing more can be ordered.
    [[nodiscard]] constexpr bool is_acyclic() const {
        assert(finished());
        return _queue.size() == num_vertices(_graph);
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const
        noexcept(noexcept(_reached_map[u])) {
        return _reached_map[u];
    }
    // Refers into the algorithm, like every melon map view: valid while this
    // object lives and stays put, mapping_ref_view's contract.
    [[nodiscard]] constexpr auto reached_map() const & noexcept(
        noexcept(maps::mapping_all(_reached_map))) {
        return maps::mapping_all(_reached_map);
    }
    // Terminal, like std::move(alg).base() -- the member left behind is valid
    // but empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto reached_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_reached_map)))) {
        return maps::mapping_all(std::move(_reached_map));
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const
        requires(Traits::store_critical_paths)
    {
        // Not .value(): asking a start vertex for a predecessor arc is a
        // precondition violation, not an exceptional condition.
        assert(reached(u) && _pred_arcs_map[u].has_value());
        return *_pred_arcs_map[u];
    }
    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const
        requires(Traits::store_critical_paths)
    {
        assert(reached(u) && _pred_arcs_map[u].has_value());
        if constexpr(has_arc_source<Graph>)
            return melon::arc_source(_graph, pred_arc(u));
        else
            return _pred_vertices_map[u];
    }
    [[nodiscard]] constexpr int rank(const vertex & u) const
        noexcept(noexcept(_rank_map[u]))
        requires(Traits::store_ranks)
    {
        assert(reached(u));
        return _rank_map[u];
    }

private:
    class path_iterator
        : public detail::intrusive_iterator_base<topological_sort, vertex> {
        // MSVC (through at least VS 18.6) denies hidden friends of a nested
        // class the enclosing class's private access: a sentinel friend
        // reading _pred_arcs_map directly fails to compile there.
        [[nodiscard]] constexpr bool _at_path_end() const {
            return !this->_structure->_pred_arcs_map[this->_cursor].has_value();
        }

    public:
        using value_type = arc;
        using reference = arc;
        using detail::intrusive_iterator_base<topological_sort,
                                              vertex>::intrusive_iterator_base;

        // A plain prvalue, not a `const` one: a const prvalue inhibits moves
        // and makes std::iterator_traits disagree with the `reference` typedef
        // above -- a std::indirectly_readable hazard.
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
            return it._at_path_end();
        }
        [[nodiscard]] constexpr friend bool operator==(
            const path_iterator & it1,
            const path_iterator & it2) noexcept(noexcept(it1._cursor ==
                                                         it2._cursor)) {
            assert(it1._structure == it2._structure);
            return it1._cursor == it2._cursor;
        }
    };

public:
    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] constexpr auto critical_path_to(const vertex & t) const
        noexcept(noexcept(std::ranges::subrange(path_iterator(this, t),
                                                std::default_sentinel)))
        requires(Traits::store_critical_paths)
    {
        assert(reached(t));
        return std::ranges::subrange(path_iterator(this, t),
                                     std::default_sentinel);
    }
};

template <typename Graph, typename Traits = topological_sort_default_traits>
topological_sort(Graph &&)
    -> topological_sort<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits>
topological_sort(Traits, Graph &&)
    -> topological_sort<views::graph_all_t<Graph>, Traits>;

}  // namespace melon
