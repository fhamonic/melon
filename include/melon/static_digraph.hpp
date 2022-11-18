#ifndef MELON_STATIC_DIGRAPH_HPP
#define MELON_STATIC_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <numeric>
#include <ranges>
#include <vector>

#include "melon/data_structures/static_map.hpp"

namespace fhamonic {
namespace melon {

class static_digraph {
public:
    using vertex_t = unsigned int;
    using arc_t = unsigned int;

    template <typename T>
    using vertex_map = static_map<T>;
    template <typename T>
    using arc_map = static_map<T>;

private:
    vertex_map<arc_t> _out_arc_begin;
    arc_map<vertex_t> _arc_target;
    arc_map<vertex_t> _arc_source;

    vertex_map<arc_t> _in_arc_begin;
    static_map<arc_t> _in_arcs;

public:
    template <std::ranges::range S, std::ranges::range T>
    static_digraph(std::size_t nb_vertices, S && sources, T && targets)
        : _out_arc_begin(nb_vertices, 0)
        , _arc_target(std::move(targets))
        , _arc_source(std::move(sources))
        , _in_arc_begin(nb_vertices, 0)
        , _in_arcs(_arc_target.size()) {
        assert(std::ranges::all_of(
            _arc_source, [n = nb_vertices](auto && v) { return v < n; }));
        assert(std::ranges::all_of(
            _arc_target, [n = nb_vertices](auto && v) { return v < n; }));
        assert(std::ranges::is_sorted(_arc_source));
        vertex_map<arc_t> in_arc_count(nb_vertices, 0);
        for(auto && s : _arc_source) ++_out_arc_begin[s];
        for(auto && t : _arc_target) ++in_arc_count[t];
        std::exclusive_scan(_out_arc_begin.begin(), _out_arc_begin.end(),
                            _out_arc_begin.begin(), 0);
        std::exclusive_scan(in_arc_count.begin(), in_arc_count.end(),
                            _in_arc_begin.begin(), 0);
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

    auto nb_vertices() const { return _out_arc_begin.size(); }
    auto nb_arcs() const { return _arc_target.size(); }

    bool is_valid_node(vertex_t u) const { return u < nb_vertices(); }
    bool is_valid_arc(arc_t u) const { return u < nb_arcs(); }

    auto vertices() const {
        return std::views::iota(static_cast<vertex_t>(0),
                                static_cast<vertex_t>(nb_vertices()));
    }
    auto arcs() const {
        return std::views::iota(static_cast<arc_t>(0),
                                static_cast<arc_t>(nb_arcs()));
    }

    auto out_arcs(const vertex_t u) const {
        assert(is_valid_node(u));
        return std::views::iota(
            _out_arc_begin[u],
            (u + 1 < nb_vertices() ? _out_arc_begin[u + 1] : nb_arcs()));
    }
    auto in_arcs(const vertex_t u) const {
        assert(is_valid_node(u));
        return std::ranges::subrange(
            _in_arcs.begin() + _in_arc_begin[u],
            (u + 1 < nb_vertices() ? _in_arcs.begin() + _in_arc_begin[u + 1]
                                   : _in_arcs.end()));
    }

    vertex_t source(arc_t a) const {
        assert(is_valid_arc(a));
        return _arc_source[a];
    }
    vertex_t target(arc_t a) const {
        assert(is_valid_arc(a));
        return _arc_target[a];
    }

    auto sources_map() const { return _arc_source; }
    auto targets_map() const { return _arc_target; }

    auto out_neighbors(const vertex_t u) const {
        assert(is_valid_node(u));
        return std::ranges::subrange(
            _arc_target.begin() + _out_arc_begin[u],
            (u + 1 < nb_vertices() ? _arc_target.begin() + _out_arc_begin[u + 1]
                                   : _arc_target.end()));
    }
    auto in_neighbors(const vertex_t u) const {
        assert(is_valid_node(u));
        return std::views::transform(in_arcs(u),
                                     [this](auto a) { return source(a); });
    }

    auto out_arcs_pairs(const vertex_t u) const {
        assert(is_valid_node(u));
        return std::views::transform(
            out_neighbors(u), [u](auto v) { return std::make_pair(u, v); });
    }
    auto arcs_pairs() const {
        return std::views::join(std::views::transform(
            vertices(), [this](auto u) { return out_arcs_pairs(u); }));
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STATIC_DIGRAPH_HPP