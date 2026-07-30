#pragma once

#include <concepts>
#include <ranges>
#include <type_traits>

#include "melon/graph.hpp"
#include "melon/mapping.hpp"

namespace melon {

namespace cpo {
template <typename T>
concept has_member_edges = requires(const T & t) {
    { t.edges() } -> std::ranges::input_range;
};

template <typename T>
concept has_adl_edges = requires(const T & t) {
    { edges(t) } -> std::ranges::input_range;
};

// `auto`, not `decltype(auto)`: every other range-returning CPO in melon
// decay-copies, so edges_range_t was the one range alias that could name a
// reference type. A graph that stores its edges in a container returns
// std::views::all(container) -- a ref_view, no copy -- which is the
// std::ranges idiom and what melon already requires of arc_targets_map.
struct edges_fn {
    // One overload per protocol, each carrying the noexcept of the expression
    // directly beside it. The private is_noexcept() this replaces re-ran the
    // same `if constexpr` in a second place, which is how undirected_graph.hpp
    // came to measure an overload its operator() never called.
    template <typename T>
        requires has_member_edges<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(t.edges())) {
        return t.edges();
    }

    template <typename T>
        requires(!has_member_edges<T>) && has_adl_edges<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(edges(t))) {
        return edges(t);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::edges_fn edges{};
}  // namespace cust

template <typename T>
using edges_range_t = decltype(melon::edges(std::declval<const T &>()));

template <typename T>
using edge_t = std::ranges::range_value_t<edges_range_t<T>>;

namespace cpo {
template <typename T>
concept has_member_num_edges = requires(const T & t) {
    { t.num_edges() } -> std::integral;
};

template <typename T>
concept has_adl_num_edges = requires(const T & t) {
    { num_edges(t) } -> std::integral;
};

struct num_edges_fn {
    // See vertices_fn: one overload per protocol, each carrying its own
    // noexcept, including the size-of-the-range fallback.
    template <typename T>
        requires has_member_num_edges<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(t.num_edges())) {
        return t.num_edges();
    }

    template <typename T>
        requires(!has_member_num_edges<T>) && has_adl_num_edges<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(num_edges(t))) {
        return num_edges(t);
    }

    template <typename T>
        requires(!has_member_num_edges<T>) && (!has_adl_num_edges<T>) &&
                std::ranges::sized_range<edges_range_t<T>>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(std::ranges::size(melon::edges(t)))) {
        return std::ranges::size(melon::edges(t));
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::num_edges_fn num_edges{};
}  // namespace cust

namespace cpo {
template <typename T>
concept has_member_edge_endpoints = requires(const T & t, const edge_t<T> & e) {
    {
        t.edge_endpoints(e)
    } -> std::convertible_to<std::pair<vertex_t<T>, vertex_t<T>>>;
};

template <typename T>
concept has_adl_edge_endpoints = requires(const T & t, const edge_t<T> & e) {
    {
        edge_endpoints(t, e)
    } -> std::convertible_to<std::pair<vertex_t<T>, vertex_t<T>>>;
};

struct edge_endpoints_fn {
    // One overload per protocol, each carrying the noexcept of the expression
    // directly beside it. The private is_noexcept() this replaces re-ran the
    // same `if constexpr` in a second place, which is how undirected_graph.hpp
    // came to measure an overload its operator() never called.
    template <typename T>
        requires has_member_edge_endpoints<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const edge_t<T> & a) const
        noexcept(noexcept(t.edge_endpoints(a))) {
        return t.edge_endpoints(a);
    }

    template <typename T>
        requires(!has_member_edge_endpoints<T>) && has_adl_edge_endpoints<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const edge_t<T> & a) const
        noexcept(noexcept(edge_endpoints(t, a))) {
        return edge_endpoints(t, a);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::edge_endpoints_fn edge_endpoints{};
}  // namespace cust

namespace cpo {
template <typename T>
concept has_member_incidence = requires(const T & t, const vertex_t<T> & v) {
    { t.incidence(v) } -> std::ranges::input_range;
};

template <typename T>
concept has_adl_incidence = requires(const T & t, const vertex_t<T> & v) {
    { incidence(t, v) } -> std::ranges::input_range;
};

struct incidence_fn {
    // One overload per protocol, each carrying the noexcept of the expression
    // directly beside it. The private is_noexcept() this replaces re-ran the
    // same `if constexpr` in a second place, which is how undirected_graph.hpp
    // came to measure an overload its operator() never called.
    template <typename T>
        requires has_member_incidence<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(t.incidence(v))) {
        return t.incidence(v);
    }

    template <typename T>
        requires(!has_member_incidence<T>) && has_adl_incidence<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(incidence(t, v))) {
        return incidence(t, v);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::incidence_fn incidence{};
}  // namespace cust

template <typename T>
using incidence_range_t = decltype(melon::incidence(
    std::declval<const T &>(), std::declval<const vertex_t<T> &>()));

template <typename T>
using incidence_iterator_t = std::ranges::iterator_t<incidence_range_t<T>>;

template <typename T>
using incidence_sentinel_t = std::ranges::sentinel_t<incidence_range_t<T>>;

