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

public:
    std::vector<Arc> out_arc_begin;
    std::vector<Node> arc_target;

public:
    // todo bulk constructor : rvalue ranges::random_access_range
    StaticDigraph(std::vector<Arc> && begins, std::vector<Node> && targets)
        : out_arc_begin(std::move(begins)), arc_target(std::move(targets)) {}

    std::size_t nb_nodes() const { return out_arc_begin.size(); }
    std::size_t nb_arcs() const { return arc_target.size(); }

    auto nodes() const {
        return std::views::iota(static_cast<std::size_t>(0), nb_nodes());
    }
    auto arcs() const {
        return std::views::iota(static_cast<std::size_t>(0), nb_arcs());
    }

    auto out_arcs(Node u) const {
        return std::views::iota(
            out_arc_begin[u],
            (u + 1 < nb_nodes() ? out_arc_begin[u + 1] : nb_arcs()));
    }
    auto out_neighbors(Node u) const {
        return std::ranges::subrange(
            arc_target.begin() + out_arc_begin[u],
            (u + 1 < nb_nodes() ? arc_target.begin() + out_arc_begin[u + 1]
                                : arc_target.end()));
    }
    auto out_arcs_pairs(Node u) const {
        return std::views::transform(
            out_neighbors(u), [u](auto v) { return std::make_pair(u, v); });
    }
    auto arcs_pairs() const {
        return std::views::join(std::views::transform(
            nodes(), [&](auto u) { return out_arcs_pairs(u); }));
    }
};

class StaticDigraphBuilder {
public:
    using Node = StaticDigraph::Node;
    using Arc = StaticDigraph::Arc;

private:
    std::size_t nb_nodes;
    std::vector<std::pair<Node, Node>> arcs;

public:
    StaticDigraphBuilder() {}

    StaticDigraph build() {
        std::ranges::sort(arcs, [](const auto & a, const auto & b) {
            if(a.first == b.first) return a.second < b.second;
            return a.first < b.first;
        });

        std::vector<Arc> begins(nb_nodes);
        std::vector<Node> targets(arcs.size());

        // fill begins
        std::size_t u = 0;
        begins[0] = 0;
        for(auto it = arcs.begin(); it != arcs.end(); ++it) {
            if(u == it->first) continue;
            std::fill(begins.begin() + u + 1, begins.begin() + it->first + 1,
                      std::distance(arcs.begin(), it));
            u = it->first;
        }
        std::fill(begins.begin() + u + 1, begins.end(), arcs.size());
        // fill targets
        std::ranges::copy(std::views::values(arcs), targets.begin());

        StaticDigraph graph(std::move(begins), std::move(targets));
        return graph;
    }

    void setNbNodes(std::size_t n) { nb_nodes = n; }
    void add(Node u, Node v) { arcs.emplace_back(u, v); }
};

}  // namespace lemon

#endif  // STATIC_GRAPH_HPP
