#ifndef MELON_MUTABLE_DIGRAPH_HPP
#define MELON_MUTABLE_DIGRAPH_HPP

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

class mutable_digraph {
public:
    using vertex = unsigned int;
    using arc = unsigned int;

private:
    static constexpr vertex INVALID_VERTEX = std::numeric_limits<vertex>::max();
    static constexpr arc INVALID_ARC = std::numeric_limits<arc>::max();
    struct vertex_struct {
        arc first_in_arc;
        arc first_out_arc;
        vertex prev_vertex;
        vertex next_vertex;
    };
    struct arc_struct {
        vertex source;
        vertex target;
        arc prev_in_arc;
        arc next_in_arc;
        arc prev_out_arc;
        arc next_out_arc;
    };
    std::vector<vertex_struct> _vertices;
    std::vector<arc_struct> _arcs;
    std::vector<bool> _vertices_filter;
    std::vector<bool> _arcs_filter;
    vertex _first_vertex;
    vertex _first_free_vertex;
    vertex _first_free_arc;

public:
    mutable_digraph() noexcept
        : _first_vertex(INVALID_VERTEX)
        , _first_free_vertex(INVALID_VERTEX)
        , _first_free_arc(INVALID_ARC){};
    mutable_digraph(const mutable_digraph & graph) = default;
    mutable_digraph(mutable_digraph && graph) = default;

    mutable_digraph & operator=(const mutable_digraph &) = default;
    mutable_digraph & operator=(mutable_digraph &&) = default;

    bool is_valid_vertex(const vertex a) const noexcept {
        if(a >= _vertices.size()) return false;
        return _vertices_filter[a];
    }
    bool is_valid_arc(const arc a) const noexcept {
        if(a >= _arcs.size()) return false;
        return _arcs_filter[a];
    }

    auto vertices() const noexcept {
        return intrusive_view(
            _first_vertex, std::identity(),
            [this](const vertex v) -> vertex {
                return _vertices[v].next_vertex;
            },
            [](const vertex a) -> bool { return a != INVALID_VERTEX; });
    }
    vertex source(const arc a) const noexcept {
        assert(is_valid_arc(a));
        return _arcs[a].source;
    }
    auto sources_map() const noexcept {
        return map_view(
            [this](const arc a) -> vertex { return _arcs[a].source; });
    }
    vertex target(const arc a) const noexcept {
        assert(is_valid_arc(a));
        return _arcs[a].target;
    }
    auto targets_map() const noexcept {
        return map_view(
            [this](const arc a) -> vertex { return _arcs[a].target; });
    }
    auto out_arcs(const vertex v) const noexcept {
        assert(is_valid_vertex(v));
        return intrusive_view(
            _vertices[v].first_out_arc, std::identity(),
            [this](const arc a) -> arc { return _arcs[a].next_out_arc; },
            [](const arc a) -> bool { return a != INVALID_ARC; });
    }
    auto in_arcs(const vertex v) const noexcept {
        assert(is_valid_vertex(v));
        return intrusive_view(
            _vertices[v].first_in_arc, std::identity(),
            [this](const arc a) -> arc { return _arcs[a].next_in_arc; },
            [](const arc a) -> bool { return a != INVALID_ARC; });
    }
    auto out_neighbors(const vertex v) const noexcept {
        assert(is_valid_vertex(v));
        return std::views::transform(
            out_arcs(v),
            [this](const arc & a) -> vertex { return _arcs[a].target; });
    }
    auto in_neighbors(const vertex v) const noexcept {
        assert(is_valid_vertex(v));
        return std::views::transform(
            in_arcs(v),
            [this](const arc & a) -> vertex { return _arcs[a].source; });
    }

    auto arcs() const noexcept {
        return std::views::join(std::views::transform(
            vertices(), [this](auto v) { return out_arcs(v); }));
    }
    auto arcs_pairs() const noexcept {
        return std::views::transform(arcs(), [this](const arc & a) {
            return std::make_pair(_arcs[a].source, _arcs[a].target);
        });
    }

    vertex create_vertex() noexcept {
        vertex new_vertex;
        if(_first_free_vertex == INVALID_VERTEX) {
            new_vertex = static_cast<vertex>(_vertices.size());
            _vertices.emplace_back(INVALID_ARC, INVALID_ARC, INVALID_VERTEX,
                                   _first_vertex);
            _vertices_filter.emplace_back(true);
        } else {
            new_vertex = _first_free_vertex;
            _first_free_vertex = _vertices[_first_free_vertex].next_vertex;
            _vertices[new_vertex] = {INVALID_ARC, INVALID_ARC, INVALID_VERTEX,
                                     _first_vertex};
            _vertices_filter[new_vertex] = true;
        }
        if(_first_vertex != INVALID_VERTEX) {
            _vertices[_first_vertex].prev_vertex = new_vertex;
        }
        _first_vertex = new_vertex;
        return new_vertex;
    }