namespace cpo {
template <typename T>
concept has_member_degree = requires(const T & t, const vertex_t<T> & v) {
    { t.degree(v) } -> std::integral;
};

template <typename T>
concept has_adl_degree = requires(const T & t, const vertex_t<T> & v) {
    { degree(t, v) } -> std::integral;
};

template <typename T>
concept has_sized_incidence = requires(const T & t, const vertex_t<T> & v) {
    { incidence_fn{}(t, v) } -> std::ranges::sized_range;
};

struct degree_fn {
    // See vertices_fn: one overload per protocol, each carrying its own
    // noexcept, including the size-of-the-range fallback.
    template <typename T>
        requires has_member_degree<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(t.degree(v))) {
        return t.degree(v);
    }

    template <typename T>
        requires(!has_member_degree<T>) && has_adl_degree<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(degree(t, v))) {
        return degree(t, v);
    }

    template <typename T>
        requires(!has_member_degree<T>) &&
                (!has_adl_degree<T>) && has_sized_incidence<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(std::ranges::size(melon::incidence(t, v)))) {
        return std::ranges::size(melon::incidence(t, v));
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::degree_fn degree{};
}  // namespace cust

template <typename T>
concept undirected_graph = requires(const T & t) {
    melon::vertices(t);
    melon::edges(t);
    melon::edge_endpoints(t, std::declval<edge_t<T>>());
};

template <typename T>
concept has_num_edges =
    undirected_graph<T> && requires(const T & t) { melon::num_edges(t); };

template <typename T>
concept has_incidence =
    undirected_graph<T> &&
    requires(const T & t, const vertex_t<T> & v) { melon::incidence(t, v); } &&
    std::convertible_to<
        typename std::ranges::range_value_t<incidence_range_t<T>>::first_type,
        edge_t<T>> &&
    std::convertible_to<
        typename std::ranges::range_value_t<incidence_range_t<T>>::second_type,
        vertex_t<T>>;

template <typename T>
concept has_degree =
    undirected_graph<T> &&
    requires(const T & t, const vertex_t<T> & v) { melon::degree(t, v); };

namespace cpo {
template <typename T, typename ValueType>
concept has_member_create_edge_map =
    requires(const T & t, const ValueType & d) {
        {
            t.template create_edge_map<ValueType>()
        } -> output_mapping_of<edge_t<T>, ValueType>;
        {
            t.template create_edge_map<ValueType>(d)
        } -> output_mapping_of<edge_t<T>, ValueType>;
    };

template <typename T, typename ValueType>
concept has_adl_create_edge_map = requires(const T & t, const ValueType & d) {
    {
        create_edge_map<ValueType>(t)
    } -> output_mapping_of<edge_t<T>, ValueType>;
    {
        create_edge_map<ValueType>(t, d)
    } -> output_mapping_of<edge_t<T>, ValueType>;
};

// Parameterised on ValueType so that the public name can be a *variable*
// template rather than a function template, for exactly the reason spelled out
// on create_vertex_map_fn in graph.hpp: a function template named
// create_edge_map living in namespace melon is reachable by ADL from
// has_adl_create_edge_map for every type whose associated namespaces include
// melon (anything deriving from undirected_graph_view_base, every melon
// undirected view), which makes the concept depend on itself. Variable
// templates are not found by ADL, so the loop cannot close.
template <typename ValueType>
struct create_edge_map_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_create_edge_map<T, ValueType>)
            return noexcept(std::declval<const T &>()
                                .template create_edge_map<ValueType>());
        else
            return noexcept(
                create_edge_map<ValueType>(std::declval<const T &>()));
    }

    // See create_vertex_map_fn::is_noexcept_default in graph.hpp: the
    // default-value overload has to probe the call it actually makes.
    template <typename T>
    static constexpr bool is_noexcept_default() {
        if constexpr(has_member_create_edge_map<T, ValueType>)
            return noexcept(
                std::declval<const T &>().template create_edge_map<ValueType>(
                    std::declval<const ValueType &>()));
        else
            return noexcept(create_edge_map<ValueType>(
                std::declval<const T &>(), std::declval<const ValueType &>()));
    }

public:
    template <typename T>
        requires has_member_create_edge_map<T, ValueType> ||
                 has_adl_create_edge_map<T, ValueType>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_create_edge_map<T, ValueType>)
            return t.template create_edge_map<ValueType>();
        else
            return create_edge_map<ValueType>(t);
    }

    template <typename T>
        requires has_member_create_edge_map<T, ValueType> ||
                 has_adl_create_edge_map<T, ValueType>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const ValueType & d) const
        noexcept(is_noexcept_default<T>()) {
        if constexpr(has_member_create_edge_map<T, ValueType>)
            return t.template create_edge_map<ValueType>(d);
        else
            return create_edge_map<ValueType>(t, d);
    }
};
}  // namespace cpo

inline namespace cust {
// A variable template, not a function template: `create_edge_map<T>(g)` reads
// the same at every call site, but the name is now invisible to ADL. See the
// comment on create_edge_map_fn.
template <typename ValueType>
inline constexpr cpo::create_edge_map_fn<ValueType> create_edge_map{};
}  // namespace cust

template <typename T, typename ValueType>
using edge_map_t =
    decltype(melon::create_edge_map<ValueType>(std::declval<const T &>()));

template <typename T, typename ValueType = std::size_t>
concept has_edge_map =
    undirected_graph<T> && requires(const T & t, const ValueType & d) {
        melon::create_edge_map<ValueType>(t);
        melon::create_edge_map<ValueType>(t, d);
    };

}  // namespace melon

#include "views/undirected_graph_view.hpp"
