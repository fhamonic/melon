#pragma once

#include <concepts>
#include <type_traits>

#include "melon/detail/fill.hpp"
#include "melon/graph.hpp"

namespace melon::detail {

// Role is forwarded to the factory, and it also keeps two disabled maps of the
// same Graph and Type *distinct* empty types: two [[no_unique_address]]
// members of the same type may not share an address, so giving them the same
// Role costs a byte of padding in every algorithm holding a pair of them.
//
// The disabled variants mirror the enabled constructors instead of accepting
// a greedy variadic: a malformed construction must fail in the author's own
// build, not only for the first user whose traits turn the map on. Graph and
// Type are valid types whatever Cond guards, so the mirrors are always
// writable.
template <bool Cond, typename Graph, typename Type,
          typename Role = default_role>
struct vertex_map_if {
    constexpr vertex_map_if() = default;
    constexpr vertex_map_if(const Graph &) {}
    constexpr vertex_map_if(const Graph &, const Type &) {}
};

template <typename Graph, typename Type, typename Role>
struct vertex_map_if<true, Graph, Type, Role> {
    vertex_map_t<Graph, Type, Role> _map;

    // Without this, a holder's defaulted default constructor is silently
    // defaulted-as-deleted the moment its map condition turns on. The empty
    // state it hands out is the map's own moved-from state, already valid.
    constexpr vertex_map_if()
        requires std::default_initializable<vertex_map_t<Graph, Type, Role>>
    = default;

    constexpr vertex_map_if(const Graph & g)
        : _map(create_vertex_map<Type, Role>(g)) {}

    // `const Type &`, not `Type &&`: Type is a class template parameter, so
    // `Type &&` is a plain rvalue reference and rejects a named default value.
    constexpr vertex_map_if(const Graph & g, const Type & v)
        : _map(create_vertex_map<Type, Role>(g, v)) {}

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
        requires member_fillable<vertex_map_t<Graph, Type, Role>, Type>
    {
        _map.fill(v);
    }
};

template <bool Cond, typename Graph, typename Type,
          typename Role = default_role>
struct arc_map_if {
    constexpr arc_map_if() = default;
    constexpr arc_map_if(const Graph &) {}
    constexpr arc_map_if(const Graph &, const Type &) {}
};

template <typename Graph, typename Type, typename Role>
struct arc_map_if<true, Graph, Type, Role> {
    arc_map_t<Graph, Type, Role> _map;

    // See vertex_map_if above for all four of these.
    constexpr arc_map_if()
        requires std::default_initializable<arc_map_t<Graph, Type, Role>>
    = default;

    constexpr arc_map_if(const Graph & g)
        : _map(create_arc_map<Type, Role>(g)) {}

    constexpr arc_map_if(const Graph & g, const Type & v)
        : _map(create_arc_map<Type, Role>(g, v)) {}

    constexpr decltype(auto) operator[](const arc_t<Graph> & v) const
        noexcept(noexcept(_map[v])) {
        return _map[v];
    }
    constexpr decltype(auto) operator[](const arc_t<Graph> & v) noexcept(
        noexcept(_map[v])) {
        return _map[v];
    }
    constexpr void fill(const Type & v) noexcept(noexcept(_map.fill(v)))
        requires member_fillable<arc_map_t<Graph, Type, Role>, Type>
    {
        _map.fill(v);
    }
};

}  // namespace melon::detail
