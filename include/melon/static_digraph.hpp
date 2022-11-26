#ifndef MELON_STATIC_DIGRAPH_HPP
#define MELON_STATIC_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

#include "melon/data_structures/static_map.hpp"
#include "melon/utils/map_view.hpp"

namespace fhamonic {
namespace melon {

class static_digraph {
public:
    using vertex_t = unsigned int;
    using arc_t = unsigned int;

private:
    static_map<vertex_t, arc_t> _out_arc_begin;
    static_map<arc_t, vertex_t> _arc_target;
    static_map<arc_t, vertex_t> _arc_source;

    static_map<vertex_t, arc_t> _in_arc_begin;
    static_map<arc_t, arc_t> _in_arcs;

public:
    template <std::ranges::range S, std::ranges::range T>
    static_digraph(const std::size_t & nb_vertices, S && sources,
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
        static_map<vertex_t, arc_t> in_arc_count(nb_vertices, 0);
        for(auto && s : sources) ++_out_arc_begin[s];
        for(auto && t : targets) ++in_arc_count[t];
        std::exclusive_scan(_out_arc_begin.data(),
                            _out_arc_begin.data() + nb_vertices,
                            _out_arc_begin.data(), 0);
        std::exclusive_scan(in_arc_count.data(),
                            in_arc_count.data() + nb_vertices,
                            _in_arc_begin.data(), 0);
        for(auto && a : arcs()) {
            vertex_t t = _arc_target[a];
            arc_t end = (t + 1 < static_cast<vertex_t>(nb_vertices)
                             ? _in_arc_begin[t + 1]
                             : static_cast<arc_t>(nb_arcs()));
            _in_arcs[end - in_arc_count[t]] = a;
            --in_arc_count[t];
        }
    }

    static_digraph() = default;
    static_digraph(const static_digraph & graph) = default;
    static_digraph(static_digraph && graph) = default;

    static_digraph & operator=(const static_digraph &) = default;
    static_digraph & operator=(static_digraph &&) = default;

    auto nb_vertices() const noexcept { return _out_arc_begin.size(); }
    auto nb_arcs() const noexcept { return _arc_target.size(); }

    bool is_valid_node(const vertex_t & u) const noexcept {
        return u < nb_vertices();
    }
    bool is_valid_arc(const arc_t & u) const noexcept { return u < nb_arcs(); }

    auto vertices() const noexcept {
        return std::views::iota(static_cast<vertex_t>(0),
                                static_cast<vertex_t>(nb_vertices()));
    }
    auto arcs() const noexcept {
        return std::views::iota(static_cast<arc_t>(0),
                                static_cast<arc_t>(nb_arcs()));
    }

    auto out_arcs(const vertex_t & u) const noexcept {
        assert(is_valid_node(u));
        return std::views::iota(
            _out_arc_begin[u],
            (u + 1 < nb_vertices() ? _out_arc_begin[u + 1] : nb_arcs()));
    }
    auto in_arcs(const vertex_t & u) const noexcept {
        assert(is_valid_node(u));
        return std::span(
            _in_arcs.data() + _in_arc_begin[u],
            (u + 1 < nb_vertices() ? _in_arcs.data() + _in_arc_begin[u + 1]
                                   : _in_arcs.data() + nb_arcs()));
    }

    vertex_t source(const arc_t & a) const noexcept {
        assert(is_valid_arc(a));
        return _arc_source[a];
    }
    vertex_t target(const arc_t & a) const noexcept {
        assert(is_valid_arc(a));
        return _arc_target[a];
    }

    const auto & sources_map() const noexcept { return _arc_source; }
    const auto & targets_map() const noexcept { return _arc_target; }

    auto out_neighbors(const vertex_t & u) const noexcept {
        assert(is_valid_node(u));
        return std::span(
            _arc_target.data() + _out_arc_begin[u],
            (u + 1 < nb_vertices() ? _arc_target.data() + _out_arc_begin[u + 1]
                                   : _arc_target.data() + nb_arcs()));
    }
    auto in_neighbors(const vertex_t & u) const noexcept {
        assert(is_valid_node(u));
        return std::views::transform(in_arcs(u),
                                     [this](auto a) { return source(a); });
    }

    auto out_arcs_pairs(const vertex_t & u) const noexcept {
        assert(is_valid_node(u));
        return std::views::transform(
            out_neighbors(u), [u](auto v) { return std::make_pair(u, v); });
    }
    auto arcs_pairs() const noexcept {
        return std::views::join(std::views::transform(
            vertices(), [this](auto u) { return out_arcs_pairs(u); }));
    }

    template <typename T>
    static_map<vertex_t, T> create_vertex_map() const noexcept {
        return static_map<vertex_t, T>(nb_vertices());
    }
    template <typename T>
    static_map<vertex_t, T> create_vertex_map(
        const T & default_value) const noexcept {
        return static_map<vertex_t, T>(nb_vertices(), default_value);
    }

    template <typename T>
    static_map<arc_t, T> create_arc_map() const noexcept {
        return static_map<arc_t, T>(nb_arcs());
    }
    template <typename T>
    static_map<arc_t, T> create_arc_map(
        const T & default_value) const noexcept {
        return static_map<arc_t, T>(nb_arcs(), default_value);
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STATIC_DIGRAPH_HPP