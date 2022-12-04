#ifndef MELON_MUTABLE_WEIGHTED_DIGRAPH_HPP
#define MELON_MUTABLE_WEIGHTED_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <limits>
#include <ranges>
#include <vector>

#include "melon/data_structures/static_map.hpp"
#include "melon/utils/intrusive_view.hpp"
#include "melon/utils/map_view.hpp"

namespace fhamonic {
namespace melon {

template <typename VW, typename AW>
class mutable_weighted_digraph {
public:
    using vertex = unsigned int;
    using arc = unsigned int;

private:
    static constexpr arc INVALID_ARC = std::numeric_limits<arc>::max();
    struct vertex_struct {
        arc first_in_arc;
        arc first_out_arc;
        VW weight;
    };
    struct arc_struct {
        vertex source;
        vertex target;
        arc prev_in_arc;
        arc next_in_arc;
        arc prev_out_arc;
        arc next_out_arc;
        AW weight;
    };
    std::vector<vertex_struct> _vertices;
    std::vector<arc_struct> _arcs;

public:
    mutable_weighted_digraph() = default;
    mutable_weighted_digraph(const mutable_weighted_digraph & graph) = default;
    mutable_weighted_digraph(mutable_weighted_digraph && graph) = default;

    mutable_weighted_digraph & operator=(const mutable_weighted_digraph &) =
        default;
    mutable_weighted_digraph & operator=(mutable_weighted_digraph &&) = default;

    auto nb_vertices() const noexcept { return _vertices.size(); }
    auto nb_arcs() const noexcept { return _arcs.size(); }
    bool is_valid_node(vertex u) const noexcept { return u < nb_vertices(); }
    auto vertices() const noexcept {
        return std::views::iota(static_cast<vertex>(0),
                                static_cast<vertex>(nb_vertices()));
    }
    auto arcs() const noexcept {
        return std::views::iota(static_cast<arc>(0),
                                static_cast<arc>(nb_arcs()));
    }
    auto arcs_pairs() const noexcept {
        return std::views::transform(_arcs, [](const arc_struct & as) {
            return std::make_pair(as.source, as.target);
        });
    }
    vertex source(const arc a) const noexcept { return _arcs[a].source; }
    auto sources_map() const noexcept {
        return map_view(
            [this](const arc a) -> vertex { return _arcs[a].source; });
    }
    vertex target(const arc a) const noexcept { return _arcs[a].target; }
    auto targets_map() const noexcept {
        return map_view(
            [this](const arc a) -> vertex { return _arcs[a].target; });
    }
    VW vertex_weight(const vertex v) const noexcept {
        return _vertices[v].weight;
    }
    auto vertices_weights_map() const noexcept {
        return map_view(
            [this](const vertex v) -> VW { return _vertices[v].weight; });
    }
    AW arc_weight(const arc a) const noexcept { return _arcs[a].weight; }
    auto arcs_weights_map() const noexcept {
        return map_view([this](const arc a) -> AW { return _arcs[a].weight; });
    }
    auto out_arcs(const vertex u) const noexcept {
        assert(is_valid_node(u));
        return intrusive_view(
            _vertices[u].first_out_arc, std::identity(),
            [this](const arc a) -> arc { return _arcs[a].next_out_arc; },
            [](const arc a) -> bool { return a != INVALID_ARC; });
    }
    auto in_arcs(const vertex u) const noexcept {
        assert(is_valid_node(u));
        return intrusive_view(
            _vertices[u].first_in_arc, std::identity(),
            [this](const arc a) -> arc { return _arcs[a].next_in_arc; },
            [](const arc a) -> bool { return a != INVALID_ARC; });
    }
    auto out_neighbors(const vertex u) const noexcept {
        assert(is_valid_node(u));
        return std::views::transform(
            out_arcs(u),
            [this](const arc & a) -> vertex { return _arcs[a].target; });
    }
    auto in_neighbors(const vertex u) const noexcept {
        assert(is_valid_node(u));
        return std::views::transform(
            in_arcs(u),
            [this](const arc & a) -> vertex { return _arcs[a].source; });
    }

