#pragma once

#include <type_traits>

#include "melon/graph.hpp"

namespace melon {

// DiscriminatingT takes no part in the type; it exists so that two disabled
// maps of the same Graph and Type are still *distinct* empty types. Two
// [[no_unique_address]] members of the same type may not share an address, so
// giving them the same DiscriminatingT costs a byte of padding in every
// algorithm holding a pair of them (bidirectional_dijkstra).
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
    // `Type &&` is a plain rvalue reference and rejects a named default value.
    constexpr vertex_map_if(Graph & g, const Type & v)
        : _map(create_vertex_map<Type>(g, v)) {}

    // decltype(auto), not auto: `auto` decay-copies, so the two overloads would
    // disagree on whether a subscript yields a reference into the map.
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
