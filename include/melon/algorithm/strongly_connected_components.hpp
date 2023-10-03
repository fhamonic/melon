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
#include "melon/detail/intrusive_view.hpp"
#include "melon/graph.hpp"
#include "melon/utility/traversal_iterator.hpp"

namespace fhamonic {
namespace melon {

template <outward_incidence_graph G>
    requires has_vertex_map<G>
class strongly_connected_components {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;
    using traits = T;

    using component_num = unsigned int;

    static constexpr component_num INVALID_COMPONENT = std::numeric_limits<component_num>::max();

    std::reference_wrapper<const G> _graph;

    template <typename R>
    struct co_range {
        std::range::iterator_t<R> it;
        R range;

        co_range(R && r) : it(r.begin()), range(r) {}

        bool empty() const { return it == range.end(); }
    }

    co_range<vertices_range_t<G>>
        _remaining_vertices;
    std::vector<std::pair<vertex, co_range<out_neighbors_range_t<G>>>> _stack;

    component_num start_index;
    component_num index;

    // vertex_map_t<G, bool> _reached_map;
    vertex_map_t<G, component_num> _index_map;
    vertex_map_t<G, component_num> _lowlink_map;

public:
    [[nodiscard]] constexpr explicit strongly_connected_components(
        const G & g) noexcept
        : _graph(g)
        , _remaining_vertices(vertices(g))
        , _stack()
        , start_index(0)
        , index(0)
        // , _reached_map(create_vertex_map<bool>(g, false))
        , _index_map(create_vertex_map<component_num>(g, INVALID_COMPONENT))
        , _lowlink_map(create_vertex_map<component_num>(g, INVALID_COMPONENT)) {
        _stack.reserve(g.nb_vertices());
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
        _remaining_vertices = co_range<vertices_range_t<G>>(vertices(g));
        _stack.resize(0);
        start_index = index = 0;
        // _reached_map.fill(false);
        return *this;
    }

    constexpr bool finished() const { return _remaining_vertices.empty(); }

    constexpr void strongconnect(vertex u) {
        _index_map[s] = _lowlink_map[s] = index;
        ++index;

    }

    constexpr void run() noexcept {
        for(auto && s : melon::vertices(_graph.get())) {
            if(reached[s]) continue;
            component_num start_index = index;
            _stack.emplace_back(s, out_neighbors(_graph.get(), s));
            start_index = _index_map[s] = _lowlink_map[s] = index;
            ++index;

            for(;;) {
                auto [u, neighbors] = _stack.back();
                if(neighbors.empty()) break;
                auto w = *neighbors.it;

                _stack.emplace_back(w, out_neighbors(_graph.get(), w));
            }

            if(auto u = _stack.back().first; _lowlink[u] == _index[u]) {
                do {
                    _stack.pop();
                } while()
            }

            while(!_stack.empty()) {
            }
        }
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_STRONGLY_CONNECTED_COMPONENTS_HPP