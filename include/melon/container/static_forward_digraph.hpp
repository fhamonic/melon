#ifndef MELON_STATIC_FORWARD_DIGRAPH_HPP
#define MELON_STATIC_FORWARD_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <numeric>
#include <ranges>
#include <span>

#include "melon/container/static_map.hpp"

namespace fhamonic {
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
    static_forward_digraph(const std::size_t & num_vertices, S && sources,
                           T && targets) noexcept
        : _out_arc_begin(num_vertices, 0), _arc_target(std::move(targets)) {
        assert(std::ranges::all_of(
            sources, [n = num_vertices](auto && v) { return v < n; }));
        assert(std::ranges::all_of(
            targets, [n = num_vertices](auto && v) { return v < n; }));
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

    auto num_vertices() const noexcept { return _out_arc_begin.size(); }
    auto num_arcs() const noexcept { return _arc_target.size(); }

    bool is_valid_vertex(const vertex & u) const noexcept {
        return u < num_vertices();
    }
    bool is_valid_arc(const arc & u) const noexcept { return u < num_arcs(); }

    auto vertices() const noexcept {
        return std::views::iota(static_cast<vertex>(0),
                                static_cast<vertex>(num_vertices()));
    }
    auto arcs() const noexcept {
        return std::views::iota(static_cast<arc>(0),
                                static_cast<arc>(num_arcs()));
    }
    auto out_arcs(const vertex & u) const noexcept {
        assert(is_valid_vertex(u));
        return std::views::iota(
            _out_arc_begin[u],
            (u + 1 < num_vertices() ? _out_arc_begin[u + 1] : num_arcs()));
    }
    vertex arc_target(const arc & a) const noexcept {
        assert(is_valid_arc(a));
        return _arc_target[a];
    }
    const auto & arc_targets_map() const { return _arc_target; }
    auto out_neighbors(const vertex & u) const noexcept {
        assert(is_valid_vertex(u));
        return std::span(
            _arc_target.data() + _out_arc_begin[u],
            (u + 1 < num_vertices() ? _arc_target.data() + _out_arc_begin[u + 1]
                                   : _arc_target.data() + num_arcs()));
    }

    template <typename T>
    static_map<vertex, T> create_vertex_map() const noexcept {
        return static_map<vertex, T>(num_vertices());
    }
    template <typename T>
    static_map<vertex, T> create_vertex_map(
        const T & default_value) const noexcept {
        return static_map<vertex, T>(num_vertices(), default_value);
    }

    template <typename T>
    static_map<arc, T> create_arc_map() const noexcept {
        return static_map<arc, T>(num_arcs());
    }
    template <typename T>
    static_map<arc, T> create_arc_map(const T & default_value) const noexcept {
        return static_map<arc, T>(num_arcs(), default_value);
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STATIC_FORWARD_DIGRAPH_HPP