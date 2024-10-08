#ifndef MELON_STATIC_DIGRAPH_HPP
#define MELON_STATIC_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

#include "melon/container/static_filter_map.hpp"
#include "melon/container/static_map.hpp"
#include "melon/mapping.hpp"

namespace fhamonic {
namespace melon {

class static_digraph {
private:
    using vertex = unsigned int;
    using arc = unsigned int;

    static_map<vertex, arc> _out_arc_begin;
    static_map<arc, vertex> _arc_target;
    static_map<arc, vertex> _arc_source;

    static_map<vertex, arc> _in_arc_begin;
    static_map<arc, arc> _in_arcs;

public:
    [[nodiscard]] static_digraph() = default;
    [[nodiscard]] static_digraph(const static_digraph & graph) = default;
    [[nodiscard]] static_digraph(static_digraph && graph) = default;

    static_digraph & operator=(const static_digraph &) = default;
    static_digraph & operator=(static_digraph &&) = default;

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

    [[nodiscard]] constexpr auto out_arcs(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        return std::views::iota(
            _out_arc_begin[u],
            (u + 1 < num_vertices() ? _out_arc_begin[u + 1] : num_arcs()));
    }
    [[nodiscard]] constexpr auto in_arcs(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        return std::span(
            _in_arcs.data() + _in_arc_begin[u],
            (u + 1 < num_vertices() ? _in_arcs.data() + _in_arc_begin[u + 1]
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

    [[nodiscard]] auto arc_sources_map() const noexcept {
        return mapping_ref_view(_arc_source);
    }
    [[nodiscard]] auto arc_targets_map() const noexcept {
        return mapping_ref_view(_arc_target);
    }

    [[nodiscard]] constexpr auto out_neighbors(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        return std::span(
            _arc_target.data() + _out_arc_begin[u],
            (u + 1 < num_vertices() ? _arc_target.data() + _out_arc_begin[u + 1]
                                   : _arc_target.data() + num_arcs()));
    }

    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map() const noexcept {
        return static_map<vertex, T>(num_vertices());
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map(
        const T & default_value) const noexcept {
        return static_map<vertex, T>(num_vertices(), default_value);
    }

    template <typename T>
    [[nodiscard]] constexpr auto create_arc_map() const noexcept {
        return static_map<arc, T>(num_arcs());
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_arc_map(
        const T & default_value) const noexcept {
        return static_map<arc, T>(num_arcs(), default_value);
    }

public:
    template <std::ranges::forward_range S, std::ranges::forward_range T>
        requires std::convertible_to<std::ranges::range_value_t<S>, vertex> &&
                     std::convertible_to<std::ranges::range_value_t<T>, vertex>
    [[nodiscard]] static_digraph(const std::size_t & num_vertices, S && sources,
                                 T && targets) noexcept
        : _out_arc_begin(num_vertices, 0)
        , _arc_target(std::forward<T>(targets))
        , _arc_source(std::forward<S>(sources))
        , _in_arc_begin(num_vertices, 0)
        , _in_arcs(_arc_target.size()) {
        assert(std::ranges::all_of(
            sources, [n = num_vertices](auto && v) { return v < n; }));
        assert(std::ranges::all_of(
            targets, [n = num_vertices](auto && v) { return v < n; }));
        assert(std::ranges::is_sorted(sources));
        static_map<vertex, arc> in_arc_count(num_vertices, 0);
        for(auto && s : sources) ++_out_arc_begin[s];
        for(auto && t : targets) ++in_arc_count[t];
        std::exclusive_scan(_out_arc_begin.data(),
                            _out_arc_begin.data() + num_vertices,
                            _out_arc_begin.data(), 0);
        std::exclusive_scan(in_arc_count.data(),
                            in_arc_count.data() + num_vertices,
                            _in_arc_begin.data(), 0);
        for(auto && a : arcs()) {
            vertex t = _arc_target[a];
            --in_arc_count[t];
            _in_arcs[_in_arc_begin[t] + in_arc_count[t]] = a;
        }
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STATIC_DIGRAPH_HPP