#ifndef MELON_STATIC_FORWARD_WEIGHTED_DIGRAPH_HPP
#define MELON_STATIC_FORWARD_WEIGHTED_DIGRAPH_HPP

#include <algorithm>
#include <cassert>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

#include <range/v3/view/iota.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>

#include "melon/concepts/range_of.hpp"
#include "melon/data_structures/static_map.hpp"
#include "melon/utils/map_view.hpp"

namespace fhamonic {
namespace melon {

template <typename W = int>
class static_forward_weighted_digraph {
private:
    using vertex = unsigned int;
    using arc = std::vector<std::pair<vertex, W>>::const_iterator;

    std::vector<std::vector<std::pair<vertex, W>>> _adjacency;

public:
    template <
        concepts::forward_range_of<std::pair<std::pair<vertex, vertex>, W>> A>
    [[nodiscard]] constexpr static_forward_weighted_digraph(
        const std::size_t & nb_vertices, A && arcs_entries) noexcept {
        assert(std::ranges::all_of(
            arcs_entries, [n = nb_vertices](const auto & p) {
                return p.first.first < n && p.first.second < n;
            }));
        _adjacency.resize(nb_vertices);
        for(auto && [arc_pair, weight] : arcs_entries)
            _adjacency[arc_pair.first].emplace_back(arc_pair.second, weight);
    }

    [[nodiscard]] constexpr static_forward_weighted_digraph() = default;
    [[nodiscard]] constexpr static_forward_weighted_digraph(
        const static_forward_weighted_digraph & graph) = default;
    [[nodiscard]] constexpr static_forward_weighted_digraph(
        static_forward_weighted_digraph && graph) = default;

    static_forward_weighted_digraph & operator=(
        const static_forward_weighted_digraph &) = default;
    static_forward_weighted_digraph & operator=(
        static_forward_weighted_digraph &&) = default;

    [[nodiscard]] constexpr auto nb_vertices() const noexcept {
        return _adjacency.size();
    }

    [[nodiscard]] constexpr bool is_valid_vertex(
        const vertex & u) const noexcept {
        return u < nb_vertices();
    }

    [[nodiscard]] constexpr auto vertices() const noexcept {
        return std::views::iota(static_cast<vertex>(0),
                                static_cast<vertex>(nb_vertices()));
    }
    [[nodiscard]] constexpr auto arcs() const noexcept {
        return std::views::join(std::views::transform(
            vertices(), [this](auto && u) { return out_arcs(u); }));
    }
    [[nodiscard]] constexpr auto out_arcs(const vertex u) const noexcept {
        assert(is_valid_vertex(u));
        return ranges::views::iota(_adjacency[u].begin(), _adjacency[u].end());
    }
    [[nodiscard]] constexpr vertex target(const arc & a) const noexcept {
        return a->first;
    }
    [[nodiscard]] constexpr auto targets_map() const {
        return map_view([](const arc & a) -> vertex { return a->first; });
    }
    [[nodiscard]] constexpr W weight(const arc & a) const noexcept {
        return a->second;
    }
    [[nodiscard]] constexpr auto weights_map() const noexcept {
        return map_view([](const arc & a) -> W { return a->second; });
    }
    // [[nodiscard]] constexpr auto weights_map() noexcept {
    //     return map_view([](arc & a) mutable -> W & { return a->second; });
    // }

    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map() const noexcept {
        return static_map<vertex, T>(nb_vertices());
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map(
        const T & default_value) const noexcept {
        return static_map<vertex, T>(nb_vertices(), default_value);
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_STATIC_FORWARD_WEIGHTED_DIGRAPH_HPP