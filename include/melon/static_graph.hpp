#ifndef STATIC_GRAPH_HPP
#define STATIC_GRAPH_HPP

#include <algorithm>
#include <ranges>
#include <vector>

namespace melon {

class StaticDigraph {
public:
    using Node = std::size_t;
    using Arc = std::size_t;
    using ArcList = std::vector<std::pair<Node, Node>>;

public:
    std::vector<Arc> out_arc_begin;
    std::vector<Node> arc_target;

public:
    template <std::ranges::random_access_range Arcs>
    requires(std::is_rvalue_reference<Arcs &&>::value)
        StaticDigraph(const std::size_t nb_nodes, Arcs && arcs)
        : out_arc_begin(nb_nodes), arc_target(arcs.size()) {
        // sort arcs
        std::ranges::sort(arcs, [](const auto & a, const auto & b) {
            if(a.first == b.first) return a.second < b.second;
            return a.first < b.first;
        });
        // fill out_arc_begin
        std::size_t u = 0;
        out_arc_begin[0] = 0;
        for(auto it = arcs.begin(); it != arcs.end(); ++it) {
            if(u == it->first) continue;
            std::fill(out_arc_begin.begin() + u + 1,
                      out_arc_begin.begin() + it->first + 1,
                      std::distance(arcs.begin(), it));
            u = it->first;
        }
        std::fill(out_arc_begin.begin() + u + 1, out_arc_begin.end(),
                  arcs.size());
        // fill arc_target
        std::ranges::copy(std::views::values(arcs), arc_target.begin());
    }
    template <std::ranges::range Arcs>
    StaticDigraph(const std::size_t nb_nodes, Arcs && arcs)
        : StaticDigraph(nb_nodes, ArcList(arcs)) {}

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

#endif  // STATIC_GRAPH_HPP
