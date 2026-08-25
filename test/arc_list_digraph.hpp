#ifndef ARC_LIST_DIGRAPH_HPP
#define ARC_LIST_DIGRAPH_HPP

#include <ranges>
#include <utility>
#include <vector>

#include "melon/container/static_map.hpp"
#include "melon/graph.hpp"

// vertices + arcs + arcs_entries + vertex maps, nothing else: no out_arcs, no
// arc_target. It pins which algorithms genuinely need incidence lists and
// which run on a bare arc list -- bellman_ford accepts it, bellman_ford_moore
// must reject it.
struct arc_list_digraph {
    unsigned int n;
    std::vector<std::pair<unsigned int, unsigned int>> ends;

    auto vertices() const { return std::views::iota(0u, n); }
    auto arcs() const {
        return std::views::iota(0u, static_cast<unsigned int>(ends.size()));
    }
    auto arcs_entries() const {
        return arcs() | std::views::transform([this](unsigned int a) {
                   return std::make_pair(a, ends[a]);
               });
    }
    template <typename T>
    auto create_vertex_map() const {
        return melon::static_map<unsigned int, T>(n);
    }
    template <typename T>
    auto create_vertex_map(const T & d) const {
        return melon::static_map<unsigned int, T>(n, d);
    }
};

#endif  // ARC_LIST_DIGRAPH_HPP