    template <std::convertible_to<VW> T>
    vertex create_vertex(T && weight) noexcept {
        _vertices.emplace_back(INVALID_ARC, INVALID_ARC,
                               std::forward<T>(weight));
        return static_cast<vertex>(_vertices.size() - 1);
    }

private:
    void remove_from_source_out_arcs(const arc a) noexcept {
        const arc_struct & as = _arcs[a];
        if(as.prev_out_arc == INVALID_ARC)
            _vertices[as.source].first_out_arc = as.next_out_arc;
        else
            _arcs[as.prev_out_arc].next_out_arc = as.next_out_arc;
        if(as.next_out_arc != INVALID_ARC)
            _arcs[as.next_out_arc].prev_out_arc = as.prev_out_arc;
    }
    void remove_from_target_in_arcs(const arc a) noexcept {
        const arc_struct & as = _arcs[a];
        if(as.prev_in_arc == INVALID_ARC)
            _vertices[as.target].first_in_arc = as.next_in_arc;
        else
            _arcs[as.prev_in_arc].next_in_arc = as.next_in_arc;
        if(as.next_in_arc != INVALID_ARC)
            _arcs[as.next_in_arc].prev_in_arc = as.prev_in_arc;
    }
    void notify_new_arc_index(const arc a) noexcept {
        const arc_struct & as = _arcs[a];
        if(as.prev_out_arc == INVALID_ARC)
            _vertices[as.source].first_out_arc = a;
        else
            _arcs[as.prev_out_arc].next_out_arc = a;
        if(as.next_out_arc != INVALID_ARC)
            _arcs[as.next_out_arc].prev_out_arc = a;
        if(as.prev_in_arc == INVALID_ARC)
            _vertices[as.target].first_in_arc = a;
        else
            _arcs[as.prev_in_arc].next_in_arc = a;
        if(as.next_in_arc != INVALID_ARC) _arcs[as.next_in_arc].prev_in_arc = a;
    }

public:
    template <std::convertible_to<AW> T>
    arc create_arc(const vertex from, const vertex to, T && weight) noexcept {
        const arc a = static_cast<arc>(_arcs.size());
        _arcs.emplace_back(
            from, to, INVALID_ARC, INVALID_ARC, _vertices[to].first_in_arc,
            _vertices[from].first_out_arc, std::forward<T>(weight));
        if(_vertices[to].first_in_arc != INVALID_ARC)
            _arcs[_vertices[to].first_in_arc].prev_in_arc = a;
        _vertices[to].first_in_arc = a;
        if(_vertices[from].first_out_arc != INVALID_ARC)
            _arcs[_vertices[from].first_out_arc].prev_in_arc = a;
        _vertices[from].first_out_arc = a;
        return a;
    }
    void remove_arc(const arc a) noexcept {
        remove_from_source_out_arcs(a);
        remove_from_target_in_arcs(a);
        std::swap(_arcs[a], _arcs.back());
        _arcs.pop_back();
        notify_new_arc_index(a);
    }
    void change_target(const arc a, const vertex v) noexcept {
        const arc_struct & as = _arcs[a];
        if(as.target == v) return;
        remove_from_target_in_arcs(a);
        as.target = v;
        as.prev_in_arc = INVALID_ARC;
        as.next_in_arc = _vertices[v].first_in_arc;
        if(_vertices[v].first_in_arc != INVALID_ARC)
            _arcs[_vertices[v].first_in_arc].prev_in_arc = a;
        _vertices[v].first_in_arc = a;
    }
    void change_source(const arc a, const vertex u) noexcept {
        const arc_struct & as = _arcs[a];
        if(as.source == u) return;
        remove_from_source_out_arcs(a);
        as.source = u;
        as.prev_out_arc = INVALID_ARC;
        as.next_out_arc = _vertices[u].first_out_arc;
        if(_vertices[u].first_out_arc != INVALID_ARC)
            _arcs[_vertices[u].first_out_arc].prev_out_arc = a;
        _vertices[u].first_out_arc = a;
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