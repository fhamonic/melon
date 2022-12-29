#ifndef DUMB_DIGRAPH_HPP
#define DUMB_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <ranges>
#include <vector>

#include "melon/utility/value_map.hpp"

// This is a dumb digraph, it implements all the lookups and modification
// operations we need on a digraph, but do it very inefficiently in order to
// guanranty its correctness because there is absolutely no additional data
// except the arcs ids, sources and targets.
class dumb_digraph {
public:
    using vertex = unsigned int;
    using arc = unsigned int;

private:
    std::vector<bool> _vertex_filter;
    struct arc_struct {
        std::pair<vertex, vertex> arc_pair;
        arc id;
        bool exists;
    };
    std::vector<arc_struct> _arcs_structs;

public:
    dumb_digraph() = default;
    dumb_digraph(const dumb_digraph & graph) = default;
    dumb_digraph(dumb_digraph && graph) = default;

    dumb_digraph & operator=(const dumb_digraph &) = default;
    dumb_digraph & operator=(dumb_digraph &&) = default;

    bool is_valid_vertex(const vertex u) const noexcept {
        if(u >= _vertex_filter.size()) return false;
        return _vertex_filter[u];
    }
    bool is_valid_arc(const arc a) const noexcept {
        if(a >= _arcs_structs.size()) return false;
        return _arcs_structs[a].exists;
    }
    auto vertices() const noexcept {
        return std::views::iota(vertex(0),
                                static_cast<vertex>(_vertex_filter.size())) |
               std::views::filter(
                   [this](const vertex & v) { return _vertex_filter[v]; });
    }
    auto valid_arcs_structs() const noexcept {
        return _arcs_structs | std::views::filter([](const arc_struct & as) {
                   return as.exists;
               });
    }
    auto arcs() const noexcept {
        return valid_arcs_structs() |
               std::views::transform(
                   [](const arc_struct & as) -> arc { return as.id; });
    }
    auto nb_vertices() const noexcept {
        return std::ranges::distance(vertices());
    }
    auto nb_arcs() const noexcept { return std::ranges::distance(arcs()); }
    auto arcs_entries() const noexcept {
        return valid_arcs_structs() |
               std::views::transform(
                   [](const arc_struct & as)
                       -> std::pair<arc, std::pair<vertex, vertex>> {
                       return std::make_pair(as.id, as.arc_pair);
                   });
    }
    auto out_arcs(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        return valid_arcs_structs() |
               std::views::filter([u](const arc_struct & as) {
                   return as.arc_pair.first == u;
               }) |
               std::views::transform([](auto && as) { return as.id; });
    }
    auto in_arcs(const vertex & u) const noexcept {
        assert(is_valid_vertex(u));
        return valid_arcs_structs() |
               std::views::filter([u](const arc_struct & as) {
                   return as.arc_pair.second == u;
               }) |
               std::views::transform([](auto && as) { return as.id; });
    }

    vertex arc_source(const arc a) const noexcept {
        assert(is_valid_arc(a));
        return _arcs_structs[a].arc_pair.first;
    }
    vertex arc_target(const arc & a) const noexcept {
        assert(is_valid_arc(a));
        return _arcs_structs[a].arc_pair.second;
    }
    auto arc_sources_map() const noexcept {
        return fhamonic::melon::views::map(
            [this](const arc a) -> vertex { return arc_source(a); });
    }
    auto arc_targets_map() const noexcept {
        return fhamonic::melon::views::map(
            [this](const arc a) -> vertex { return arc_target(a); });
    }
    auto out_neighbors(const vertex & u) const noexcept {
        assert(is_valid_vertex(u));
        return std::views::transform(out_arcs(u),
                                     [this](auto a) { return arc_target(a); });
    }
    auto in_neighbors(const vertex & u) const noexcept {
        assert(is_valid_vertex(u));
        return std::views::transform(in_arcs(u),
                                     [this](auto a) { return arc_source(a); });
    }

    void create_vertex(const vertex u) noexcept {
        assert(!is_valid_vertex(u));
        if(u >= _vertex_filter.size()) {
            _vertex_filter.resize(u + 1, false);
        } else {
            assert(!_vertex_filter[u]);
        }
        _vertex_filter[u] = true;
    }
    void remove_vertex(const vertex u) noexcept {
        assert(is_valid_vertex(u));
        for(arc_struct & as : _arcs_structs) {
            if(as.arc_pair.first == u || as.arc_pair.second == u) {
                as.exists = false;
            }
        }
        _vertex_filter[u] = false;
    }
    void create_arc(const arc a, const vertex s, const vertex t) noexcept {
        assert(!is_valid_arc(a));
        assert(is_valid_vertex(s));
        assert(is_valid_vertex(t));
        if(a >= _arcs_structs.size()) {
            _arcs_structs.resize(
                a + 1, {.arc_pair = {0, 0}, .id = 0, .exists = false});
        }
        _arcs_structs[a] = {.arc_pair = {s, t}, .id = a, .exists = true};
        assert(is_valid_arc(a));
    }
    void remove_arc(const arc a) noexcept {
        assert(is_valid_arc(a));
        _arcs_structs[a].exists = false;
    }
    void change_arc_source(const arc a, const vertex s) noexcept {
        assert(is_valid_arc(a));
        assert(is_valid_vertex(s));
        _arcs_structs[a].arc_pair.first = s;
    }
    void change_arc_target(const arc a, const vertex t) noexcept {
        assert(is_valid_arc(a));
        assert(is_valid_vertex(t));
        _arcs_structs[a].arc_pair.second = t;
    }
};

#endif  // DUMB_DIGRAPH_HPP