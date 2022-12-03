#ifndef MELON_MUTABLE_WEIGHTED_DIGRAPH_HPP
#define MELON_MUTABLE_WEIGHTED_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

#include "melon/data_structures/static_map.hpp"
#include "melon/utils/intrusive_view.hpp"
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
        int prev_in_arc;
        int next_in_arc;
        int prev_out_arc;
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
    const auto & arcs() const { return _arcs; }
    auto arcs_pairs() const {
        return std::views::transform(_arcs, [](const auto & a) {
            return std::make_pair(a.source, a.target);
        });
    }
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
    auto out_arcs(const vertex u) const {
        assert(is_valid_node(u));
        return intrusive_view(
            _first_out_arc[u],
            [this](const int arc_index) -> const arc & {
                return _arcs[static_cast<std::size_t>(arc_index)];
            },
            [this](const int arc_index) -> int {
                return _arcs[static_cast<std::size_t>(arc_index)].next_out_arc;
            },
            [](const int arc_index) -> bool { return arc_index != -1; });
    }
    auto in_arcs(const vertex u) const {
        assert(is_valid_node(u));
        return intrusive_view(
            _first_in_arc[u],
            [this](const int arc_index) -> const arc & {
                return _arcs[static_cast<std::size_t>(arc_index)];
            },
            [this](const int arc_index) -> int {
                return _arcs[static_cast<std::size_t>(arc_index)].next_in_arc;
            },
            [](const int arc_index) -> bool { return arc_index != -1; });
    }
    auto out_neighbors(const vertex u) const {
        assert(is_valid_node(u));
        return std::views::transform(
            out_arcs(u), [this](const arc & a) { return a.target; });
    }
    auto in_neighbors(const vertex u) const {
        assert(is_valid_node(u));
        return std::views::transform(
            in_arcs(u), [this](const arc & a) { return a.source; });
    }

    vertex create_vertex() noexcept {
        _first_in_arc.emplace_back(-1);
        _first_out_arc.emplace_back(-1);
        return static_cast<vertex>(_first_out_arc.size() - 1);
    }

private:
    int index_of_arc(const arc & a) const noexcept {
        if(a.prev_out_arc != -1) return _arcs[a.prev_out_arc].next_out_arc;
        return _first_out_arc[a.source];
    }
    void remove_from_source_out_arcs(const arc & a) noexcept {
        if(a.prev_out_arc == -1)
            _first_out_arc[a.source] = a.next_out_arc;
        else
            _arcs[a.prev_out_arc].next_out_arc = a.next_out_arc;
        if(a.next_out_arc != -1)
            _arcs[a.next_out_arc].prev_out_arc = a.prev_out_arc;
    }
    void remove_from_target_in_arcs(const arc & a) noexcept {
        if(a.prev_in_arc == -1)
            _first_in_arc[a.target] = a.next_in_arc;
        else
            _arcs[a.prev_in_arc].next_in_arc = a.next_in_arc;
        if(a.next_in_arc != -1)
            _arcs[a.next_in_arc].prev_in_arc = a.prev_in_arc;
    }
    void change_arc_index(arc & a, const int i) noexcept {
        if(a.prev_out_arc == -1)
            _first_out_arc[a.source] = i;
        else
            _arcs[a.prev_out_arc].next_out_arc = i;
        if(a.next_out_arc != -1) _arcs[a.next_out_arc].prev_out_arc = i;
        if(a.prev_in_arc == -1)
            _first_in_arc[a.target] = i;
        else
            _arcs[a.prev_in_arc].next_in_arc = i;
        if(a.next_in_arc != -1) _arcs[a.next_in_arc].prev_in_arc = i;
    }

public:
    template <std::convertible_to<W> T>
    arc create_arc(const vertex from, const vertex to, T && weight) noexcept {
        const int n = static_cast<int>(_arcs.size());
        _arcs.emplace_back(from, to, -1, -1, _first_in_arc[to],
                           _first_out_arc[from], weight);
        if(_first_in_arc[to] != -1)
            _arcs[static_cast<std::size_t>(_first_in_arc[to])].prev_in_arc = n;
        _first_in_arc[to] = n;
        if(_first_out_arc[from] != -1)
            _arcs[static_cast<std::size_t>(_first_out_arc[from])].prev_in_arc =
                n;
        _first_out_arc[from] = n;
        return _arcs.back();
    }
    void remove_arc(arc & a) noexcept {
        const int i = index_of_arc(a);
        remove_from_source_out_arcs(a);
        remove_from_target_in_arcs(a);
        std::swap(_arcs[static_cast<std::size_t>(i)], _arcs.back());
        _arcs.pop_back();
        change_arc_index(_arcs[i], i);
    }
    void charge_target(arc & a, const vertex v) noexcept {
        if(a.target == v) return;
        const int i = index_of_arc(a);
        remove_from_target_in_arcs(a);
        a.target = v;
        a.prev_in_arc = -1;
        a.next_in_arc = _first_in_arc[v];
        if(_first_in_arc[v] != -1)
            _arcs[static_cast<std::size_t>(_first_in_arc[v])].prev_in_arc = i;
        _first_in_arc[v] = i;
    }
    void charge_source(arc & a, const vertex v) noexcept {
        if(a.source == v) return;
        const int i = index_of_arc(a);
        remove_from_source_out_arcs(a);
        a.source = v;
        a.prev_out_arc = -1;
        a.next_out_arc = _first_out_arc[v];
        if(_first_out_arc[v] != -1)
            _arcs[static_cast<std::size_t>(_first_out_arc[v])].prev_out_arc = i;
        _first_out_arc[v] = i;
    }

    template <typename T>
    static_map<vertex, T> create_vertex_map() const noexcept {
        return static_map<vertex, T>(nb_vertices());
    }
    template <typename T>
    static_map<vertex, T> create_vertex_map(
        const T & default_value) const noexcept {
        return static_map<vertex, T>(nb_vertices(), default_value);
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_MUTABLE_WEIGHTED_DIGRAPH_HPP