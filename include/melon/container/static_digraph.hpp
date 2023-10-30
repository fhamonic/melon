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
    template <std::ranges::forward_range S, std::ranges::forward_range T>
        requires std::convertible_to<std::ranges::range_value_t<S>, vertex> &&
                     std::convertible_to<std::ranges::range_value_t<T>, vertex>
    [[nodiscard]] static_digraph(const std::size_t & nb_vertices, S && sources,
                                 T && targets) noexcept
        : _out_arc_begin(nb_vertices, 0)
        , _arc_target(std::forward<T>(targets))
        , _arc_source(std::forward<S>(sources))
        , _in_arc_begin(nb_vertices, 0)
        , _in_arcs(_arc_target.size()) {
        assert(std::ranges::all_of(
            sources, [n = nb_vertices](auto && v) { return v < n; }));
        assert(std::ranges::all_of(
            targets, [n = nb_vertices](auto && v) { return v < n; }));
        assert(std::ranges::is_sorted(sources));
        static_map<vertex, arc> in_arc_count(nb_vertices, 0);
        for(auto && s : sources) ++_out_arc_begin[s];
        for(auto && t : targets) ++in_arc_count[t];
        std::exclusive_scan(_out_arc_begin.data(),
                            _out_arc_begin.data() + nb_vertices,
                            _out_arc_begin.data(), 0);
        std::exclusive_scan(in_arc_count.data(),
                            in_arc_count.data() + nb_vertices,
                            _in_arc_begin.data(), 0);
        for(auto && a : arcs()) {
            vertex t = _arc_target[a];
            --in_arc_count[t];
            _in_arcs[_in_arc_begin[t] + in_arc_count[t]] = a;
        }
    }

    [[nodiscard]] static_digraph() = default;
    [[nodiscard]] static_digraph(const static_digraph & graph) = default;
    [[nodiscard]] static_digraph(static_digraph && graph) = default;

    static_digraph & operator=(const static_digraph &) = default;
    static_digraph & operator=(static_digraph &&) = default;

    [[nodiscard]] constexpr auto nb_vertices() const noexcept {
        return _out_arc_begin.size();
    }
    [[nodiscard]] constexpr auto nb_arcs() const noexcept {
        return _arc_target.size();
    }

    [[nodiscard]] constexpr bool is_valid_vertex(
        const vertex u) const noexcept {
        return u < nb_vertices();
    }
    [[nodiscard]] constexpr bool is_valid_arc(const arc u) const noexcept {
        return u < nb_arcs();
    }

    [[nodiscard]] constexpr auto vertices() const noexcept {
        return std::views::iota(static_cast<vertex>(0),
                                static_cast<vertex>(nb_vertices()));
    }
    [[nodiscard]] constexpr auto arcs() const noexcept {
        return std::views::iota(static_cast<arc>(0),
                                static_cast<arc>(nb_arcs()));
    }

    [[nodiscard]] constexpr auto out_arcs(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        return std::views::iota(
            _out_arc_begin[u],
            (u + 1 < nb_vertices() ? _out_arc_begin[u + 1] : nb_arcs()));
    }
    [[nodiscard]] constexpr auto in_arcs(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        return std::span(
            _in_arcs.data() + _in_arc_begin[u],
            (u + 1 < nb_vertices() ? _in_arcs.data() + _in_arc_begin[u + 1]
                                   : _in_arcs.data() + nb_arcs()));
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
            (u + 1 < nb_vertices() ? _arc_target.data() + _out_arc_begin[u + 1]
                                   : _arc_target.data() + nb_arcs()));
    }

    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map() const noexcept {
        // if constexpr(std::same_as<T, bool>)
        //     return static_filter_map<vertex>(nb_vertices());
        // else
        return static_map<vertex, T>(nb_vertices());
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map(
        const T & default_value) const noexcept {
        // if constexpr(std::same_as<T, bool>)
        //     return static_filter_map<vertex>(nb_vertices(), default_value);
        // else
        return static_map<vertex, T>(nb_vertices(), default_value);
    }

    template <typename T>
    [[nodiscard]] constexpr auto create_arc_map() const noexcept {
        // if constexpr(std::same_as<T, bool>)
        //     return static_filter_map<arc>(nb_arcs());
        // else
        return static_map<arc, T>(nb_arcs());
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_arc_map(
        const T & default_value) const noexcept {
        // if constexpr(std::same_as<T, bool>)
        //     return static_filter_map<arc>(nb_arcs(), default_value);
        // else
        return static_map<arc, T>(nb_arcs(), default_value);
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STATIC_DIGRAPH_HPP