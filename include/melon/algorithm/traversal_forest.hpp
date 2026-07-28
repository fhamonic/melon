#pragma once

#include <cassert>
#include <ranges>

#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/detail/consumable_view.hpp"
#include "melon/graph.hpp"

namespace melon {

template <graph Graph, std::ranges::range Sources>
    requires has_vertex_map<Graph> &&
             std::same_as<std::ranges::range_value_t<Sources>, vertex_t<Graph>>
class traversal_forest
    : public algorithm_view_interface<traversal_forest<Graph, Sources>> {
private:
    using vertex = vertex_t<Graph>;
    struct bfs_traits {
        static constexpr bool store_pred_vertices = false;
        static constexpr bool store_pred_arcs = false;
        static constexpr bool store_distances = false;
        static constexpr bool store_traversal_range = true;
    };

private:
    Graph _graph;
    consumable_view<Sources> _remaining_sources;
    breadth_first_search<views::graph_all_t<Graph>, bfs_traits> _bfs;

public:
    template <typename G>
    [[nodiscard]] constexpr explicit traversal_forest(G && g) noexcept
        : _graph(views::graph_all(std::forward<G>(g)))
        , _remaining_sources(vertices(_graph))
        , _bfs(_graph) {
        advance();
    }

    template <typename G, typename S>
    [[nodiscard]] constexpr explicit traversal_forest(G && g,
                                                      S && sources) noexcept
        : _graph(views::graph_all(std::forward<G>(g)))
        , _remaining_sources(std::views::all(sources))
        , _bfs(_graph) {
        advance();
    }

    [[nodiscard]] constexpr traversal_forest(const traversal_forest &) =
        default;
    [[nodiscard]] constexpr traversal_forest(traversal_forest &&) = default;

    constexpr traversal_forest & operator=(const traversal_forest &) = default;
    constexpr traversal_forest & operator=(traversal_forest &&) = default;

    constexpr traversal_forest & reset() noexcept {
        _remaining_sources = vertices(_graph);
        _bfs.reset();
        return *this;
    }

    [[nodiscard]] constexpr bool finished() noexcept {
        return _remaining_sources.empty();
    }

    [[nodiscard]] constexpr auto current() noexcept {
        assert(!finished());
        return _bfs.traversal();
    }

    constexpr void advance() noexcept {
        assert(!finished());
        while(_bfs.reached(_remaining_sources.current())) {
            _remaining_sources.advance();
            if(_remaining_sources.empty()) return;
        }
        _bfs.add_source(_remaining_sources.current()).run();
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _bfs.reached(u);
    }
};

template <typename Graph>
traversal_forest(Graph &&)
    -> traversal_forest<views::graph_all_t<Graph>, vertices_range_t<Graph>>;

template <typename Graph, typename Sources>
traversal_forest(Graph &&, Sources &&)
    -> traversal_forest<views::graph_all_t<Graph>, std::views::all_t<Sources>>;

}  // namespace melon
