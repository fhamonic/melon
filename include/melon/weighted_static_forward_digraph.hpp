#ifndef MELON_WEIGHTED_STATIC_FORWARD_DIGRAPH_HPP
#define MELON_WEIGHTED_STATIC_FORWARD_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

#include "melon/concepts/range_of.hpp"
#include "melon/data_structures/static_map.hpp"
#include "melon/utils/map_view.hpp"

namespace fhamonic {
namespace melon {

template <typename W = int>
class weighted_static_forward_digraph {
private:
    using vertex = unsigned int;
    using arc = std::pair<vertex, W>;

    static_map<vertex, std::size_t> _out_arc_begin;
    std::vector<arc> _arcs;

public:
    template <concepts::forward_range_of<vertex> S,
              concepts::forward_range_of<arc> T>
    weighted_static_forward_digraph(const std::size_t & nb_vertices,
                                    S && arcs_sources, T && arcs) noexcept
        : _out_arc_begin(nb_vertices, 0), _arcs(std::move(arcs)) {
        assert(std::ranges::all_of(
            arcs_sources, [n = nb_vertices](const vertex & v) { return v < n; }));
        assert(std::ranges::all_of(
            arcs, [n = nb_vertices](const arc & a) { return a.first < n; }));
        assert(std::ranges::is_sorted(arcs_sources));
        for(auto && s : arcs_sources) ++_out_arc_begin[s];
        std::exclusive_scan(_out_arc_begin.data(),
                            _out_arc_begin.data() + nb_vertices,
                            _out_arc_begin.data(), 0);
    }

    weighted_static_forward_digraph() = default;
    weighted_static_forward_digraph(
        const weighted_static_forward_digraph & graph) = default;
    weighted_static_forward_digraph(weighted_static_forward_digraph && graph) =
        default;

    weighted_static_forward_digraph & operator=(
        const weighted_static_forward_digraph &) = default;
    weighted_static_forward_digraph & operator=(
        weighted_static_forward_digraph &&) = default;

    auto nb_vertices() const noexcept { return _out_arc_begin.size(); }
    auto nb_arcs() const noexcept { return _arcs.size(); }

    bool is_valid_node(const vertex & u) const noexcept {
        return u < nb_vertices();
    }

    auto vertices() const noexcept {
        return std::views::iota(static_cast<vertex>(0),
                                static_cast<vertex>(nb_vertices()));
    }
    auto arcs() const noexcept {
        return std::views::join(std::views::transform(
            vertices(), [this](auto && u) { return out_arcs(u); }));
    }
    auto out_arcs(const vertex & u) const noexcept {
        assert(is_valid_node(u));
        return std::span(
            _arcs.data() + _out_arc_begin[u],
            (u + 1 < nb_vertices() ? _arcs.data() + _out_arc_begin[u + 1]
                                   : _arcs.data() + nb_arcs()));
    }
    vertex target(const arc & a) const noexcept { return a.first; }
    auto targets_map() const {
        return map_view([](const arc & a) -> vertex { return a.first; });
    }
    W weight(const arc & a) const noexcept { return a.second; }
    auto weight_map() const {
        return map_view([](const arc & a) -> W { return a.second; });
    }
    auto out_neighbors(const vertex & u) const noexcept {
        assert(is_valid_node(u));
        return std::views::keys(out_arcs(u));
    }

    auto out_arcs_pairs(const vertex & u) const noexcept {
        assert(is_valid_node(u));
        return std::views::transform(
            out_neighbors(u), [u](auto && v) { return std::make_pair(u, v); });
    }
    auto arcs_pairs() const noexcept {
        return std::views::join(std::views::transform(
            vertices(), [this](auto && u) { return out_arcs_pairs(u); }));
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

#endif  // MELON_WEIGHTED_STATIC_FORWARD_DIGRAPH_HPP