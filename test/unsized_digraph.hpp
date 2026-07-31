#ifndef UNSIZED_DIGRAPH_HPP
#define UNSIZED_DIGRAPH_HPP

#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "melon/container/static_map.hpp"
#include "melon/graph.hpp"

// A conforming digraph with no route to num_vertices: no member, no ADL
// overload, and a filter_view vertex range so the sized-range CPO fallback is
// unavailable too. It exists to pin the algorithms' no-reserve code paths --
// e.g. the flow BFS queues grow while being walked, which reallocates when
// nothing reserved them. Unlike dumb_digraph, the incidence ranges are spans,
// so per-vertex cursor maps stay default-constructible and the flow
// algorithms compile over it.
class unsized_digraph {
public:
    using vertex = unsigned int;
    using arc = unsigned int;

private:
    vertex _nb_vertices;
    std::vector<std::vector<arc>> _out_arcs, _in_arcs;
    std::vector<std::pair<vertex, vertex>> _arc_ends;

public:
    unsized_digraph(const vertex n,
                    const std::vector<std::pair<vertex, vertex>> & arc_pairs)
        : _nb_vertices(n), _out_arcs(n), _in_arcs(n) {
        for(const auto & [s, t] : arc_pairs) {
            const arc a = static_cast<arc>(_arc_ends.size());
            _out_arcs[s].push_back(a);
            _in_arcs[t].push_back(a);
            _arc_ends.emplace_back(s, t);
        }
    }

    auto vertices() const noexcept {
        return std::views::iota(vertex(0), _nb_vertices) |
               std::views::filter([](const vertex &) { return true; });
    }
    auto out_arcs(const vertex u) const noexcept {
        return std::span<const arc>(_out_arcs[u]);
    }
    auto in_arcs(const vertex u) const noexcept {
        return std::span<const arc>(_in_arcs[u]);
    }
    vertex arc_source(const arc a) const noexcept { return _arc_ends[a].first; }
    vertex arc_target(const arc a) const noexcept {
        return _arc_ends[a].second;
    }
    template <typename T>
    auto create_vertex_map() const {
        return melon::static_map<vertex, T>(_nb_vertices);
    }
    template <typename T>
    auto create_vertex_map(const T & default_value) const {
        return melon::static_map<vertex, T>(_nb_vertices, default_value);
    }
    template <typename T>
    auto create_arc_map() const {
        return melon::static_map<arc, T>(_arc_ends.size());
    }
    template <typename T>
    auto create_arc_map(const T & default_value) const {
        return melon::static_map<arc, T>(_arc_ends.size(), default_value);
    }
};

// The property this fixture exists for; if a new CPO route ever makes
// num_vertices answerable, the fixture no longer tests anything and this
// fires.
static_assert(!melon::has_num_vertices<unsized_digraph>);
static_assert(melon::outward_incidence_graph<unsized_digraph>);
static_assert(melon::inward_incidence_graph<unsized_digraph>);

#endif  // UNSIZED_DIGRAPH_HPP
