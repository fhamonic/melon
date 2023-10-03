#ifndef MELON_ALGORITHM_STRONGLY_CONNECTED_COMPONENTS_HPP
#define MELON_ALGORITHM_STRONGLY_CONNECTED_COMPONENTS_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <stack>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/detail/constexpr_ternary.hpp"
#include "melon/graph.hpp"
#include "melon/utility/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

template <outward_adjacency_graph G>
    requires has_vertex_map<G>
class strongly_connected_components {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;

    using component_num = unsigned int;

    static constexpr component_num INVALID_COMPONENT =
        std::numeric_limits<component_num>::max();

    template <typename R>
    struct co_range {
        std::ranges::iterator_t<R> it;
        R range;

        co_range(R && r) : it(r.begin()), range(r) {}

        bool empty() const { return it == range.end(); }
        void advance() { ++it; }
        auto current() { return *it; }
    };

    std::reference_wrapper<const G> _graph;

    // co_range<vertices_range_t<G>>
    //     _remaining_vertices;
    std::vector<std::pair<vertex, co_range<out_neighbors_range_t<G>>>> _stack;

    // std::vector<vertex> _tarjan_stack;
    // component_num start_index;
    // component_num index;

    vertex_map_t<G, bool> _reached_map;
    // vertex_map_t<G, component_num> _index_map;
    // vertex_map_t<G, component_num> _lowlink_map;

public:
    [[nodiscard]] constexpr explicit strongly_connected_components(
        const G & g) noexcept
        : _graph(g), _stack(), _reached_map(create_vertex_map<bool>(g, false)) {
        _stack.reserve(g.nb_vertices());
    }

    [[nodiscard]] constexpr strongly_connected_components(
        const G & g, const vertex & s) noexcept
        : strongly_connected_components(g) {
        add_source(s);
    }

    [[nodiscard]] constexpr strongly_connected_components(
        const strongly_connected_components &) = default;
    [[nodiscard]] constexpr strongly_connected_components(
        strongly_connected_components &&) = default;

    constexpr strongly_connected_components & operator=(
        const strongly_connected_components &) = default;
    constexpr strongly_connected_components & operator=(
        strongly_connected_components &&) = default;

    constexpr strongly_connected_components & reset() noexcept {
        _stack.resize(0);
        _reached_map.fill(false);
        return *this;
    }
    constexpr strongly_connected_components & add_source(
        const vertex & s) noexcept {
        assert(!_reached_map[s]);
        _stack.emplace_back(s, out_neighbors(_graph.get(), s));
        _reached_map[s] = true;
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _stack.empty();
    }

    [[nodiscard]] constexpr vertex current() const noexcept {
        assert(!finished());
        return _stack.back().first;
    }

    constexpr void advance() noexcept {
        assert(!finished());
        do {
            auto & remaining_neighbors = _stack.back().second;
            for(; !remaining_neighbors.empty(); remaining_neighbors.advance()) {
                auto w = remaining_neighbors.current();
                if(_reached_map[w]) continue;
                _stack.emplace_back(w, out_neighbors(_graph.get(), w));
                _reached_map[w] = true;
                return;
            };
            _stack.pop_back();
        } while(!_stack.empty());
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }
    [[nodiscard]] constexpr auto begin() noexcept {
        return traversal_iterator(*this);
    }
    [[nodiscard]] constexpr auto end() const noexcept {
        return traversal_end_sentinel();
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const noexcept {
        return _reached_map[u];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_STRONGLY_CONNECTED_COMPONENTS_HPP