    arc create_arc(const vertex from, const vertex to) noexcept {
        arc new_arc;
        vertex_struct & tos = _vertices[to];
        vertex_struct & froms = _vertices[from];
        if(_first_free_arc == INVALID_ARC) {
            new_arc = static_cast<arc>(_arcs.size());
            _arcs.emplace_back(from, to, INVALID_ARC, tos.first_in_arc,
                               INVALID_ARC, froms.first_out_arc);
            _arcs_filter.emplace_back(true);
        } else {
            new_arc = _first_free_arc;
            _first_free_arc = _arcs[_first_free_arc].next_in_arc;
            _arcs[new_arc] = {from,        to,
                              INVALID_ARC, tos.first_in_arc,
                              INVALID_ARC, froms.first_out_arc};
            _arcs_filter[new_arc] = true;
        }
        if(tos.first_in_arc != INVALID_ARC)
            _arcs[tos.first_in_arc].prev_in_arc = new_arc;
        tos.first_in_arc = new_arc;
        if(froms.first_out_arc != INVALID_ARC)
            _arcs[froms.first_out_arc].prev_out_arc = new_arc;
        froms.first_out_arc = new_arc;
        return new_arc;
    }

private:
    void remove_from_source_out_arcs(const arc a) noexcept {
        assert(is_valid_arc(a));
        const arc_struct & as = _arcs[a];
        if(as.next_out_arc != INVALID_ARC)
            _arcs[as.next_out_arc].prev_out_arc = as.prev_out_arc;
        if(as.prev_out_arc != INVALID_ARC)
            _arcs[as.prev_out_arc].next_out_arc = as.next_out_arc;
        else
            _vertices[as.source].first_out_arc = as.next_out_arc;
    }
    void remove_from_target_in_arcs(const arc a) noexcept {
        assert(is_valid_arc(a));
        const arc_struct & as = _arcs[a];
        if(as.next_in_arc != INVALID_ARC)
            _arcs[as.next_in_arc].prev_in_arc = as.prev_in_arc;
        if(as.prev_in_arc != INVALID_ARC)
            _arcs[as.prev_in_arc].next_in_arc = as.next_in_arc;
        else
            _vertices[as.target].first_in_arc = as.next_in_arc;
    }
    void remove_incident_arcs(const vertex v) noexcept {
        assert(is_valid_vertex(v));
        // in_arcs are already linked by .next_in_arc
        arc last_in_arc = INVALID_ARC;
        for(const arc & a : in_arcs(v)) {
            last_in_arc = a;
            remove_from_source_out_arcs(a);
            _arcs_filter[a] = false;
        }
        arc last_out_arc = INVALID_ARC;
        for(const arc & a : out_arcs(v)) {
            last_out_arc = a;
            remove_from_target_in_arcs(a);
            // once removed from the targets in arcs .next_in_arc is free
            _arcs[a].next_in_arc = _arcs[a].next_out_arc;
            _arcs_filter[a] = false;
        }
        // out_arcs were linked by .next_out_arc
        // [first_out_arc, last_out_arc] are now linked by .next_in_arc
        if(last_in_arc != INVALID_ARC) {
            _arcs[last_in_arc].next_in_arc = _first_free_arc;
            _first_free_arc = last_in_arc;
        }
        if(last_out_arc != INVALID_ARC) {
            _arcs[last_out_arc].next_in_arc = _first_free_arc;
            _first_free_arc = last_out_arc;
        }
    }

public:
    void remove_vertex(const vertex v) noexcept {
        assert(is_valid_vertex(v));
        remove_incident_arcs(v);
        vertex_struct & vs = _vertices[v];
        if(vs.next_vertex != INVALID_VERTEX) {
            _vertices[vs.next_vertex].prev_vertex = vs.prev_vertex;
        }
        if(vs.prev_vertex != INVALID_VERTEX) {
            _vertices[vs.prev_vertex].next_vertex = vs.next_vertex;
        } else {
            _first_vertex = vs.next_vertex;
        }
        vs.next_vertex = _first_free_vertex;
        _first_free_vertex = v;
        _vertices_filter[v] = false;
    }
    void remove_arc(const arc a) noexcept {
        assert(is_valid_arc(a));
        remove_from_source_out_arcs(a);
        remove_from_target_in_arcs(a);
        _arcs[a].next_in_arc = _first_free_arc;
        _first_free_arc = a;
        _arcs_filter[a] = false;
    }
    void change_target(const arc a, const vertex t) noexcept {
        assert(is_valid_arc(a));
        assert(is_valid_vertex(t));
        arc_struct & as = _arcs[a];
        if(as.target == t) return;
        remove_from_target_in_arcs(a);
        vertex_struct & ts = _vertices[t];
        as.target = t;
        as.prev_in_arc = INVALID_ARC;
        as.next_in_arc = ts.first_in_arc;
        if(ts.first_in_arc != INVALID_ARC)
            _arcs[ts.first_in_arc].prev_in_arc = a;
        ts.first_in_arc = a;
    }
    void change_source(const arc a, const vertex s) noexcept {
        assert(is_valid_arc(a));
        assert(is_valid_vertex(s));
        arc_struct & as = _arcs[a];
        if(as.source == s) return;
        remove_from_source_out_arcs(a);
        vertex_struct & ss = _vertices[s];
        as.source = s;
        as.prev_out_arc = INVALID_ARC;
        as.next_out_arc = ss.first_out_arc;
        if(ss.first_out_arc != INVALID_ARC)
            _arcs[ss.first_out_arc].prev_out_arc = a;
        ss.first_out_arc = a;
    }

    template <typename T>
    static_map<vertex, T> create_vertex_map() const noexcept {
        return static_map<vertex, T>(_vertices.size());
    }
    template <typename T>
    static_map<vertex, T> create_vertex_map(
        const T & default_value) const noexcept {
        return static_map<vertex, T>(_vertices.size(), default_value);
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_MUTABLE_DIGRAPH_HPP