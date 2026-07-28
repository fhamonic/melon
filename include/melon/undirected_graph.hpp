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

struct edges_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_edges<T>)
            return noexcept(std::declval<T &>().edges());
        else
            return noexcept(edges(std::declval<T &>()));
    }

public:
    template <typename T>
        requires has_member_edges<T> || has_adl_edges<T>
    constexpr decltype(auto) operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T &>()) {
        if constexpr(has_member_edges<T>)
            return t.edges();
        else if constexpr(has_adl_edges<T>)
            return edges(t);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::edges_fn edges{};
}  // namespace cust

template <typename T>
using edges_range_t = decltype(melon::edges(std::declval<T &>()));

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
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_num_edges<T>)
            return noexcept(std::declval<T &>().num_edges());
        else if constexpr(has_adl_num_edges<T>)
            return noexcept(num_edges(std::declval<T &>()));
        else
            return noexcept(
                std::ranges::size(melon::edges(std::declval<T &>())));
    }

public:
    template <typename T>
        requires has_member_num_edges<T> || has_adl_num_edges<T> ||
                 std::ranges::sized_range<edges_range_t<T>>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T &>()) {
        if constexpr(has_member_num_edges<T>)
            return t.num_edges();
        else if constexpr(has_adl_num_edges<T>)
            return num_edges(t);
        else
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
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_edge_endpoints<T>)
            return noexcept(std::declval<T &>().edge_endpoints(
                std::declval<edge_t<T> &>()));
        else
            return noexcept(edge_endpoints(std::declval<T &>(),
                                           std::declval<edge_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_edge_endpoints<T> || has_adl_edge_endpoints<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const edge_t<T> & a) const
        noexcept(is_noexcept<T &>()) {
        if constexpr(has_member_edge_endpoints<T>)
            return t.edge_endpoints(a);
        else
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

struct incident_edges_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_incidence<T>)
            return noexcept(
                std::declval<T &>().incidence(std::declval<vertex_t<T> &>()));
        else
            return noexcept(
                incidence(std::declval<T &>(), std::declval<vertex_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_incidence<T> || has_adl_incidence<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(is_noexcept<T &>()) {
        if constexpr(has_member_incidence<T>)
            return t.incidence(v);
        else
            return incidence(t, v);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::incident_edges_fn incidence{};
}  // namespace cust

template <typename T>
using incidence_range_t = decltype(melon::incidence(
    std::declval<T &>(), std::declval<vertex_t<T> &>()));

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
    { incident_edges_fn{}(t, v) } -> std::ranges::sized_range;
};

struct degree_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_degree<T>)
            return noexcept(
                std::declval<T &>().degree(std::declval<vertex_t<T> &>()));
        else if constexpr(has_adl_degree<T>)
            return noexcept(
                degree(std::declval<T &>(), std::declval<vertex_t<T> &>()));
        else
            return noexcept(std::ranges::size(melon::incidence(
                std::declval<T &>(), std::declval<vertex_t<T> &>())));
    }

public:
    template <typename T>
        requires has_member_degree<T> || has_adl_degree<T> ||
                 has_sized_incidence<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(is_noexcept<T &>()) {
        if constexpr(has_member_degree<T>)
            return t.degree(v);
        else if constexpr(has_adl_degree<T>)
            return degree(t, v);
        else
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

struct create_edge_map_fn {
private:
    template <typename ValueType, typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_create_edge_map<T, ValueType>)
            return noexcept(
                std::declval<T &>().template create_edge_map<ValueType>());
        else
            return noexcept(create_edge_map<ValueType>(std::declval<T &>()));
    }

public:
    template <typename ValueType, typename T>
        requires has_member_create_edge_map<T, ValueType> ||
                 has_adl_create_edge_map<T, ValueType>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<ValueType, T &>()) {
        if constexpr(has_member_create_edge_map<T, ValueType>)
            return t.template create_edge_map<ValueType>();
        else
            return create_edge_map<ValueType>(t);
    }

    template <typename ValueType, typename T>
        requires has_member_create_edge_map<T, ValueType> ||
                 has_adl_create_edge_map<T, ValueType>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const ValueType & d) const
        noexcept(is_noexcept<ValueType, T &>()) {
        if constexpr(has_member_create_edge_map<T, ValueType>)
            return t.template create_edge_map<ValueType>(d);
        else
            return create_edge_map<ValueType>(t, d);
    }
};
}  // namespace cpo

inline namespace cust {
template <typename ValueType, typename T>
    requires requires(const T & t) {
        cpo::create_edge_map_fn{}.template operator()<ValueType>(t);
    }
inline constexpr auto create_edge_map(const T & t) noexcept(
    noexcept(cpo::create_edge_map_fn{}.template operator()<ValueType>(
        std::declval<T &>()))) {
    return cpo::create_edge_map_fn{}.template operator()<ValueType>(t);
}
template <typename ValueType, typename T>
    requires requires(const T & t, const ValueType & d) {
        cpo::create_edge_map_fn{}.template operator()<ValueType>(t, d);
    }
inline constexpr auto
create_edge_map(const T & t, const ValueType & d) noexcept(
    noexcept(cpo::create_edge_map_fn{}.template operator()<ValueType>(
        std::declval<T &>(), std::declval<ValueType &>()))) {
    return cpo::create_edge_map_fn{}.template operator()<ValueType>(t, d);
}
}  // namespace cust

template <typename T, typename ValueType>
using edge_map_t =
    decltype(melon::create_edge_map<ValueType>(std::declval<T &>()));

template <typename T, typename ValueType = std::size_t>
concept has_edge_map =
    undirected_graph<T> && requires(const T & t, const ValueType & d) {
        melon::create_edge_map<ValueType>(t);
        melon::create_edge_map<ValueType>(t, d);
    };

}  // namespace melon

#include "views/undirected_graph_view.hpp"
