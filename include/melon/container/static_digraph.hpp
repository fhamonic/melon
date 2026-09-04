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
#include <vector>

#include "melon/container/static_map.hpp"
#include "melon/mapping.hpp"

namespace melon {

// Unsigned handles only: the constructor's endpoint checks and every
// incidence bound are `<` comparisons that a signed id would pass at -1.
template <std::unsigned_integral V = unsigned int,
          std::unsigned_integral A = unsigned int>
class basic_static_digraph {
private:
    using vertex = V;
    using arc = A;

    static_map<vertex, arc> _out_arc_begin;
    static_map<arc, vertex> _arc_target;
    static_map<arc, vertex> _arc_source;

    static_map<vertex, arc> _in_arc_begin;
    static_map<arc, arc> _in_arcs;

public:
    basic_static_digraph() = default;
    basic_static_digraph(const basic_static_digraph & graph) = default;
    basic_static_digraph(basic_static_digraph && graph) = default;

    basic_static_digraph & operator=(const basic_static_digraph &) = default;
    basic_static_digraph & operator=(basic_static_digraph &&) = default;

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

    // Both ends cast to `arc`. Without the cast the ternary's common type is
    // num_arcs()'s std::size_t, so this yields an iota_view<arc, size_t>: a
    // *non-common* range, 16 bytes instead of 8, comparing an arc against a
    // size_t on every iteration. The size shows up multiplied --
    // depth_first_search keeps one such cursor per stack frame and dinitz one
    // per vertex in each of two maps.
    [[nodiscard]] constexpr auto out_arcs(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        const vertex next = next_vertex(u);
        return std::views::iota(
            _out_arc_begin[u],
            static_cast<arc>(next < num_vertices() ? _out_arc_begin[next]
                                                   : num_arcs()));
    }
    [[nodiscard]] constexpr auto in_arcs(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        const vertex next = next_vertex(u);
        return std::span(
            _in_arcs.data() + _in_arc_begin[u],
            (next < num_vertices() ? _in_arcs.data() + _in_arc_begin[next]
                                   : _in_arcs.data() + num_arcs()));
    }

    [[nodiscard]] constexpr vertex arc_source(const arc a) const noexcept {
        assert(is_valid_arc(a));
        return _arc_source[a];
    }
    [[nodiscard]] constexpr vertex arc_target(const arc a) const noexcept {
        assert(is_valid_arc(a));
        return _arc_target[a];
    }

    [[nodiscard]] constexpr auto arc_sources_map() const noexcept {
        return mapping_ref_view(_arc_source);
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
    // fill-assign for the default-value form -- so has_vertex_map/has_arc_map
    // answer false for value types the maps cannot hold, instead of
    // hard-erroring during return-type deduction.
    template <typename T>
        requires std::default_initializable<T>
    [[nodiscard]] constexpr auto create_vertex_map() const {
        return static_map<vertex, T>(num_vertices());
    }
    template <typename T>
        requires std::default_initializable<T> &&
                 std::assignable_from<T &, const T &>
    [[nodiscard]] constexpr auto create_vertex_map(
        const T & default_value) const {
        return static_map<vertex, T>(num_vertices(), default_value);
    }

    template <typename T>
        requires std::default_initializable<T>
    [[nodiscard]] constexpr auto create_arc_map() const {
        return static_map<arc, T>(num_arcs());
    }
    template <typename T>
        requires std::default_initializable<T> &&
                 std::assignable_from<T &, const T &>
    [[nodiscard]] constexpr auto create_arc_map(const T & default_value) const {
        return static_map<arc, T>(num_arcs(), default_value);
    }

public:
    template <std::ranges::forward_range S, std::ranges::forward_range T>
        requires std::convertible_to<std::ranges::range_value_t<S>, vertex> &&
                     std::convertible_to<std::ranges::range_value_t<T>, vertex>
    basic_static_digraph(const std::size_t & num_vertices_, S && sources,
                         T && targets)
        : _out_arc_begin(num_vertices_, 0)
        , _arc_target(std::forward<T>(targets))
        , _arc_source(std::forward<S>(sources))
        , _in_arc_begin(num_vertices_, 0)
        , _in_arcs(_arc_target.size()) {
        // At most max, not max + 1: vertices() and arcs() are iotas whose
        // end is the count cast to the handle type, and a count of exactly
        // max + 1 casts to 0 -- an empty range over a full graph.
        assert(num_vertices_ <= std::numeric_limits<vertex>::max());
        assert(_arc_target.size() <= std::numeric_limits<arc>::max());
        // Read the members, not the parameters: both were forwarded into
        // _arc_source / _arc_target above, so after a move the parameters may
        // legitimately be empty and every assertion below would pass
        // vacuously. The members are contiguous, so this is also the cheaper
        // scan.
        assert(_arc_source.size() == _arc_target.size());
        assert(std::ranges::all_of(
            _arc_source, [n = num_vertices_](auto && v) { return v < n; }));
        assert(std::ranges::all_of(
            _arc_target, [n = num_vertices_](auto && v) { return v < n; }));
        assert(std::ranges::is_sorted(_arc_source));
        static_map<vertex, arc> in_arc_count(num_vertices_, 0);
        for(auto && s : _arc_source) ++_out_arc_begin[s];
        for(auto && t : _arc_target) ++in_arc_count[t];
        // arc{0}, not 0: exclusive_scan accumulates in the init value's type,
        // and an int accumulator is signed-overflow UB past INT_MAX arcs. The
        // explicit op keeps a 16-bit arc from being promoted to int and
        // narrowed back inside the scan (MSVC C4242).
        std::exclusive_scan(
            _out_arc_begin.data(), _out_arc_begin.data() + num_vertices_,
            _out_arc_begin.data(), arc{0},
            [](arc a, arc b) { return static_cast<arc>(a + b); });
        std::exclusive_scan(
            in_arc_count.data(), in_arc_count.data() + num_vertices_,
            _in_arc_begin.data(), arc{0},
            [](arc a, arc b) { return static_cast<arc>(a + b); });
        // Descending over the arc ids: each bucket fills from its back, so
        // walking the ids backwards leaves every in_arcs() range ascending --
        // the order out_arcs() already has, and the forward stride every arc
        // map indexed inside an in_arcs loop prefers.
        for(arc a = static_cast<arc>(num_arcs()); a-- > 0;) {
            vertex t = _arc_target[a];
            --in_arc_count[t];
            _in_arcs[static_cast<arc>(_in_arc_begin[t] + in_arc_count[t])] = a;
        }
    }
};

using static_digraph = basic_static_digraph<>;

}  // namespace melon
