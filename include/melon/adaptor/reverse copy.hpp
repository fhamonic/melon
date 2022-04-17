#ifndef MELON_ADAPTOR_REVERSE_HPP
#define MELON_ADAPTOR_REVERSE_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

#include "melon/data_structures/static_map.hpp"

namespace fhamonic {
namespace melon {

template <typename G>
class reverse {
public:
    using vertex_t = typename G::vertex_t;
    using arc_t = typename G::arc_t;

    template <typename T>
    using vertex_map = static_map<T>;
    template <typename T>
    using arc_map = static_map<T>;

private:
    const G & _graph;
    vertex_map<std::size_t> _in_arc_begin;
    std::vector<arc_t> _in_arcs;
    std::vector<vertex_t> _arc_source;

public:
    reverse(G && g)
        : _graph(std::forward(g)) {}

    reverse() = default;
    reverse(const reverse & graph) = default;
    reverse(reverse && graph) = default;

    reverse & operator=(const reverse &) = default;
    reverse & operator=(reverse &&) = default;

    auto nb_vertices() const { return _out_arc_begin.size(); }
    auto nb_arcs() const { return _arc_target.size(); }


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
    vertex_t source(arc_t a) const {  // O(\log |V|)
        assert(is_valid_arc(a));
        auto it =
            std::ranges::lower_bound(_out_arc_begin, a, std::less_equal<arc_t>());
        return static_cast<vertex_t>(std::distance(_out_arc_begin.begin(), --it));
    }
    vertex_t target(arc_t a) const {
        assert(is_valid_arc(a));
        return _arc_target[a];
    }
    auto out_neighbors(const vertex_t u) const {
        assert(is_valid_node(u));
        return std::ranges::subrange(
            _arc_target.begin() + _out_arc_begin[u],
            (u + 1 < nb_vertices() ? _arc_target.begin() + _out_arc_begin[u + 1]
                                : _arc_target.end()));
    }

    auto out_arcs_pairs(const vertex_t u) const {
    }
    auto arcs_pairs() const {
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ADAPTOR_REVERSE_HPP