#ifndef MELON_UNDIRECTED_GRAPH_HPP
#define MELON_UNDIRECTED_GRAPH_HPP

#include <concepts>
#include <ranges>
#include <type_traits>

#include "melon/graph.hpp"
#include "melon/mapping.hpp"

namespace fhamonic {
namespace melon {

namespace __cust_access {
template <typename _Tp>
concept __member_edges = requires(const _Tp & __t) {
    { __t.edges() } -> std::ranges::input_range;
};

template <typename _Tp>
concept __adl_edges = requires(const _Tp & __t) {
    { edges(__t) } -> std::ranges::input_range;
};

struct _Edges {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_edges<_Tp>)
            return noexcept(std::declval<_Tp &>().edges());
        else
            return noexcept(edges(std::declval<_Tp &>()));
    }

public:
    template <typename _Tp>
        requires __member_edges<_Tp> || __adl_edges<_Tp>
    constexpr decltype(auto) operator() [[nodiscard]] (const _Tp & __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_edges<_Tp>)
            return __t.edges();
        else if constexpr(__adl_edges<_Tp>)
            return edges(__t);
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_Edges edges{};
}  // namespace __cust

template <typename _Tp>
using edges_range_t = decltype(melon::edges(std::declval<_Tp &>()));

template <typename _Tp>
using edge_t = std::ranges::range_value_t<edges_range_t<_Tp>>;

namespace __cust_access {
template <typename _Tp>
concept __member_num_edges = requires(const _Tp & __t) {
    { __t.num_edges() } -> std::integral;
};

template <typename _Tp>
concept __adl_num_edges = requires(const _Tp & __t) {
    { num_edges(__t) } -> std::integral;
};

struct _NumEdges {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_num_edges<_Tp>)
            return noexcept(std::declval<_Tp &>().num_edges());
        else if constexpr(__adl_num_edges<_Tp>)
            return noexcept(num_edges(std::declval<_Tp &>()));
        else
            return noexcept(
                std::ranges::size(melon::edges(std::declval<_Tp &>())));
    }

public:
    template <typename _Tp>
        requires __member_num_edges<_Tp> || __adl_num_edges<_Tp> ||
                 std::ranges::sized_range<edges_range_t<_Tp>>
    constexpr auto operator() [[nodiscard]] (const _Tp & __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_num_edges<_Tp>)
            return __t.num_edges();
        else if constexpr(__adl_num_edges<_Tp>)
            return num_edges(__t);
        else
            return std::ranges::size(melon::edges(__t));
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_NumEdges num_edges{};
}  // namespace __cust

namespace __cust_access {
template <typename _Tp>
concept __member_edge_endpoints =
    requires(const _Tp & __t, const edge_t<_Tp> & __e) {
        {
            __t.edge_endpoints(__e)
        } -> std::convertible_to<std::pair<vertex_t<_Tp>, vertex_t<_Tp>>>;
    };

template <typename _Tp>
concept __adl_edge_endpoints =
    requires(const _Tp & __t, const edge_t<_Tp> & __e) {
        {
            edge_endpoints(__t, __e)
        } -> std::convertible_to<std::pair<vertex_t<_Tp>, vertex_t<_Tp>>>;
    };

struct _EdgeEndpoints {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_edge_endpoints<_Tp>)
            return noexcept(std::declval<_Tp &>().edge_endpoints(
                std::declval<edge_t<_Tp> &>()));
        else
            return noexcept(edge_endpoints(std::declval<_Tp &>(),
                                           std::declval<edge_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_edge_endpoints<_Tp> || __adl_edge_endpoints<_Tp>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const edge_t<_Tp> & __a) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_edge_endpoints<_Tp>)
            return __t.edge_endpoints(__a);
        else
            return edge_endpoints(__t, __a);
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_EdgeEndpoints edge_endpoints{};
}  // namespace __cust


namespace __cust_access {
template <typename _Tp>
concept __member_incidence =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { __t.incidence(__v) } -> std::ranges::input_range;
    };

template <typename _Tp>
concept __adl_incidence =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { incidence(__t, __v) } -> std::ranges::input_range;
    };

struct _IncidentEdges {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_incidence<_Tp>)
            return noexcept(std::declval<_Tp &>().incidence(
                std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(incidence(std::declval<_Tp &>(),
                                           std::declval<vertex_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_incidence<_Tp> || __adl_incidence<_Tp>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_incidence<_Tp>)
            return __t.incidence(__v);
        else
            return incidence(__t, __v);
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_IncidentEdges incidence{};
}  // namespace __cust

template <typename _Tp>
using incidence_range_t = decltype(melon::incidence(
    std::declval<_Tp &>(), std::declval<vertex_t<_Tp> &>()));

template <typename _Tp>
using incidence_iterator_t =
    std::ranges::iterator_t<incidence_range_t<_Tp>>;

template <typename _Tp>
using incidence_sentinel_t =
    std::ranges::sentinel_t<incidence_range_t<_Tp>>;

namespace __cust_access {
template <typename _Tp>
concept __member_degree =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { __t.degree(__v) } -> std::integral;
    };

template <typename _Tp>
concept __adl_degree =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { degree(__t, __v) } -> std::integral;
    };

template <typename _Tp>
concept __has_sized_incidence =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { _IncidentEdges{}(__t, __v) } -> std::ranges::sized_range;
    };

