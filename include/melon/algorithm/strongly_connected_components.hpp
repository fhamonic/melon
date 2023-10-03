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

    std::reference_wrapper<const G> _graph;
    std::vector<vertex> _stack;

    vertex_map_t<G, bool> _reached_map;
    vertex_map_t<G, component_num> _index_map;
    vertex_map_t<G, component_num> _lowlink_map;

public:
    [[nodiscard]] constexpr explicit strongly_connected_components(
        const G & g) noexcept
        : _graph(g)
        , _stack()
        , _reached_map(create_vertex_map<bool>(g, false))
        , _index_map(create_vertex_map<component_num>(g), 0)
        , _lowlink_map(create_vertex_map<component_num>(g), 0) {
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
        _stack.resize(0);
        _reached_map.fill(false);
        return *this;
    }

    constexpr void run() noexcept {
        component_num index = 0;
        for(auto && s : melon::vertices(_graph.get())) {
            if(reached[s]) continue;
            _stack.push_back(s);
            _reached_map[s] = true;
            _component_num_map[s] = index;
            ++index;

            component_num start_index = index;
            auto u = s;
            while(_lowlink[u] != _index[u]) {
                for(auto && w : out_neighbors(_graph.get(), u)) {
                    if(_reached_map[w]) continue;
                    _stack.push_back(w);
                    _reached_map[w] = true;
                }
                u = _stack.back();
            }

            while(!_stack.empty()) {
            }
        }
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_STRONGLY_CONNECTED_COMPONENTS_HPP