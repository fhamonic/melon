#pragma once

#include <type_traits>

#include "melon/graph.hpp"

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

    // `const _Type &`, not `_Type &&`: _Type is a class template parameter, so
    // the latter was a plain rvalue reference and a named default value could
    // not be passed.
    [[nodiscard]] constexpr vertex_map_if(_Graph & g, const _Type & v)
        : _map(create_vertex_map<_Type>(g, v)) {}

    // decltype(auto), not auto: the const overload used to decay-copy the
    // mapped value while the mutable one returned a reference.
    constexpr decltype(auto) operator[](const vertex_t<_Graph> & v) const {
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

    // See vertex_map_if above for both of these.
    [[nodiscard]] constexpr arc_map_if(_Graph & g, const _Type & v)
        : _map(create_arc_map<_Type>(g, v)) {}

    constexpr decltype(auto) operator[](const arc_t<_Graph> & v) const {
        return _map[v];
    }
    constexpr decltype(auto) operator[](const arc_t<_Graph> & v) {
        return _map[v];
    }
};

}  // namespace melon
