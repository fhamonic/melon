#ifndef MELON_MUTABLE_DIGRAPH_HPP
#define MELON_MUTABLE_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

namespace fhamonic {
namespace melon {

class mutable_digraph {
public:
    using vertex = unsigned int;
    using arc = std::vector<vertex_t>::iterator;

    template <typename T>
    using vertex_map = std::vector<T>;
    template <typename T>
    using arc_map = std::vector<T>;

private:
    vertex_map<std::vector<vertex_t>> _adjacency_list;

public:
    mutable_digraph() = default;
    mutable_digraph(const mutable_digraph & graph) = default;
    mutable_digraph(mutable_digraph && graph) = default;

    auto nb_vertices() const { return _adjacency_list.size(); }
    bool is_valid_node(vertex u) const { return u < nb_vertices(); }
    auto vertices() const {
        return std::views::iota(static_cast<vertex_t>(0),
                                static_cast<vertex_t>(nb_vertices()));
    }

private:
    auto out_arcs(const vertex u) const {
        assert(is_valid_node(u));
        return std::views::iota(_adjacency_list[u].begin(),
                                _adjacency_list[u].end());
    }

public:
    auto arcs() const {
        return std::views::join(std::views::transform(
            vertices(), [this](auto u) { return out_arcs(u); }));
    }
    vertex_t target(arc_t a) const { return *a; }
    const auto & out_neighbors(const vertex u) const {
        assert(is_valid_node(u));
        return _adjacency_list[u];
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

#endif  // MELON_MUTABLE_DIGRAPH_HPP