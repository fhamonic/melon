#ifndef MELON_VIEWS_COMPLETE_DIGRAPH_HPP
#define MELON_VIEWS_COMPLETE_DIGRAPH_HPP

#include <concepts>
#include <ranges>

#include "melon/container/static_map.hpp"
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

private:
    template <std::integral T>
    class custom_iota_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using reference = T;
        using pointer = void;
        using difference_type = std::ptrdiff_t;

    private:
        T _cursor;
        T _upper_bound;
        T _increment;

    public:
        constexpr custom_iota_iterator(T cursor, T upper_bound,
                                       T increment = T{1})
            : _cursor(cursor)
            , _upper_bound(upper_bound)
            , _increment(increment) {}

        constexpr custom_iota_iterator() = default;
        constexpr custom_iota_iterator(custom_iota_iterator &&) = default;
        constexpr custom_iota_iterator(const custom_iota_iterator &) = default;

        constexpr const reference operator*() const { return _cursor; }
        constexpr custom_iota_iterator & operator++() noexcept {
            _cursor += _increment;
            return *this;
        }
        constexpr custom_iota_iterator operator++(int) noexcept {
            custom_iota_iterator it(*this);
            _cursor += _increment;
            return it;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const custom_iota_iterator & it, std::default_sentinel_t) noexcept {
            return it._cursor >= it._upper_bound;
        }

        constexpr custom_iota_iterator & operator=(custom_iota_iterator &&) =
            default;
        constexpr custom_iota_iterator & operator=(
            const custom_iota_iterator &) = default;

        [[nodiscard]] constexpr friend bool operator==(
            const custom_iota_iterator & it1,
            const custom_iota_iterator & it2) noexcept {
            assert(it1._increment == it2._increment);
            return it1._cursor == it2._cursor;
        }
    };

public:
    [[nodiscard]] constexpr auto in_arcs(const vertex u) const noexcept {
        assert(u < _num_vertices);
        const auto increment = static_cast<arc>(_num_vertices - 1);
        return std::views::concat(
            std::ranges::subrange(
                custom_iota_iterator(static_cast<arc>(u - 1),
                                     static_cast<arc>(u) * increment,
                                     increment),
                std::default_sentinel),
            std::ranges::subrange(
                custom_iota_iterator(
                    static_cast<arc>((u + 1) * _num_vertices - 1),
                    static_cast<arc>(num_arcs()), increment),
                std::default_sentinel));
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