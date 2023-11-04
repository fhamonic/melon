#ifndef MELON_DETAIL_MAP_IF_HPP
#define MELON_DETAIL_MAP_IF_HPP

#include <type_traits>

#include "melon/graph.hpp"

namespace fhamonic {
namespace melon {

template <bool _Cond, typename _Graph, typename _Type,
          typename _DiscriminatingT = int>
struct vertex_map_if {
    template <typename... _Args>
    [[nodiscard]] constexpr vertex_map_if(_Args &&...) {}
};

template <typename _Graph, typename _Type, typename _DiscriminatingT>
struct vertex_map_if<true, _Graph, _Type, _DiscriminatingT> {
    vertex_map_t<_Graph, _Type> _map;

    [[nodiscard]] constexpr vertex_map_if(_Graph & g)
        : _map(create_vertex_map<_Type>(g)) {}

    [[nodiscard]] constexpr vertex_map_if(_Graph & g, _Type && v)
        : _map(create_vertex_map<_Type>(g, v)) {}

    constexpr auto operator[](const vertex_t<_Graph> & v) const {
        return _map[v];
    }
    constexpr decltype(auto) operator[](const vertex_t<_Graph> & v) {
        return _map[v];
    }
};

template <bool _Cond, typename _Graph, typename _Type,
          typename _DiscriminatingT = int>
struct arc_map_if {
    template <typename... _Args>
    [[nodiscard]] constexpr arc_map_if(_Args &&...) {}
};

template <typename _Graph, typename _Type, typename _DiscriminatingT>
struct arc_map_if<true, _Graph, _Type, _DiscriminatingT> {
    arc_map_t<_Graph, _Type> _map;

    [[nodiscard]] constexpr arc_map_if(_Graph & g)
        : _map(create_arc_map<_Type>(g)) {}

    [[nodiscard]] constexpr arc_map_if(_Graph & g, _Type && v)
        : _map(create_arc_map<_Type>(g, v)) {}

    constexpr auto operator[](const arc_t<_Graph> & v) const { return _map[v]; }
    constexpr decltype(auto) operator[](const arc_t<_Graph> & v) {
        return _map[v];
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DETAIL_MAP_IF_HPP