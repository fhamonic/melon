#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <numeric>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/detail/stdlib_check.hpp"
#include "melon/graph.hpp"

namespace melon {

namespace detail {
// Probed on E exactly as the range hands it over, reference category
// included: tuple_element_t would strip it and admit a move-only property
// reached through an lvalue entry, which push_arc then fails to copy.
template <typename E, typename Vertex, typename... Props>
constexpr bool builder_entry_fields_convertible =
    []<std::size_t... Is>(std::index_sequence<Is...>) {
        return std::convertible_to<decltype(std::get<0>(std::declval<E>())),
                                   std::pair<Vertex, Vertex>> &&
               (std::convertible_to<
                    decltype(std::get<Is + 1>(std::declval<E>())), Props> &&
                ...);
    }(std::index_sequence_for<Props...>{});

// What one element of an add_arcs range must be: the endpoint pair itself
// for a property-less builder, otherwise a tuple-like holding the pair and
// then one value per property -- the shape std::views::zip produces from an
// endpoints range and the property ranges.
template <typename E, typename Vertex, typename... Props>
concept builder_arc_entry =
    (sizeof...(Props) == 0 &&
     std::convertible_to<E, std::pair<Vertex, Vertex>>) ||
    (sizeof...(Props) > 0 &&
     requires { std::tuple_size<std::remove_cvref_t<E>>::value; } &&
     std::tuple_size_v<std::remove_cvref_t<E>> == 1 + sizeof...(Props) &&
     builder_entry_fields_convertible<E, Vertex, Props...>);
}  // namespace detail

template <graph G, typename... ArcProperty>
class static_digraph_builder {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;
    using property_maps = std::tuple<std::vector<ArcProperty>...>;

    std::size_t _num_vertices;
    std::vector<vertex> _arc_sources;
    std::vector<vertex> _arc_targets;
    property_maps _arc_property_maps;

public:
    static_digraph_builder() : _num_vertices(0) {}
    explicit static_digraph_builder(std::size_t num_vertices_)
        : _num_vertices(num_vertices_) {}

private:
    template <class Maps, class Properties, std::size_t... Is>
    void add_properties(Maps & maps, Properties && properties,
                        std::index_sequence<Is...>) {
        (get<Is>(maps).push_back(
             std::get<Is>(std::forward<Properties>(properties))),
         ...);
    }

    void push_arc(std::pair<vertex, vertex> uv, ArcProperty... properties) {
        assert(_num_vertices > std::max(uv.first, uv.second));
        _arc_sources.push_back(uv.first);
        _arc_targets.push_back(uv.second);
        add_properties(
            _arc_property_maps, std::forward_as_tuple(std::move(properties)...),
            std::make_index_sequence<std::tuple_size<property_maps>{}>{});
    }

    template <std::ranges::input_range R>
    void push_arcs(R && entries) {
        if constexpr(std::ranges::sized_range<R>) {
            const std::size_t n =
                _arc_sources.size() +
                static_cast<std::size_t>(std::ranges::size(entries));
            _arc_sources.reserve(n);
            _arc_targets.reserve(n);
            std::apply(
                [n](auto &... property_map) { (property_map.reserve(n), ...); },
                _arc_property_maps);
        }
        for(auto && entry : entries) {
            if constexpr(sizeof...(ArcProperty) == 0) {
                push_arc(std::forward<decltype(entry)>(entry));
            } else {
                std::apply(
                    [this](auto && uv, auto &&... properties) {
                        push_arc(
                            std::forward<decltype(uv)>(uv),
                            std::forward<decltype(properties)>(properties)...);
                    },
                    std::forward<decltype(entry)>(entry));
            }
        }
    }

