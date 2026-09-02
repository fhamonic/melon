#pragma once

#include <concepts>
#include <type_traits>

#include "melon/detail/fill.hpp"
#include "melon/graph.hpp"

namespace melon::detail {

// DiscriminatingT takes no part in the type; it exists so that two disabled
// maps of the same Graph and Type are still *distinct* empty types. Two
// [[no_unique_address]] members of the same type may not share an address, so
// giving them the same DiscriminatingT costs a byte of padding in every
// algorithm holding a pair of them (bidirectional_dijkstra).
//
// The disabled variants mirror the enabled constructors instead of accepting
// a greedy variadic: a malformed construction must fail in the author's own
// build, not only for the first user whose traits turn the map on. Graph and
// Type are valid types whatever Cond guards, so the mirrors are always
// writable.
template <bool Cond, typename Graph, typename Type,
          typename DiscriminatingT = int>
struct vertex_map_if {
    constexpr vertex_map_if() = default;
    constexpr vertex_map_if(const Graph &) {}
    constexpr vertex_map_if(const Graph &, const Type &) {}
};

template <typename Graph, typename Type, typename DiscriminatingT>
struct vertex_map_if<true, Graph, Type, DiscriminatingT> {
    vertex_map_t<Graph, Type> _map;

    // Without this, a holder's defaulted default constructor is silently
    // defaulted-as-deleted the moment its map condition turns on. The empty
    // state it hands out is the map's own moved-from state, already valid.
    constexpr vertex_map_if()
        requires std::default_initializable<vertex_map_t<Graph, Type>>
    = default;

    constexpr vertex_map_if(const Graph & g)
        : _map(create_vertex_map<Type>(g)) {}

    // `const Type &`, not `Type &&`: Type is a class template parameter, so
    // `Type &&` is a plain rvalue reference and rejects a named default value.
    constexpr vertex_map_if(const Graph & g, const Type & v)
        : _map(create_vertex_map<Type>(g, v)) {}

    // decltype(auto), not auto: `auto` decay-copies, so the two overloads would
    // disagree on whether a subscript yields a reference into the map.
    constexpr decltype(auto) operator[](const vertex_t<Graph> & v) const
        noexcept(noexcept(_map[v])) {
        return _map[v];
    }
    constexpr decltype(auto) operator[](const vertex_t<Graph> & v) noexcept(
        noexcept(_map[v])) {
        return _map[v];
    }
    // Present so that a guarded map can be re-initialised between runs the
    // same way a plain vertex_map_t is. Constrained, not merely lazily
    // instantiated: detail::fill probes for this member, and an unconstrained
    // declaration over a fill-less map turns that probe into a hard error in
    // the noexcept-specifier.
    constexpr void fill(const Type & v) noexcept(noexcept(_map.fill(v)))
        requires member_fillable<vertex_map_t<Graph, Type>, Type>
    {
        _map.fill(v);
    }
};

template <bool Cond, typename Graph, typename Type,
          typename DiscriminatingT = int>
struct arc_map_if {
    constexpr arc_map_if() = default;
    constexpr arc_map_if(const Graph &) {}
    constexpr arc_map_if(const Graph &, const Type &) {}
};

template <typename Graph, typename Type, typename DiscriminatingT>
struct arc_map_if<true, Graph, Type, DiscriminatingT> {
    arc_map_t<Graph, Type> _map;

    // See vertex_map_if above for all four of these.
    constexpr arc_map_if()
        requires std::default_initializable<arc_map_t<Graph, Type>>
    = default;

    constexpr arc_map_if(const Graph & g) : _map(create_arc_map<Type>(g)) {}

    constexpr arc_map_if(const Graph & g, const Type & v)
        : _map(create_arc_map<Type>(g, v)) {}

    constexpr decltype(auto) operator[](const arc_t<Graph> & v) const
        noexcept(noexcept(_map[v])) {
        return _map[v];
    }
    constexpr decltype(auto) operator[](const arc_t<Graph> & v) noexcept(
        noexcept(_map[v])) {
        return _map[v];
    }
    constexpr void fill(const Type & v) noexcept(noexcept(_map.fill(v)))
        requires member_fillable<arc_map_t<Graph, Type>, Type>
    {
        _map.fill(v);
    }
};

}  // namespace melon::detail
