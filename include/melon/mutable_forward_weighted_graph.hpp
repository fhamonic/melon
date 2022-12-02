#ifndef MELON_MUTABLE_DIGRAPH_HPP
#define MELON_MUTABLE_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <list>
#include <ranges>
#include <vector>

namespace fhamonic {
namespace melon {

template <typename W>
class mutable_forward_weighted_digraph {
public:
    using vertex = unsigned int;
    using arc = std::vector<std::pair<vertex, W>>::iterator;

private:
    std::vector<std::vector<std::pair<vertex, W>>> _adjacency_list;

public:
    mutable_forward_weighted_digraph() = default;
    mutable_forward_weighted_digraph(
        const mutable_forward_weighted_digraph & graph) = default;
    mutable_forward_weighted_digraph(
        mutable_forward_weighted_digraph && graph) = default;

    auto nb_vertices() const { return _adjacency_list.size(); }
    bool is_valid_node(vertex u) const { return u < nb_vertices(); }
    auto vertices() const {
        return std::views::iota(static_cast<vertex>(0),
                                static_cast<vertex>(nb_vertices()));
    }
    auto out_arcs(const vertex u) const {
        assert(is_valid_node(u));
        return std::views::iota(_adjacency_list[u].begin(),
                                _adjacency_list[u].end());
    }
    auto arcs() const {
        return std::views::join(std::views::transform(
            vertices(), [this](auto u) { return out_arcs(u); }));
    }
    vertex target(const arc & a) const { return a->first; }
    auto targets_map() const {
        return map_view([](const arc & a) -> vertex { return a->first; });
    }
    vertex weight(const arc & a) const { return a->second; }
    auto weights_map() const {
        return map_view([](const arc & a) -> vertex { return a->second; });
    }
    const auto & out_neighbors(const vertex u) const {
        assert(is_valid_node(u));
        return std::views::transform(
            _adjacency_list[u],
            [](const auto & p) -> vertex { return p.first; });
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

    vertex create_vertex() noexcept {
        _adjacency_list.emplace_back();
        return static_cast<vertex>(_adjacency_list.size() - 1);
    }
    template <std::convertible_to<W> T>
    arc create_arc(const vertex from, const vertex to, T && weight) noexcept {
        _adjacency_list[from].emplace_back(to, std::forward<T>(weight));
        return _adjacency_list[from].end() - 1;
    }
    vertex remove_arc(const arc uv) noexcept {
        const vertex u = uv->first;
        if(_adjacency_list[u].size() > 1)
            std::iter_swap(uv, _adjacency_list[u].end() - 1);
        _adjacency_list[u].pop_back();
        return static_cast<vertex>(_adjacency_list.size() - 1);
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_MUTABLE_DIGRAPH_HPP