#ifndef STATIC_GRAPH_HPP
#define STATIC_GRAPH_HPP

#include <ranges>
#include <vector>

class StaticDigraph {
public:
    using Node = std::size_t;
    using Arc = std::size_t;

private:
    std::vector<Arc> out_arc_begin;
    std::vector<Node> arc_target;

public:
    StaticDigraph(std::vector<Arc> && begins, std::vector<Node> && targets)
        : out_arc_begin(std::move(begins)), arc_target(std::move(targets)) {}

    std::size_t nb_nodes() const { return out_arc_begin.size(); }
    std::size_t nb_arcs() const { return arc_target.size(); }

    auto nodes() const { return std::views::iota(nb_nodes); }
    auto arcs() const { return std::views::iota(nb_arcs); }

    auto arcs_pairs() {
        
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
};

class StaticDigraphBuilder {
public:
    using Node = StaticDigraph::Node;
    using Arc = StaticDigraph::Arc;

private:
    std::vector<std::pair<Node, Node>> arcs;

public:
    StaticDigraphBuilder() {}

    StaticDigraph build() const {
        std::ranges::sort(arcs, [](const auto & a, const auto & b) {
            return a.first < b.first;
        });

        const Node nb_nodes = std::ranges::max_element(std::views::transform(
            arcs,
            [](const auto a) { return std::ranges::max(a.fitst, a.second); })) + 1;
        std::vector<Arc> begins(nb_nodes);
        std::vector<Node> targets(arcs.size());

        std::transform(arcs.begin(), arcs.end(), targets.begin(),
                       [](const auto a) { return a.second; });


        Node tmp = it->first;
        std::size_t count = 0;
        for(int i=0; i < nb_nodes; ++i) {
            begins[i] = count;
        }

        auto it = arcs.begin();
        begins[it->first] = 0;
        for(; it != arcs.end(); ++it) {
            if(it->first == tmp) continue;
            begins[it->first] = std::distance(arcs.begin(), it);
            tmp = it->first;
        }

        for(Node u = 0; u <)

            StaticDigraph graph(std::move(begins), std::move(targets));
        return graph;
    }

    void add(Node u, Node v) { arcs.emplace_back(u, v); }
}

#endif  // STATIC_GRAPH_HPP