struct _Degree {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_degree<_Tp>)
            return noexcept(std::declval<_Tp &>().degree(
                std::declval<vertex_t<_Tp> &>()));
        else if constexpr(__adl_degree<_Tp>)
            return noexcept(degree(std::declval<_Tp &>(),
                                       std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(std::ranges::size(melon::incidence(
                std::declval<_Tp &>(), std::declval<vertex_t<_Tp> &>())));
    }

public:
    template <typename _Tp>
        requires __member_degree<_Tp> || __adl_degree<_Tp> ||
                 __has_sized_incidence<_Tp>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_degree<_Tp>)
            return __t.degree(__v);
        else if constexpr(__adl_degree<_Tp>)
            return degree(__t, __v);
        else
            return std::ranges::size(melon::incidence(__t, __v));
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_Degree degree{};
}  // namespace __cust


template <typename _Tp>
concept undirected_graph = requires(const _Tp & __t) {
    melon::vertices(__t);
    melon::edges(__t);
    melon::edge_endpoints(__t, std::declval<edge_t<_Tp>>());
};

template <typename _Tp>
concept has_num_edges =
    undirected_graph<_Tp> && requires(const _Tp & __t) { melon::num_edges(__t); };

template <typename _Tp>
concept has_incidence =
    undirected_graph<_Tp> &&
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        melon::incidence(__t, __v);
    } &&
    std::convertible_to<std::ranges::range_value_t<incidence_range_t<_Tp>>,
                        arc_t<_Tp>>;

template <typename _Tp>
concept has_degree =
    undirected_graph<_Tp> && requires(const _Tp & __t) { melon::degree(__t); };

namespace __cust_access {
template <typename _Tp, typename _ValueType>
concept __member_create_edge_map =
    requires(const _Tp & __t, const _ValueType & __d) {
        {
            __t.template create_edge_map<_ValueType>()
        } -> output_mapping_of<edge_t<_Tp>, _ValueType>;
        {
            __t.template create_edge_map<_ValueType>(__d)
        } -> output_mapping_of<edge_t<_Tp>, _ValueType>;
    };

template <typename _Tp, typename _ValueType>
concept __adl_create_edge_map =
    requires(const _Tp & __t, const _ValueType & __d) {
        {
            create_edge_map<_ValueType>(__t)
        } -> output_mapping_of<edge_t<_Tp>, _ValueType>;
        {
            create_edge_map<_ValueType>(__t, __d)
        } -> output_mapping_of<edge_t<_Tp>, _ValueType>;
    };

struct _CreateEdgeMap {
private:
    template <typename _ValueType, typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_create_edge_map<_Tp, _ValueType>)
            return noexcept(
                std::declval<_Tp &>().template create_edge_map<_ValueType>());
        else
            return noexcept(create_edge_map<_ValueType>(std::declval<_Tp &>()));
    }

public:
    template <typename _ValueType, typename _Tp>
        requires __member_create_edge_map<_Tp, _ValueType> ||
                 __adl_create_edge_map<_Tp, _ValueType>
    constexpr auto operator() [[nodiscard]] (const _Tp & __t) const
        noexcept(_S_noexcept<_ValueType, _Tp &>()) {
        if constexpr(__member_create_edge_map<_Tp, _ValueType>)
            return __t.template create_edge_map<_ValueType>();
        else
            return create_edge_map<_ValueType>(__t);
    }

    template <typename _ValueType, typename _Tp>
        requires __member_create_edge_map<_Tp, _ValueType> ||
                 __adl_create_edge_map<_Tp, _ValueType>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const _ValueType & __d) const
        noexcept(_S_noexcept<_ValueType, _Tp &>()) {
        if constexpr(__member_create_edge_map<_Tp, _ValueType>)
            return __t.template create_edge_map<_ValueType>(__d);
        else
            return create_edge_map<_ValueType>(__t, __d);
    }
};
}  // namespace __cust_access

inline namespace __cust {
template <typename _ValueType, typename _Tp>
    requires requires(const _Tp & __t) {
        __cust_access::_CreateEdgeMap{}.template operator()<_ValueType>(__t);
    }
inline constexpr auto create_edge_map(const _Tp & __t) noexcept(
    noexcept(__cust_access::_CreateEdgeMap{}.template operator()<_ValueType>(
        std::declval<_Tp &>()))) {
    return __cust_access::_CreateEdgeMap{}.template operator()<_ValueType>(__t);
}
template <typename _ValueType, typename _Tp>
    requires requires(const _Tp & __t, const _ValueType & __d) {
        __cust_access::_CreateEdgeMap{}.template operator()<_ValueType>(__t,
                                                                        __d);
    }
inline constexpr auto
create_edge_map(const _Tp & __t, const _ValueType & __d) noexcept(
    noexcept(__cust_access::_CreateEdgeMap{}.template operator()<_ValueType>(
        std::declval<_Tp &>(), std::declval<_ValueType &>()))) {
    return __cust_access::_CreateEdgeMap{}.template operator()<_ValueType>(__t,
                                                                           __d);
}
}  // namespace __cust

template <typename _Tp, typename _ValueType>
using edge_map_t =
    decltype(melon::create_edge_map<_ValueType>(std::declval<_Tp &>()));

template <typename _Tp, typename _ValueType = std::size_t>
concept has_edge_map =
    undirected_graph<_Tp> && requires(const _Tp & __t, const _ValueType & __d) {
        melon::create_edge_map<_ValueType>(__t);
        melon::create_edge_map<_ValueType>(__t, __d);
    };

}  // namespace melon
}  // namespace fhamonic

#include "views/undirected_graph_view.hpp"

#endif  // MELON_UNDIRECTED_GRAPH_HPP