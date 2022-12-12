#ifndef MELON_STATIC_FORWARD_DIGRAPH_HPP
#define MELON_STATIC_FORWARD_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

#include "melon/concepts/range_of.hpp"
#include "melon/data_structures/static_map.hpp"

namespace fhamonic {
namespace melon {

class static_forward_digraph {
private:
    using vertex = unsigned int;
    using arc = unsigned int;

    static_map<vertex, arc> _out_arc_begin;
    static_map<arc, vertex> _arc_target;

public:
    template <concepts::forward_range_of<vertex> S,
              concepts::forward_range_of<vertex> T>
    static_forward_digraph(const std::size_t & nb_vertices, S && sources,
                           T && targets) noexcept
        : _out_arc_begin(nb_vertices, 0), _arc_target(std::move(targets)) {
        assert(std::ranges::all_of(
            sources, [n = nb_vertices](auto && v) { return v < n; }));
        assert(std::ranges::all_of(
            targets, [n = nb_vertices](auto && v) { return v < n; }));
        assert(std::ranges::is_sorted(sources));
        for(auto && s : sources) ++_out_arc_begin[s];
        std::exclusive_scan(_out_arc_begin.data(),
                            _out_arc_begin.data() + nb_vertices,
                            _out_arc_begin.data(), 0);
    }

    static_forward_digraph() = default;
    static_forward_digraph(const static_forward_digraph & graph) = default;
    static_forward_digraph(static_forward_digraph && graph) = default;

    static_forward_digraph & operator=(const static_forward_digraph &) =
        default;
    static_forward_digraph & operator=(static_forward_digraph &&) = default;

    auto nb_vertices() const noexcept { return _out_arc_begin.size(); }
    auto nb_arcs() const noexcept { return _arc_target.size(); }

    bool is_valid_vertex(const vertex & u) const noexcept {
        return u < nb_vertices();
    }
    bool is_valid_arc(const arc & u) const noexcept { return u < nb_arcs(); }

    auto vertices() const noexcept {
        return std::views::iota(static_cast<vertex>(0),
                                static_cast<vertex>(nb_vertices()));
    }
    auto arcs() const noexcept {
        return std::views::iota(static_cast<arc>(0),
                                static_cast<arc>(nb_arcs()));
    }
    auto out_arcs(const vertex & u) const noexcept {
        assert(is_valid_vertex(u));
        return std::views::iota(
            _out_arc_begin[u],
            (u + 1 < nb_vertices() ? _out_arc_begin[u + 1] : nb_arcs()));
    }
    vertex target(const arc & a) const noexcept {
        assert(is_valid_arc(a));
        return _arc_target[a];
    }
    const auto & targets_map() const { return _arc_target; }
    auto out_neighbors(const vertex & u) const noexcept {
        assert(is_valid_vertex(u));
        return std::span(
            _arc_target.data() + _out_arc_begin[u],
            (u + 1 < nb_vertices() ? _arc_target.data() + _out_arc_begin[u + 1]
                                   : _arc_target.data() + nb_arcs()));
    }

    auto out_arc_entries(const vertex & s) const noexcept {
        assert(is_valid_vertex(s));
        return std::views::transform(
            out_arcs(s), [this, s](const arc a) { return std::make_pair(a,std::make_pair(s, _arc_target[a])); });
    }
    auto arcs_entries() const noexcept {
        return std::views::join(std::views::transform(
            vertices(), [this](const vertex s) { return out_arc_entries(s); }));
    }

    template <typename T>
    static_map<vertex, T> create_vertex_map() const noexcept {
        return static_map<vertex, T>(nb_vertices());
    }
    template <typename T>
    static_map<vertex, T> create_vertex_map(
        const T & default_value) const noexcept {
        return static_map<vertex, T>(nb_vertices(), default_value);
    }

    template <typename T>
    static_map<arc, T> create_arc_map() const noexcept {
        return static_map<arc, T>(nb_arcs());
    }
    template <typename T>
    static_map<arc, T> create_arc_map(const T & default_value) const noexcept {
        return static_map<arc, T>(nb_arcs(), default_value);
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STATIC_FORWARD_DIGRAPH_HPP