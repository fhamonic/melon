#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <ranges>

#include "melon/container/static_map.hpp"
#include "melon/detail/borrowed_graph.hpp"
#include "melon/detail/concat_view.hpp"
#include "melon/mapping.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {
namespace views {

template <std::integral V = unsigned int, std::integral A = unsigned int>
class complete_digraph : public graph_view_base {
private:
    using vertex = V;
    using arc = A;

    vertex _vertices_end;

public:
    constexpr explicit complete_digraph(const std::size_t n = 0)
        : _vertices_end(static_cast<vertex>(n)) {}

    constexpr complete_digraph(const complete_digraph &) = default;
    constexpr complete_digraph(complete_digraph &&) = default;

    constexpr complete_digraph & operator=(const complete_digraph &) = default;
    constexpr complete_digraph & operator=(complete_digraph &&) = default;

    [[nodiscard]] constexpr std::size_t num_vertices() const noexcept {
        return static_cast<std::size_t>(_vertices_end);
    }
    [[nodiscard]] constexpr std::size_t num_arcs() const noexcept {
        const std::size_t n = static_cast<std::size_t>(_vertices_end);
        assert(n * (n - 1) <= std::numeric_limits<arc>::max());
        return n * (n - 1);
    }

    [[nodiscard]] constexpr bool is_valid_vertex(
        const vertex u) const noexcept {
        return u < _vertices_end;
    }
    [[nodiscard]] constexpr bool is_valid_arc(const arc a) const noexcept {
        return static_cast<std::size_t>(a) < num_arcs();
    }

    [[nodiscard]] constexpr auto vertices() const noexcept {
        return std::views::iota(vertex(0), _vertices_end);
    }
    [[nodiscard]] constexpr auto arcs() const noexcept {
        return std::views::iota(arc(0), static_cast<arc>(num_arcs()));
    }

    [[nodiscard]] constexpr vertex arc_source(const arc a) const noexcept {
        assert(a < num_arcs());
        return static_cast<vertex>(a / (_vertices_end - 1));
    }
    [[nodiscard]] constexpr vertex arc_target(const arc a) const noexcept {
        assert(a < num_arcs());
        vertex source = arc_source(a);
        vertex target = a % (_vertices_end - 1);
        return target + (source <= target);
    }

    // O(1), and the only way in_degree exists at all: in_arcs below is a
    // concat of two subranges, which is not a sized_range, so the CPO's
    // size-of-the-range fallback never fires for it.
    [[nodiscard]] constexpr vertex out_degree(const vertex u) const noexcept {
        assert(u < _vertices_end);
        return static_cast<vertex>(_vertices_end - 1);
    }
    [[nodiscard]] constexpr vertex in_degree(const vertex u) const noexcept {
        assert(u < _vertices_end);
        return static_cast<vertex>(_vertices_end - 1);
    }

    [[nodiscard]] constexpr auto out_arcs(const vertex u) const noexcept {
        assert(u < _vertices_end);
        return std::views::iota(
            static_cast<arc>(u * (_vertices_end - 1)),
            static_cast<arc>((u + 1) * (_vertices_end - 1)));
    }

private:
    template <std::integral T>
    class custom_iota_iterator {
    public:
        // operator*() returns a prvalue, which Cpp17ForwardIterator forbids
        // (*it must be a reference); the C++20 concept has no such
        // requirement, so the two tags deliberately differ. Nothing outside
        // can catch a regression here -- the class is private and in_arcs()
        // hands out a concat over it, so std::iterator_traits of *this*
        // iterator is unreachable from a test.
        using iterator_concept = std::forward_iterator_tag;
        using iterator_category = std::input_iterator_tag;
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

        constexpr reference operator*() const { return _cursor; }
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
    // The first subrange is empty for u == 0 only by unsigned wraparound: its
    // cursor starts at arc(-1) and the sentinel test above is `>=`, not `!=`.
    // Tightening that test, or making arc signed, turns in_arcs(0) into a walk
    // from -1 that never reaches its bound.
    [[nodiscard]] constexpr auto in_arcs(const vertex u) const noexcept {
        assert(u < _vertices_end);
        const auto increment = static_cast<arc>(_vertices_end - 1);
        return melon::detail::views::concat(
            std::ranges::subrange(
                custom_iota_iterator(static_cast<arc>(u - 1),
                                     static_cast<arc>(u) * increment,
                                     increment),
                std::default_sentinel),
            std::ranges::subrange(
                custom_iota_iterator(
                    static_cast<arc>((u + 1) * _vertices_end - 1),
                    static_cast<arc>(num_arcs()), increment),
                std::default_sentinel));
    }

    // None of the four below are noexcept: they allocate.
    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map() const {
        return static_map<vertex, T>(_vertices_end);
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_vertex_map(
        const T & default_value) const {
        return static_map<vertex, T>(_vertices_end, default_value);
    }

    template <typename T>
    [[nodiscard]] constexpr auto create_arc_map() const {
        return static_map<arc, T>(num_arcs());
    }
    template <typename T>
    [[nodiscard]] constexpr auto create_arc_map(const T & default_value) const {
        return static_map<arc, T>(num_arcs(), default_value);
    }
};

}  // namespace views

// Purely generated: vertices(), arcs() and out_arcs() are iota_views and
// in_arcs() a concat of subranges over self-contained iterators, none of
// which refers to the view object.
template <std::integral V, std::integral A>
inline constexpr bool enable_borrowed_graph<views::complete_digraph<V, A>> =
    true;

}  // namespace melon
