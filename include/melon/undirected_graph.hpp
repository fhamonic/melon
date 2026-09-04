#pragma once

#include <concepts>
#include <cstddef>
#include <ranges>
#include <type_traits>
#include <utility>

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

// Range-returning, so `T &&` + category constraint per the vertices_fn
// contract in graph.hpp: `const T &` would accept a temporary graph and
// dangle behind the returned view. The body still reads through as_const --
// test/undirected_graph.cpp pins that the const overload is the one called,
// keeping edges_range_t (computed from a const graph) the type the call
// actually returns.
struct edges_fn {
    template <typename T, typename G = std::remove_cvref_t<T>>
        requires has_member_edges<G> &&
                 (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator() [[nodiscard]] (T && t) const
        noexcept(noexcept(std::as_const(t).edges())) {
        return std::as_const(t).edges();
    }

    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(!has_member_edges<G>) && has_adl_edges<G> &&
                (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator() [[nodiscard]] (T && t) const
        noexcept(noexcept(edges(std::as_const(t)))) {
        return edges(std::as_const(t));
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

// Range-returning, so `T &&` + category constraint per the vertices_fn
// contract in graph.hpp; as_const in the body per edges_fn above.
struct incidence_fn {
    template <typename T, typename G = std::remove_cvref_t<T>>
        requires has_member_incidence<G> &&
                 (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator()
        [[nodiscard]] (T && t, const vertex_t<G> & v) const
        noexcept(noexcept(std::as_const(t).incidence(v))) {
        return std::as_const(t).incidence(v);
    }

    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(!has_member_incidence<G>) && has_adl_incidence<G> &&
                (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator()
        [[nodiscard]] (T && t, const vertex_t<G> & v) const
        noexcept(noexcept(incidence(std::as_const(t), v))) {
        return incidence(std::as_const(t), v);
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

// Shape-probed through std::get, mirroring the directed layer's
// arc_entry_shape: naming ::first_type instead would demand literal
// std::pair and silently reject a tuple-shaped incidence the directed side
// accepts.
template <typename T>
concept has_incidence =
    undirected_graph<T> && requires(const T & t, const vertex_t<T> & v) {
        melon::incidence(t, v);
    } && requires(const std::ranges::range_value_t<incidence_range_t<T>> & e) {
        { std::get<0>(e) } -> std::convertible_to<edge_t<T>>;
        { std::get<1>(e) } -> std::convertible_to<vertex_t<T>>;
    };

template <typename T>
concept has_degree =
    undirected_graph<T> &&
    requires(const T & t, const vertex_t<T> & v) { melon::degree(t, v); };

}  // namespace melon

// Outside namespace melon for the reason spelled out on graph.hpp's
// melon_create_map_cpo: the unqualified calls must reach global-scope
// customization functions without MSVC's instantiation-time lookup meeting
// the melon::create_edge_map variable below.
namespace melon_create_map_cpo {
// The two factory shapes and their probe order, as on graph.hpp's
// create_vertex_map_fn.
template <typename T, typename ValueType, typename Role>
concept has_role_member_create_edge_map =
    requires(const T & t, const ValueType & d) {
        {
            t.template create_edge_map<ValueType, Role>()
        } -> melon::output_mapping_of<melon::edge_t<T>, ValueType>;
        {
            t.template create_edge_map<ValueType, Role>(d)
        } -> melon::output_mapping_of<melon::edge_t<T>, ValueType>;
    };

template <typename T, typename ValueType, typename Role>
concept has_role_adl_create_edge_map =
    requires(const T & t, const ValueType & d) {
        {
            create_edge_map<ValueType, Role>(t)
        } -> melon::output_mapping_of<melon::edge_t<T>, ValueType>;
        {
            create_edge_map<ValueType, Role>(t, d)
        } -> melon::output_mapping_of<melon::edge_t<T>, ValueType>;
    };

template <typename T, typename ValueType>
concept has_member_create_edge_map =
    requires(const T & t, const ValueType & d) {
        {
            t.template create_edge_map<ValueType>()
        } -> melon::output_mapping_of<melon::edge_t<T>, ValueType>;
        {
            t.template create_edge_map<ValueType>(d)
        } -> melon::output_mapping_of<melon::edge_t<T>, ValueType>;
    };

template <typename T, typename ValueType>
concept has_adl_create_edge_map = requires(const T & t, const ValueType & d) {
    {
        create_edge_map<ValueType>(t)
    } -> melon::output_mapping_of<melon::edge_t<T>, ValueType>;
    {
        create_edge_map<ValueType>(t, d)
    } -> melon::output_mapping_of<melon::edge_t<T>, ValueType>;
};

template <typename T, typename ValueType, typename Role>
concept can_create_edge_map =
    has_role_member_create_edge_map<T, ValueType, Role> ||
    has_role_adl_create_edge_map<T, ValueType, Role> ||
    has_member_create_edge_map<T, ValueType> ||
    has_adl_create_edge_map<T, ValueType>;

// Parameterised on ValueType and Role so that the public name can be a
// *variable* template rather than a function template: a function template
// named create_edge_map living in namespace melon is reachable by ADL from
// has_adl_create_edge_map for every type whose associated namespaces include
// melon (every melon undirected view), which makes the concept depend on
// itself. Variable templates are not found by ADL, so the loop cannot close.
template <typename ValueType, typename Role>
struct create_edge_map_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_role_member_create_edge_map<T, ValueType, Role>)
            return noexcept(std::declval<const T &>()
                                .template create_edge_map<ValueType, Role>());
        else if constexpr(has_role_adl_create_edge_map<T, ValueType, Role>)
            return noexcept(
                create_edge_map<ValueType, Role>(std::declval<const T &>()));
        else if constexpr(has_member_create_edge_map<T, ValueType>)
            return noexcept(std::declval<const T &>()
                                .template create_edge_map<ValueType>());
        else
            return noexcept(
                create_edge_map<ValueType>(std::declval<const T &>()));
    }

    template <typename T>
    static constexpr bool is_noexcept_default() {
        if constexpr(has_role_member_create_edge_map<T, ValueType, Role>)
            return noexcept(std::declval<const T &>()
                                .template create_edge_map<ValueType, Role>(
                                    std::declval<const ValueType &>()));
        else if constexpr(has_role_adl_create_edge_map<T, ValueType, Role>)
            return noexcept(create_edge_map<ValueType, Role>(
                std::declval<const T &>(), std::declval<const ValueType &>()));
        else if constexpr(has_member_create_edge_map<T, ValueType>)
            return noexcept(
                std::declval<const T &>().template create_edge_map<ValueType>(
                    std::declval<const ValueType &>()));
        else
            return noexcept(create_edge_map<ValueType>(
                std::declval<const T &>(), std::declval<const ValueType &>()));
    }

public:
    template <typename T>
        requires can_create_edge_map<T, ValueType, Role>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_role_member_create_edge_map<T, ValueType, Role>)
            return t.template create_edge_map<ValueType, Role>();
        else if constexpr(has_role_adl_create_edge_map<T, ValueType, Role>)
            return create_edge_map<ValueType, Role>(t);
        else if constexpr(has_member_create_edge_map<T, ValueType>)
            return t.template create_edge_map<ValueType>();
        else
            return create_edge_map<ValueType>(t);
    }

    template <typename T>
        requires can_create_edge_map<T, ValueType, Role>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const ValueType & d) const
        noexcept(is_noexcept_default<T>()) {
        if constexpr(has_role_member_create_edge_map<T, ValueType, Role>)
            return t.template create_edge_map<ValueType, Role>(d);
        else if constexpr(has_role_adl_create_edge_map<T, ValueType, Role>)
            return create_edge_map<ValueType, Role>(t, d);
        else if constexpr(has_member_create_edge_map<T, ValueType>)
            return t.template create_edge_map<ValueType>(d);
        else
            return create_edge_map<ValueType>(t, d);
    }
};
}  // namespace melon_create_map_cpo

namespace melon {

inline namespace cust {
template <typename ValueType, typename Role = default_role>
inline constexpr melon_create_map_cpo::create_edge_map_fn<ValueType, Role>
    create_edge_map{};
}  // namespace cust

template <typename T, typename ValueType, typename Role = default_role>
using edge_map_t = decltype(melon::create_edge_map<ValueType, Role>(
    std::declval<const T &>()));

template <typename T, typename ValueType = std::size_t,
          typename Role = default_role>
concept has_edge_map =
    undirected_graph<T> && requires(const T & t, const ValueType & d) {
        melon::create_edge_map<ValueType, Role>(t);
        melon::create_edge_map<ValueType, Role>(t, d);
    };

}  // namespace melon

#include "views/undirected_graph_view.hpp"
