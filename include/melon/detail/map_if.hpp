#pragma once

#include <type_traits>

#include "melon/graph.hpp"

namespace melon {

template <bool Cond, typename Graph, typename Type,
          typename DiscriminatingT = int>
struct vertex_map_if {
    template <typename... Args>
    constexpr vertex_map_if(Args &&...) {}
};

template <typename Graph, typename Type, typename DiscriminatingT>
struct vertex_map_if<true, Graph, Type, DiscriminatingT> {
    vertex_map_t<Graph, Type> _map;

    constexpr vertex_map_if(Graph & g) : _map(create_vertex_map<Type>(g)) {}

    // `const Type &`, not `Type &&`: Type is a class template parameter, so
    // the latter was a plain rvalue reference and a named default value could
    // not be passed.
    constexpr vertex_map_if(Graph & g, const Type & v)
        : _map(create_vertex_map<Type>(g, v)) {}

    // decltype(auto), not auto: the const overload used to decay-copy the
    // mapped value while the mutable one returned a reference.
    constexpr decltype(auto) operator[](const vertex_t<Graph> & v) const {
        return _map[v];
    }
    constexpr decltype(auto) operator[](const vertex_t<Graph> & v) {
        return _map[v];
    }
    // Present so that a guarded map can be re-initialised between runs the
    // same way a plain vertex_map_t is; only instantiated where it is called.
    constexpr void fill(const Type & v) { _map.fill(v); }
};

template <bool Cond, typename Graph, typename Type,
          typename DiscriminatingT = int>
struct arc_map_if {
    template <typename... Args>
    constexpr arc_map_if(Args &&...) {}
};

template <typename Graph, typename Type, typename DiscriminatingT>
struct arc_map_if<true, Graph, Type, DiscriminatingT> {
    arc_map_t<Graph, Type> _map;

    constexpr arc_map_if(Graph & g) : _map(create_arc_map<Type>(g)) {}

    // See vertex_map_if above for both of these.
    constexpr arc_map_if(Graph & g, const Type & v)
        : _map(create_arc_map<Type>(g, v)) {}

    constexpr decltype(auto) operator[](const arc_t<Graph> & v) const {
        return _map[v];
    }
    constexpr decltype(auto) operator[](const arc_t<Graph> & v) {
        return _map[v];
    }
    // See vertex_map_if above.
    constexpr void fill(const Type & v) { _map.fill(v); }
};

}  // namespace melon
