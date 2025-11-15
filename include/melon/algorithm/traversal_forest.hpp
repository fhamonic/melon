#ifndef MELON_ALGORITHM_TRAVERSAL_FOREST_HPP
#define MELON_ALGORITHM_TRAVERSAL_FOREST_HPP

#include <cassert>
#include <ranges>

#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/detail/consumable_view.hpp"
#include "melon/graph.hpp"

namespace fhamonic {
namespace melon {

template <graph _Graph, std::ranges::range _Sources>
    requires has_vertex_map<_Graph> &&
             std::same_as<std::ranges::range_value_t<_Sources>,
                          vertex_t<_Graph>>
class traversal_forest
    : public algorithm_view_interface<traversal_forest<_Graph, _Sources>> {
private:
    using vertex = vertex_t<_Graph>;
    struct bfs_traits {
        static constexpr bool store_pred_vertices = false;
        static constexpr bool store_pred_arcs = false;
        static constexpr bool store_distances = false;
        static constexpr bool store_traversal_range = true;
    };

private:
    _Graph _graph;
    consumable_view<_Sources> _remaining_sources;
    breadth_first_search<views::graph_all_t<_Graph>, bfs_traits> _bfs;

public:
    template <typename _G>
    [[nodiscard]] constexpr explicit traversal_forest(_G && g) noexcept
        : _graph(views::graph_all(std::forward<_G>(g)))
        , _remaining_sources(vertices(_graph))
        , _bfs(_graph) {
        advance();
    }

    template <typename _G, typename _S>
    [[nodiscard]] constexpr explicit traversal_forest(_G && g,
                                                      _S && sources) noexcept
        : _graph(views::graph_all(std::forward<_G>(g)))
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

template <typename _Graph>
traversal_forest(_Graph &&)
    -> traversal_forest<views::graph_all_t<_Graph>, vertices_range_t<_Graph>>;

template <typename _Graph, typename _Sources>
traversal_forest(_Graph &&, _Sources &&)
    -> traversal_forest<views::graph_all_t<_Graph>,
                        std::views::all_t<_Sources>>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_TRAVERSAL_FOREST_HPP