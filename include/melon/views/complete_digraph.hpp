#ifndef MELON_VIEWS_COMPLETE_DIGRAPH_HPP
#define MELON_VIEWS_COMPLETE_DIGRAPH_HPP

#include <concepts>
#include <ranges>

#include <range/v3/view/concat.hpp>
#include "melon/detail/range-v3_compatibility.hpp"

#include "melon/container/static_map.hpp"
#include "melon/detail/intrusive_view.hpp"
#include "melon/mapping.hpp"
#include "melon/views/graph_view.hpp"

namespace fhamonic {
namespace melon {
namespace views {

template <std::integral V = unsigned int, std::integral A = unsigned int>
class complete_digraph : public graph_view_base {
private:
    using vertex = V;
    using arc = A;

    vertex _num_vertices;

public:
    [[nodiscard]] constexpr explicit complete_digraph(const std::size_t n = 0)
        : _num_vertices(static_cast<vertex>(n)) {}

    [[nodiscard]] constexpr complete_digraph(const complete_digraph &) =
        default;
    [[nodiscard]] constexpr complete_digraph(complete_digraph &&) = default;

    constexpr complete_digraph & operator=(const complete_digraph &) = default;
    constexpr complete_digraph & operator=(complete_digraph &&) = default;

    [[nodiscard]] constexpr std::size_t num_vertices() const noexcept {
        return _num_vertices;
    }
    [[nodiscard]] constexpr std::size_t num_arcs() const noexcept {
        return static_cast<arc>(_num_vertices) *
               static_cast<arc>(_num_vertices - 1);
    }

    [[nodiscard]] constexpr auto vertices() const noexcept {
        return std::views::iota(vertex(0), _num_vertices);
    }
    [[nodiscard]] constexpr auto arcs() const noexcept {
        return std::views::iota(arc(0), num_arcs());
    }

    [[nodiscard]] constexpr vertex arc_source(const arc a) const noexcept {
        assert(a < num_arcs());
        return static_cast<vertex>(a / (_num_vertices - 1));
    }
    [[nodiscard]] constexpr vertex arc_target(const arc a) const noexcept {
        assert(a < num_arcs());
        vertex source = arc_source(a);
        vertex target = a % (_num_vertices - 1);
        return target + (source <= target);
    }

    [[nodiscard]] constexpr auto out_arcs(const vertex u) const noexcept {
        assert(u < _num_vertices);
        return std::views::iota(
            static_cast<arc>(u * (_num_vertices - 1)),
            static_cast<arc>((u + 1) * (_num_vertices - 1)));
    }
    [[nodiscard]] constexpr auto in_arcs(const vertex u) const noexcept {
        assert(u < _num_vertices);
        return ranges::views::concat(
            intrusive_view(
                static_cast<arc>(u - 1), std::identity(),
                [offset = static_cast<arc>(_num_vertices - 1)](const arc a) {
                    return a + offset;
                },
                [offset = static_cast<arc>(_num_vertices - 1), u](const arc a)
                    -> bool { return a < static_cast<arc>(u) * offset; }),
            intrusive_view(
                static_cast<arc>((u + 1) * _num_vertices - 1), std::identity(),
                [offset = static_cast<arc>(_num_vertices - 1)](const arc a) {
                    return a + offset;
                },
                [m = static_cast<arc>(num_arcs())](const arc a) -> bool {
                    return a < m;
                }));
    }

    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map() const noexcept {
        return static_map<vertex, T>(_num_vertices);
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map(
        const T & default_value) const noexcept {
        return static_map<vertex, T>(_num_vertices, default_value);
    }

    template <typename T>
    [[nodiscard]] constexpr auto create_arc_map() const noexcept {
        return static_map<arc, T>(num_arcs());
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_arc_map(
        const T & default_value) const noexcept {
        return static_map<arc, T>(num_arcs(), default_value);
    }
};

}  // namespace views
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_VIEWS_COMPLETE_DIGRAPH_HPP