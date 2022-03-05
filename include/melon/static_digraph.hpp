#ifndef MELON_STATIC_DIGRAPH_HPP
#define MELON_STATIC_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

#include "melon/static_map.hpp"

namespace fhamonic {
namespace melon {

class static_digraph {
public:
    using vertex = unsigned int;
    using arc = unsigned int;

    template <typename T>
    using vertex_map = static_map<T>;
    template <typename T>
    using arc_map = static_map<T>;

private:
    vertex_map<arc> _out_arc_begin;
    arc_map<vertex> _arc_target;

public:
    static_digraph(std::vector<arc> && begins, std::vector<vertex> && targets)
        : _out_arc_begin(std::move(begins)), _arc_target(std::move(targets)) {}

    static_digraph() = default;
    static_digraph(const static_digraph & graph) = default;
    static_digraph(static_digraph && graph) = default;

    static_digraph & operator=(const static_digraph &) = default;
    static_digraph & operator=(static_digraph &&) = default;

    auto nb_vertices() const { return _out_arc_begin.size(); }
    auto nb_arcs() const { return _arc_target.size(); }

    bool is_valid_node(vertex u) const { return u < nb_vertices(); }
    bool is_valid_arc(arc u) const { return u < nb_arcs(); }

    auto vertices() const {
        return std::views::iota(static_cast<vertex>(0),
                                static_cast<vertex>(nb_vertices()));
    }
    auto arcs() const {
        return std::views::iota(static_cast<arc>(0),
                                static_cast<arc>(nb_arcs()));
    }
    auto out_arcs(const vertex u) const {
        assert(is_valid_node(u));
        return std::views::iota(
            _out_arc_begin[u],
            (u + 1 < nb_vertices() ? _out_arc_begin[u + 1] : nb_arcs()));
    }
    vertex source(arc a) const {  // O(\log |V|)
        assert(is_valid_arc(a));
        auto it =
            std::ranges::lower_bound(_out_arc_begin, a, std::less_equal<arc>());
        return static_cast<vertex>(std::distance(_out_arc_begin.begin(), --it));
    }
    vertex target(arc a) const {
        assert(is_valid_arc(a));
        return _arc_target[a];
    }
    auto out_neighbors(const vertex u) const {
        assert(is_valid_node(u));
        return std::ranges::subrange(
            _arc_target.begin() + _out_arc_begin[u],
            (u + 1 < nb_vertices() ? _arc_target.begin() + _out_arc_begin[u + 1]
                                : _arc_target.end()));
    }

    auto out_arcs_pairs(const vertex u) const {
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