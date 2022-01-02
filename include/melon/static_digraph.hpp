#ifndef STATIC_DIGRAPH_HPP
#define STATIC_DIGRAPH_HPP

#include <algorithm>
#include <ranges>
#include <vector>

namespace melon {

class StaticDigraph {
public:
    using Node = std::size_t;
    using Arc = std::size_t;

public:
    std::vector<Arc> out_arc_begin;
    std::vector<Node> arc_target;

public:
    StaticDigraph(std::vector<Arc> && begins, std::vector<Node> && targets)
        : out_arc_begin(std::move(begins)), arc_target(std::move(targets)) {}

    StaticDigraph(const StaticDigraph & graph) = default;
    StaticDigraph(StaticDigraph && graph) = default;

    std::size_t nb_nodes() const { return out_arc_begin.size(); }
    std::size_t nb_arcs() const { return arc_target.size(); }

    auto nodes() const {
        return std::views::iota(static_cast<std::size_t>(0), nb_nodes());
    }
    auto arcs() const {
        return std::views::iota(static_cast<std::size_t>(0), nb_arcs());
    }
    auto out_arcs(const Node u) const {
        return std::views::iota(
            out_arc_begin[u],
            (u + 1 < nb_nodes() ? out_arc_begin[u + 1] : nb_arcs()));
    }
    Node target(Arc a) const { return arc_target[a]; }
    auto out_neighbors(const Node u) const {
        return std::ranges::subrange(
            arc_target.begin() + out_arc_begin[u],
            (u + 1 < nb_nodes() ? arc_target.begin() + out_arc_begin[u + 1]
                                : arc_target.end()));
    }

    auto out_arcs_pairs(const Node u) const {
        return std::views::transform(
            out_neighbors(u), [u](auto v) { return std::make_pair(u, v); });
    }
    auto arcs_pairs() const {
        return std::views::join(std::views::transform(
            nodes(), [&](auto u) { return out_arcs_pairs(u); }));
    }
};

}  // namespace melon

#endif  // STATIC_DIGRAPH_HPP