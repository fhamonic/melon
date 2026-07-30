#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <numeric>
#include <ranges>
#include <span>

#include "melon/container/static_map.hpp"
#include "melon/mapping.hpp"

namespace melon {

class static_forward_digraph {
private:
    using vertex = unsigned int;
    using arc = unsigned int;

    static_map<vertex, arc> _out_arc_begin;
    static_map<arc, vertex> _arc_target;

public:
    template <std::ranges::forward_range S, std::ranges::forward_range T>
        requires std::convertible_to<std::ranges::range_value_t<S>, vertex> &&
                     std::convertible_to<std::ranges::range_value_t<T>, vertex>
    // Not noexcept: builds two static_maps, i.e. two allocations.
    // std::forward, not std::move: `targets` is a forwarding reference, so
    // std::move stole from an lvalue the caller still owns. The checks below
    // then read the member rather than the parameter -- after forwarding,
    // `targets` may legitimately be empty. It only ever worked because
    // static_map's range constructor copies.
    static_forward_digraph(const std::size_t & num_vertices, S && sources,
                           T && targets)
        : _out_arc_begin(num_vertices, 0)
        , _arc_target(std::forward<T>(targets)) {
        assert(std::ranges::all_of(
            sources, [n = num_vertices](auto && v) { return v < n; }));
        assert(std::ranges::all_of(
            _arc_target, [n = num_vertices](auto && v) { return v < n; }));
        assert(std::ranges::is_sorted(sources));
        for(auto && s : sources) ++_out_arc_begin[s];
        std::exclusive_scan(_out_arc_begin.data(),
                            _out_arc_begin.data() + num_vertices,
                            _out_arc_begin.data(), 0);
    }

    static_forward_digraph() = default;
    static_forward_digraph(const static_forward_digraph & graph) = default;
    static_forward_digraph(static_forward_digraph && graph) = default;

    static_forward_digraph & operator=(const static_forward_digraph &) =
        default;
    static_forward_digraph & operator=(static_forward_digraph &&) = default;

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
    // See static_digraph::out_arcs: cast both ends to `arc`, or the ternary's
    // common type is num_arcs()'s std::size_t and the range is a non-common
    // 16-byte iota instead of a common 8-byte one.
    [[nodiscard]] constexpr auto out_arcs(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        return std::views::iota(
            _out_arc_begin[u],
            static_cast<arc>(u + 1 < num_vertices() ? _out_arc_begin[u + 1]
                                                    : num_arcs()));
    }
    [[nodiscard]] constexpr vertex arc_target(const arc a) const noexcept {
        assert(is_valid_arc(a));
        return _arc_target[a];
    }
    // See static_digraph's twin: constexpr and noexcept, like it.
    [[nodiscard]] constexpr auto arc_targets_map() const noexcept {
        return mapping_ref_view(_arc_target);
    }
    [[nodiscard]] constexpr auto out_neighbors(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        return std::span(
            _arc_target.data() + _out_arc_begin[u],
            (u + 1 < num_vertices() ? _arc_target.data() + _out_arc_begin[u + 1]
                                    : _arc_target.data() + num_arcs()));
    }

    // None of the four below are noexcept: they allocate.
    template <typename T>
    [[nodiscard]] constexpr static_map<vertex, T> create_vertex_map() const {
        return static_map<vertex, T>(num_vertices());
    }
    template <typename T>
    [[nodiscard]] constexpr static_map<vertex, T> create_vertex_map(
        const T & default_value) const {
        return static_map<vertex, T>(num_vertices(), default_value);
    }

    template <typename T>
    [[nodiscard]] constexpr static_map<arc, T> create_arc_map() const {
        return static_map<arc, T>(num_arcs());
    }
    template <typename T>
    [[nodiscard]] constexpr static_map<arc, T> create_arc_map(
        const T & default_value) const {
        return static_map<arc, T>(num_arcs(), default_value);
    }
};

}  // namespace melon
