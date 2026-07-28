#pragma once

#include <algorithm>
#include <cassert>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/detail/intrusive_iterator_base.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace melon {

struct topological_sort_default_traits {
    static constexpr bool store_ranks = false;
    static constexpr bool store_critical_paths = false;
};

template <graph Graph, typename Traits = topological_sort_default_traits>
    requires outward_incidence_graph<Graph> && has_vertex_map<Graph>
class topological_sort
    : public algorithm_view_interface<topological_sort<Graph, Traits>> {
public:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    // No static_assert guarding store_pred_arcs: the class already requires
    // outward_incidence_graph, so the arcs are always available. The check
    // that used to sit here rejected every graph that *also* modelled
    // outward_adjacency_graph -- static_digraph among them -- which made
    // store_pred_arcs impossible to switch on.

    using reached_map = vertex_map_t<Graph, bool>;
    using remaining_in_degree_map = vertex_map_t<Graph, std::size_t>;

private:
    Graph _graph;
    std::vector<vertex> _queue;
    std::vector<vertex>::iterator _queue_current;
    reached_map _reached_map;
    remaining_in_degree_map _remaining_in_degree_map;

    [[no_unique_address]] vertex_map_if<Traits::store_critical_paths &&
                                            !has_arc_source<Graph>,
                                        Graph, vertex> _pred_vertices_map;
    [[no_unique_address]] vertex_map_if<Traits::store_critical_paths, Graph,
                                        std::optional<arc>> _pred_arcs_map;
    [[no_unique_address]] vertex_map_if<Traits::store_ranks, Graph, int>
        _rank_map;

    constexpr void push_start_vertices() noexcept {
        _queue.resize(0);
        _queue_current = _queue.begin();
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
    }

public:
    template <typename G>
    [[nodiscard]] constexpr explicit topological_sort(G && g)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _queue()
        , _reached_map(create_vertex_map<bool>(g, false))
        , _remaining_in_degree_map(create_vertex_map<long unsigned int>(
              g, std::numeric_limits<unsigned int>::max()))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _rank_map(_graph) {
        _queue.reserve(num_vertices(g));
        push_start_vertices();
    }

    template <typename... Args>
    [[nodiscard]] constexpr topological_sort(Traits, Args &&... args)
        : topological_sort(std::forward<Args>(args)...) {}

    [[nodiscard]] constexpr topological_sort(const topological_sort & bin) =
        default;
    [[nodiscard]] constexpr topological_sort(topological_sort && bin) = default;

    constexpr topological_sort & operator=(const topological_sort &) = default;
    constexpr topological_sort & operator=(topological_sort &&) = default;

public:
    constexpr topological_sort & reset() noexcept {
        _queue.resize(0);
        _queue_current = _queue.begin();
        _reached_map.fill(false);
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _queue_current == _queue.end();
    }

    [[nodiscard]] constexpr vertex current() const noexcept {
        assert(!finished());
        return *_queue_current;
    }

    constexpr void advance() noexcept {
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

    constexpr void run() noexcept {
        while(!finished()) advance();
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _reached_map[u];
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const noexcept
        requires(Traits::store_critical_paths)
    {
        assert(reached(u));
        return _pred_arcs_map[u].value();
    }
    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const noexcept
        requires(Traits::store_critical_paths)
    {
        assert(reached(u) && _pred_arcs_map[u].has_value());
        if constexpr(has_arc_source<Graph>)
            return melon::arc_source(_graph, pred_arc(u));
        else
            return _pred_vertices_map[u];
    }
    [[nodiscard]] constexpr int rank(const vertex & u) const noexcept
        requires(Traits::store_ranks)
    {
        assert(reached(u));
        return _rank_map[u];
    }

private:
    class path_iterator
        : public intrusive_iterator_base<topological_sort, vertex> {
    public:
        using value_type = arc;
        using reference = arc;
        using intrusive_iterator_base<topological_sort,
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
    [[nodiscard]] constexpr auto critical_path_to(
        const vertex & t) const noexcept
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
