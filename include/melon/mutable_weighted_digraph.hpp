#ifndef MELON_MUTABLE_WEIGHTED_DIGRAPH_HPP
#define MELON_MUTABLE_WEIGHTED_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

#include "melon/data_structures/static_map.hpp"
#include "melon/utils/intrusive_input_range.hpp"
#include "melon/utils/map_view.hpp"

namespace fhamonic {
namespace melon {

template <typename W>
class mutable_weighted_digraph {
public:
    using vertex = unsigned int;
    struct arc {
        vertex source;
        vertex target;
        int next_in_arc;
        int next_out_arc;
        W weight;
    };

private:
    std::vector<int> _first_in_arc;
    std::vector<int> _first_out_arc;
    std::vector<arc> _arcs;

public:
    mutable_weighted_digraph() = default;
    mutable_weighted_digraph(const mutable_weighted_digraph & graph) = default;
    mutable_weighted_digraph(mutable_weighted_digraph && graph) = default;

    mutable_weighted_digraph & operator=(const mutable_weighted_digraph &) =
        default;
    mutable_weighted_digraph & operator=(mutable_weighted_digraph &&) = default;

    auto nb_vertices() const { return _first_out_arc.size(); }
    bool is_valid_node(vertex u) const { return u < nb_vertices(); }
    auto vertices() const {
        return std::views::iota(static_cast<vertex>(0),
                                static_cast<vertex>(nb_vertices()));
    }
    auto out_arcs(const vertex u) {
        assert(is_valid_node(u));
        return intrusive_input_range(
            _first_out_arc[u],
            [this](const int arc_index) -> const arc & {
                return _arcs[static_cast<std::size_t>(arc_index)];
            },
            [this](const int arc_index) -> int {
                return _arcs[static_cast<std::size_t>(arc_index)].next_out_arc;
            },
            [](const int arc_index) -> bool {
                return arc_index != -1;
            });
    }
    const auto & arcs() const { return _arcs; }
    vertex source(const arc & a) const { return a.source; }
    auto sources_map() const {
        return map_view([](const arc & a) -> vertex { return a.source; });
    }
    vertex target(const arc & a) const { return a.target; }
    auto targets_map() const {
        return map_view([](const arc & a) -> vertex { return a.target; });
    }
    W weight(const arc & a) const { return a.weight; }
    auto weights_map() const {
        return map_view([](const arc & a) -> vertex { return a.weight; });
    }

    // const auto & out_neighbors(const vertex u) const {
    //     assert(is_valid_node(u));
    //     return std::views::transform(
    //         _adjacency_list[u],
    //         [](const auto & p) -> vertex { return p.first; });
    // }
    // auto out_arcs_pairs(const vertex u) const {
    //     assert(is_valid_node(u));
    //     return std::views::transform(
    //         out_neighbors(u), [u](auto v) { return std::make_pair(u, v);
    //         });
    // }
    auto arcs_pairs() const {
        return std::views::transform(_arcs, [](const auto & a) {
            return std::make_pair(a.source, a.target);
        });
    }

    vertex create_vertex() noexcept {
        _first_in_arc.emplace_back(-1);
        _first_out_arc.emplace_back(-1);
        return static_cast<vertex>(_first_out_arc.size() - 1);
    }
    template <std::convertible_to<W> T>
    arc create_arc(const vertex from, const vertex to, T && weight) noexcept {
        const int n = static_cast<int>(_arcs.size());
        _arcs.emplace_back(from, to, _first_in_arc[to], _first_out_arc[from],
                           weight);
        _first_in_arc[to] = n;
        _first_out_arc[from] = n;
        return _arcs.back();
    }
    // void remove_arc(const arc & uv) noexcept {
    //     const vertex u = uv->first;
    //     if(_adjacency_list[u].size() > 1)
    //         std::swap(*uv, _adjacency_list[u].back());
    //     _adjacency_list[u].pop_back();
    // }
    // void charge_target(arc & a, const vertex v) noexcept { a->first = v;
    // }

    // template <typename T>
    // static_map<vertex, T> create_vertex_map() const noexcept {
    //     return static_map<vertex, T>(nb_vertices());
    // }
    // template <typename T>
    // static_map<vertex, T> create_vertex_map(
    //     const T & default_value) const noexcept {
    //     return static_map<vertex, T>(nb_vertices(), default_value);
    // }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_MUTABLE_WEIGHTED_DIGRAPH_HPP