    void sort_arcs() {
        auto arcs_zipped_view = std::apply(
            [this](auto &&... property_map) {
                return std::views::zip(_arc_sources, _arc_targets,
                                       property_map...);
            },
            _arc_property_maps);
        std::ranges::sort(arcs_zipped_view, [](const auto & a, const auto & b) {
            if(std::get<0>(a) == std::get<0>(b))
                return std::get<1>(a) < std::get<1>(b);
            return std::get<0>(a) < std::get<0>(b);
        });
    }

public:
    // Ref-qualified so that value category survives a chain: a single
    // `static_digraph_builder &` return would make
    // `std::move(b).add_arc({u, v})` an *lvalue* and send the following
    // .build() to the copying overload.
    //
    // The usual rvalue-builder caveat applies: the reference is to the object
    // the chain started from, so binding it past the end of the full
    // expression (`auto && b = static_digraph_builder<G>(6).add_arc({0, 1});`)
    // dangles, exactly as it would for any other member returning *this.
    // The endpoints travel as one pair -- add_arc({u, v}, length) -- so the
    // call shows where the topology stops and the properties begin, which
    // three positional integers do not.
    static_digraph_builder & add_arc(std::pair<vertex, vertex> uv,
                                     ArcProperty... properties) & {
        push_arc(uv, std::move(properties)...);
        return *this;
    }
    static_digraph_builder && add_arc(std::pair<vertex, vertex> uv,
                                      ArcProperty... properties) && {
        push_arc(uv, std::move(properties)...);
        return std::move(*this);
    }

    // Without properties nothing can follow the endpoints, so the two-vertex
    // spelling every graph library uses is unambiguous and offered too. The
    // requires-clause removes it, rather than the pair form, the moment a
    // property is added.
    static_digraph_builder & add_arc(vertex u, vertex v) &
        requires(sizeof...(ArcProperty) == 0)
    {
        push_arc({u, v});
        return *this;
    }
    static_digraph_builder && add_arc(vertex u, vertex v) &&
        requires(sizeof...(ArcProperty) == 0)
    {
        push_arc({u, v});
        return std::move(*this);
    }

    // Bulk form: a range of endpoint pairs, or of (pair, properties...)
    // tuple-likes -- std::views::zip(endpoints, lengths) -- appended in
    // range order. Sized ranges reserve up front. Properties are copied out
    // of each entry unless the range yields rvalues (std::views::as_rvalue);
    // an rvalue range is not moved from on its own, since a non-borrowed
    // view over the caller's container would drain it.
    template <std::ranges::input_range R>
        requires detail::builder_arc_entry<std::ranges::range_reference_t<R>,
                                           vertex, ArcProperty...>
    static_digraph_builder & add_arcs(R && entries) & {
        push_arcs(std::forward<R>(entries));
        return *this;
    }
    template <std::ranges::input_range R>
        requires detail::builder_arc_entry<std::ranges::range_reference_t<R>,
                                           vertex, ArcProperty...>
    static_digraph_builder && add_arcs(R && entries) && {
        push_arcs(std::forward<R>(entries));
        return std::move(*this);
    }

    // Copies the property vectors: the builder is still usable afterwards
    // (though build() is not idempotent -- it sorts in place, and a second
    // call re-sorts an already-sorted list).
    [[nodiscard]] auto build() & {
        sort_arcs();
        return std::apply(
            [this](auto &&... property_map) {
                return std::make_tuple(
                    G(_num_vertices, _arc_sources, _arc_targets),
                    property_map...);
            },
            _arc_property_maps);
    }

    // Moves them instead. The endpoint vectors are still copied: they go
    // through static_map's range constructor, which copies whatever it is
    // given. Leaves the builder moved-from: valid, to be destroyed rather than
    // added to.
    [[nodiscard]] auto build() && {
        sort_arcs();
        return std::apply(
            [this](auto &&... property_map) {
                return std::make_tuple(
                    G(_num_vertices, _arc_sources, _arc_targets),
                    std::move(property_map)...);
            },
            _arc_property_maps);
    }
};

}  // namespace melon
