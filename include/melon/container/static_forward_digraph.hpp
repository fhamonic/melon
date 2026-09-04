#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <numeric>
#include <ranges>
#include <span>
#include <utility>

#include "melon/container/static_map.hpp"
#include "melon/mapping.hpp"

namespace melon {

// Unsigned handles only: the constructor's endpoint checks and every
// incidence bound are `<` comparisons that a signed id would pass at -1.
template <std::unsigned_integral V = unsigned int,
          std::unsigned_integral A = unsigned int>
class basic_static_forward_digraph {
private:
    using vertex = V;
    using arc = A;

    static_map<vertex, arc> _out_arc_begin;
    static_map<arc, vertex> _arc_target;

public:
    template <std::ranges::forward_range S, std::ranges::forward_range T>
        requires std::convertible_to<std::ranges::range_value_t<S>, vertex> &&
                     std::convertible_to<std::ranges::range_value_t<T>, vertex>
    // Not noexcept: builds two static_maps, i.e. two allocations.
    // std::forward, not std::move: `targets` is a forwarding reference, and
    // std::move would steal from an lvalue the caller still owns. The checks
    // below therefore read _arc_target rather than `targets`, which after
    // forwarding may legitimately be empty and assert vacuously.
    basic_static_forward_digraph(const std::size_t & num_vertices_,
                                 S && sources, T && targets)
        : _out_arc_begin(num_vertices_, 0)
        , _arc_target(std::forward<T>(targets)) {
        // At most max, not max + 1: vertices() and arcs() are iotas whose
        // end is the count cast to the handle type, and a count of exactly
        // max + 1 casts to 0 -- an empty range over a full graph.
        assert(num_vertices_ <= std::numeric_limits<vertex>::max());
        assert(_arc_target.size() <= std::numeric_limits<arc>::max());
        assert(static_cast<std::size_t>(std::ranges::distance(sources)) ==
               _arc_target.size());
        assert(std::ranges::all_of(
            sources, [n = num_vertices_](auto && v) { return v < n; }));
        assert(std::ranges::all_of(
            _arc_target, [n = num_vertices_](auto && v) { return v < n; }));
        assert(std::ranges::is_sorted(sources));
        for(auto && s : sources) ++_out_arc_begin[s];
        // arc{0}, not 0: exclusive_scan accumulates in the init value's type,
        // and an int accumulator is signed-overflow UB past INT_MAX arcs.
        std::exclusive_scan(_out_arc_begin.data(),
                            _out_arc_begin.data() + num_vertices_,
                            _out_arc_begin.data(), arc{0});
    }

    basic_static_forward_digraph() = default;
    basic_static_forward_digraph(const basic_static_forward_digraph & graph) =
        default;
    basic_static_forward_digraph(basic_static_forward_digraph && graph) =
        default;

    basic_static_forward_digraph & operator=(
        const basic_static_forward_digraph &) = default;
    basic_static_forward_digraph & operator=(basic_static_forward_digraph &&) =
        default;

    [[nodiscard]] constexpr auto num_vertices() const noexcept {
        return _out_arc_begin.size();
    }
    [[nodiscard]] constexpr auto num_arcs() const noexcept {
        return _arc_target.size();
    }

    [[nodiscard]] constexpr bool is_valid_vertex(
        const vertex u) const noexcept {
        return u < num_vertices();
    }
    [[nodiscard]] constexpr bool is_valid_arc(const arc u) const noexcept {
        return u < num_arcs();
    }

    [[nodiscard]] constexpr auto vertices() const noexcept {
        return std::views::iota(static_cast<vertex>(0),
                                static_cast<vertex>(num_vertices()));
    }
    [[nodiscard]] constexpr auto arcs() const noexcept {
        return std::views::iota(static_cast<arc>(0),
                                static_cast<arc>(num_arcs()));
    }
    // `u + 1` computed back in `vertex`: on a handle narrower than int the
    // sum promotes to int, and indexing with it is a narrowing conversion.
    // It cannot wrap, since the constructor caps num_vertices at the handle's
    // max.
    [[nodiscard]] constexpr vertex next_vertex(const vertex u) const noexcept {
        return static_cast<vertex>(u + 1);
    }

    // Cast both ends to `arc`, or the ternary's common type is num_arcs()'s
    // std::size_t and the range is a non-common 16-byte iota instead of a
    // common 8-byte one.
    [[nodiscard]] constexpr auto out_arcs(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        const vertex next = next_vertex(u);
        return std::views::iota(
            _out_arc_begin[u],
            static_cast<arc>(next < num_vertices() ? _out_arc_begin[next]
                                                   : num_arcs()));
    }
    [[nodiscard]] constexpr vertex arc_target(const arc a) const noexcept {
        assert(is_valid_arc(a));
        return _arc_target[a];
    }
    [[nodiscard]] constexpr auto arc_targets_map() const noexcept {
        return mapping_ref_view(_arc_target);
    }
    [[nodiscard]] constexpr auto out_neighbors(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        const vertex next = next_vertex(u);
        return std::span(
            _arc_target.data() + _out_arc_begin[u],
            (next < num_vertices() ? _arc_target.data() + _out_arc_begin[next]
                                   : _arc_target.data() + num_arcs()));
    }

    // None of the four below are noexcept: they allocate. Each is constrained
    // on what static_map's constructor does with T -- default-init, plus
    // fill-assign for the default-value form -- so the map-creation concepts
    // answer false for value types the maps cannot hold, instead of the
    // declaration promising a construction whose body cannot compile.
    template <typename T>
        requires std::default_initializable<T>
    [[nodiscard]] constexpr static_map<vertex, T> create_vertex_map() const {
        return static_map<vertex, T>(num_vertices());
    }
    template <typename T>
        requires std::default_initializable<T> &&
                 std::assignable_from<T &, const T &>
    [[nodiscard]] constexpr static_map<vertex, T> create_vertex_map(
        const T & default_value) const {
        return static_map<vertex, T>(num_vertices(), default_value);
    }

    template <typename T>
        requires std::default_initializable<T>
    [[nodiscard]] constexpr static_map<arc, T> create_arc_map() const {
        return static_map<arc, T>(num_arcs());
    }
    template <typename T>
        requires std::default_initializable<T> &&
                 std::assignable_from<T &, const T &>
    [[nodiscard]] constexpr static_map<arc, T> create_arc_map(
        const T & default_value) const {
        return static_map<arc, T>(num_arcs(), default_value);
    }
};

using static_forward_digraph = basic_static_forward_digraph<>;

}  // namespace melon
