#ifndef MELON_GRAPH_HPP
#define MELON_GRAPH_HPP

#include <concepts>
#include <ranges>
#include <type_traits>

namespace fhamonic {
namespace melon {

namespace __detail {
template <int N>
struct __priority_tag : __priority_tag<N - 1> {};
template <>
struct __priority_tag<0> {};

template <typename _Tp, template <typename...> typename _Primary>
struct __is_specialization_of : std::false_type {};

template <template <typename...> typename _Primary, typename... _Args>
struct __is_specialization_of<_Primary<_Args...>, _Primary> : std::true_type {};

template <typename _Tp, template <typename...> typename _Primary>
concept __specialization_of = __is_specialization_of<_Tp, _Primary>::value;
}  // namespace __detail

namespace __cust_access {
template <typename _Tp>
concept __member_vertices = requires(_Tp & __t) {
    { __t.vertices() } -> std::ranges::input_range;
};

template <typename _Tp>
concept __adl_vertices = requires(_Tp & __t) {
    { vertices(__t) } -> std::ranges::input_range;
};

struct _Vertices {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_vertices<_Tp>)
            return noexcept(std::declval<_Tp &>().vertices());
        else
            return noexcept(vertices(std::declval<_Tp &>()));
    }

public:
    template <typename _Tp>
    requires __member_vertices<_Tp> || __adl_vertices<_Tp>
    constexpr auto operator() [[nodiscard]] (_Tp && __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_vertices<_Tp>)
            return __t.vertices();
        else if constexpr(__adl_vertices<_Tp>)
            return vertices(__t);
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_Vertices vertices{};
}  // namespace __cust

template <typename _Tp>
using vertices_range_t = decltype(melon::vertices(std::declval<_Tp &>()));

template <typename _Tp>
using vertex_t = std::ranges::range_value_t<vertices_range_t<_Tp>>;

namespace __cust_access {
template <typename _Tp>
concept __member_nb_vertices = requires(_Tp & __t) {
    { __t.nb_vertices() } -> std::integral;
};

template <typename _Tp>
concept __adl_nb_vertices = requires(_Tp & __t) {
    { nb_vertices(__t) } -> std::integral;
};

struct _NbVertices {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_vertices<_Tp>)
            return noexcept(std::declval<_Tp &>().nb_vertices());
        else if constexpr(__adl_nb_vertices<_Tp>)
            return noexcept(nb_vertices(std::declval<_Tp &>()));
        else
            return noexcept(
                std::ranges::size(melon::vertices(std::declval<_Tp &>())));
    }

public:
    template <typename _Tp>
    requires __member_nb_vertices<_Tp> || __adl_nb_vertices<_Tp> ||
        std::ranges::sized_range<vertices_range_t<_Tp>>
    constexpr auto operator() [[nodiscard]] (_Tp && __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_nb_vertices<_Tp>)
            return __t.nb_vertices();
        else if constexpr(__adl_nb_vertices<_Tp>)
            return nb_vertices(__t);
        else
            return std::ranges::size(melon::vertices(__t));
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_NbVertices nb_vertices{};
}  // namespace __cust

namespace __cust_access {
template <typename _Tp>
concept __member_out_arcs = requires(_Tp & __t, vertex_t<_Tp> & __v) {
    { __t.out_arcs(__v) } -> std::ranges::input_range;
};

template <typename _Tp>
concept __adl_out_arcs = requires(_Tp & __t, vertex_t<_Tp> & __v) {
    { out_arcs(__t, __v) } -> std::ranges::input_range;
};

struct _OutArcs {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_out_arcs<_Tp>)
            return noexcept(std::declval<_Tp &>().out_arcs(
                std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(out_arcs(std::declval<_Tp &>(),
                                     std::declval<vertex_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
    requires __member_out_arcs<_Tp> || __adl_out_arcs<_Tp>
    constexpr auto operator()
        [[nodiscard]] (_Tp && __t, const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_out_arcs<_Tp>)
            return __t.out_arcs(__v);
        else
            return out_arcs(__t, __v);
    }
};

template <typename _Tp>
concept __member_in_arcs = requires(_Tp & __t, vertex_t<_Tp> & __v) {
    { __t.in_arcs(__v) } -> std::ranges::input_range;
};

template <typename _Tp>
concept __adl_in_arcs = requires(_Tp & __t, vertex_t<_Tp> & __v) {
    { in_arcs(__t, __v) } -> std::ranges::input_range;
};

struct _InArcs {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_in_arcs<_Tp>)
            return noexcept(
                std::declval<_Tp &>().in_arcs(std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(in_arcs(std::declval<_Tp &>(),
                                    std::declval<vertex_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
    requires __member_in_arcs<_Tp> || __adl_in_arcs<_Tp>
    constexpr auto operator()
        [[nodiscard]] (_Tp && __t, const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_in_arcs<_Tp>)
            return __t.in_arcs(__v);
        else
            return in_arcs(__t, __v);
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_OutArcs out_arcs{};
inline constexpr __cust_access::_InArcs in_arcs{};
}  // namespace __cust

template <typename _Tp>
using out_arcs_range_t = decltype(melon::out_arcs(
    std::declval<_Tp &>(), std::declval<vertex_t<_Tp> &>()));

template <typename _Tp>
using in_arcs_range_t = decltype(melon::in_arcs(
    std::declval<_Tp &>(), std::declval<vertex_t<_Tp> &>()));

namespace __cust_access {
template <typename _Tp>
concept __member_arcs = requires(_Tp & __t) {
    { __t.arcs() } -> std::ranges::input_range;
};

template <typename _Tp>
concept __adl_arcs = requires(_Tp & __t) {
    { arcs(__t) } -> std::ranges::input_range;
};

template <typename _Tp, typename _Incidence>
requires requires(_Tp && __t, vertex_t<_Tp> & __v) {
    { _Incidence{}(__t, __v) } -> std::ranges::viewable_range;
}
inline constexpr auto __join_incidence
    [[nodiscard]] (_Tp && __t, _Incidence && __incidence) {
    return std::views::join(std::views::transform(
        melon::vertices(__t),
        [&](const vertex_t<_Tp> & v) { return __incidence(__t, v); }));
}

template <typename _Tp, typename _Incidence>
concept __can_join_incidence = requires(_Tp & __t) {
    __join_incidence(__t, _Incidence{});
};

template <typename _Tp>
inline constexpr int __arcs_range_rank() {
    if constexpr(__detail::__specialization_of<_Tp, std::ranges::iota_view>)
        return 3;
    else if constexpr(std::ranges::contiguous_range<_Tp>)
        return 2;
    else if constexpr(std::ranges::viewable_range<_Tp>)
        return 1;
    else
        return 0;
};

struct _Arcs {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_arcs<_Tp>)
            return noexcept(std::declval<_Tp &>().arcs());
        else if constexpr(__adl_arcs<_Tp>)
            return noexcept(arcs(std::declval<_Tp &>()));
        else if constexpr(__can_join_incidence<_Tp, _OutArcs> &&
                          !__can_join_incidence<_Tp, _InArcs>)
            return noexcept(
                __join_incidence(std::declval<_Tp &>(), _OutArcs{}));
        else if constexpr(__can_join_incidence<_Tp, _InArcs> &&
                          !__can_join_incidence<_Tp, _OutArcs>)
            return noexcept(__join_incidence(std::declval<_Tp &>(), _InArcs{}));
        else {
            if(__arcs_range_rank<out_arcs_range_t<_Tp>>() >
               __arcs_range_rank<in_arcs_range_t<_Tp>>())
                return noexcept(
                    __join_incidence(std::declval<_Tp &>(), _OutArcs{}));
            else
                return noexcept(
                    __join_incidence(std::declval<_Tp &>(), _InArcs{}));
        }
    }

public:
    template <typename _Tp>
    requires __member_arcs<_Tp> || __adl_arcs<_Tp> ||
        __can_join_incidence<_Tp, _OutArcs> ||
        __can_join_incidence<_Tp, _InArcs>
    constexpr auto operator() [[nodiscard]] (_Tp && __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_arcs<_Tp>)
            return __t.arcs();
        else if constexpr(__adl_arcs<_Tp>)
            return arcs(__t);
        else if constexpr(__can_join_incidence<_Tp, _OutArcs> &&
                          !__can_join_incidence<_Tp, _InArcs>)
            return __join_incidence(__t, _OutArcs{});
        else if constexpr(__can_join_incidence<_Tp, _InArcs> &&
                          !__can_join_incidence<_Tp, _OutArcs>)
            return __join_incidence(__t, _InArcs{});
        else {
            if(__arcs_range_rank<out_arcs_range_t<_Tp>>() >
               __arcs_range_rank<in_arcs_range_t<_Tp>>())
                return __join_incidence(__t, _OutArcs{});
            else
                return __join_incidence(__t, _InArcs{});
        }
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_Arcs arcs{};
}  // namespace __cust

template <typename _Tp>
using arcs_range_t = decltype(melon::arcs(std::declval<_Tp &>()));

template <typename _Tp>
using arc_t = std::ranges::range_value_t<arcs_range_t<_Tp>>;

namespace __cust_access {
template <typename _Tp>
concept __member_nb_arcs = requires(_Tp & __t) {
    { __t.nb_arcs() } -> std::integral;
};

template <typename _Tp>
concept __adl_nb_arcs = requires(_Tp & __t) {
    { nb_arcs(__t) } -> std::integral;
};

struct _NbArcs {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_nb_arcs<_Tp>)
            return noexcept(std::declval<_Tp &>().nb_arcs());
        else if constexpr(__adl_nb_arcs<_Tp>)
            return noexcept(nb_arcs(std::declval<_Tp &>()));
        else
            return noexcept(std::ranges::size(_Arcs{}(std::declval<_Tp &>())));
    }

public:
    template <typename _Tp>
    requires __member_nb_arcs<_Tp> || __adl_nb_arcs<_Tp> ||
        std::ranges::sized_range<arcs_range_t<_Tp>>
    constexpr auto operator() [[nodiscard]] (_Tp && __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_nb_arcs<_Tp>)
            return __t.nb_arcs();
        else if constexpr(__adl_nb_arcs<_Tp>)
            return nb_arcs(__t);
        else
            return std::ranges::size(_Arcs{}(__t));
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_NbArcs nb_arcs{};
}  // namespace __cust

namespace __cust_access {
template <typename _Tp>
concept __member_arc_source = requires(_Tp & __t, arc_t<_Tp> & __a) {
    { __t.arc_source(__a) } -> std::convertible_to<vertex_t<_Tp>>;
};

template <typename _Tp>
concept __adl_arc_source = requires(_Tp & __t, arc_t<_Tp> & __a) {
    { arc_source(__t, __a) } -> std::convertible_to<vertex_t<_Tp>>;
};

struct _ArcSource {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_arc_source<_Tp>)
            return noexcept(
                std::declval<_Tp &>().arc_source(std::declval<arc_t<_Tp> &>()));
        else
            return noexcept(arc_source(std::declval<_Tp &>(),
                                       std::declval<arc_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
    requires __member_arc_source<_Tp> || __adl_arc_source<_Tp>
    constexpr auto operator()
        [[nodiscard]] (_Tp && __t, const arc_t<_Tp> & __a) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_arc_source<_Tp>)
            return __t.arc_source(__a);
        else
            return arc_source(__t, __a);
    }
};

template <typename _Tp>
concept __member_arc_target = requires(_Tp & __t, arc_t<_Tp> & __a) {
    { __t.arc_target(__a) } -> std::convertible_to<vertex_t<_Tp>>;
};

template <typename _Tp>
concept __adl_arc_target = requires(_Tp & __t, arc_t<_Tp> & __a) {
    { arc_target(__t, __a) } -> std::convertible_to<vertex_t<_Tp>>;
};

struct _ArcTarget {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_arc_target<_Tp>)
            return noexcept(
                std::declval<_Tp &>().arc_target(std::declval<arc_t<_Tp> &>()));
        else
            return noexcept(arc_target(std::declval<_Tp &>(),
                                       std::declval<arc_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
    requires __member_arc_target<_Tp> || __adl_arc_target<_Tp>
    constexpr auto operator()
        [[nodiscard]] (_Tp && __t, const arc_t<_Tp> & __a) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_arc_target<_Tp>)
            return __t.arc_target(__a);
        else
            return arc_target(__t, __a);
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_ArcSource arc_source{};
inline constexpr __cust_access::_ArcTarget arc_target{};
}  // namespace __cust

namespace __cust_access {
template <typename _Tp>
concept __member_arcs_entries = requires(_Tp & __t) {
    { __t.arcs_entries() } -> std::ranges::input_range;
};

template <typename _Tp>
concept __adl_arcs_entries = requires(_Tp & __t) {
    { arcs_entries(__t) } -> std::ranges::input_range;
};

template <typename _Tp>
requires requires(_Tp && __t, arc_t<_Tp> & __a) {
    { _Arcs{}(__t) } -> std::ranges::viewable_range;
    _ArcSource{}(__t, __a);
    _ArcTarget{}(__t, __a);
}
inline constexpr auto __list_arcs_entries [[nodiscard]] (_Tp && __t) {
    return std::views::transform(melon::arcs(__t), [&](const arc_t<_Tp> & a) {
        return std::make_pair(
            a, std::make_pair(_ArcSource{}(__t, a), _ArcTarget{}(__t, a)));
    });
}

template <typename _Tp>
concept __can_list_arcs_entries = requires(_Tp & __t) {
    __list_arcs_entries(__t);
};

template <typename _Tp>
requires requires(_Tp && __t, arc_t<_Tp> & __a) {
    { _OutArcs{}(__t) } -> std::ranges::viewable_range;
    _ArcTarget{}(__t, __a);
}
inline constexpr auto __join_out_arcs_entries [[nodiscard]] (_Tp && __t) {
    return std::views::join(std::views::transform(
        melon::vertices(__t), [&__t](const vertex_t<_Tp> & s) {
            return std::views::transform(
                melon::out_arcs(__t, s), [&__t, s](const arc_t<_Tp> & a) {
                    return std::make_pair(
                        a, std::make_pair(s, melon::arc_target(__t, a)));
                });
        }));
}

template <typename _Tp>
concept __can_join_out_incidence_entries = requires(_Tp & __t) {
    __join_out_arcs_entries(__t);
};

template <typename _Tp>
requires requires(_Tp && __t, arc_t<_Tp> & __a) {
    { _InArcs{}(__t) } -> std::ranges::viewable_range;
    _ArcSource{}(__t, __a);
}
inline constexpr auto __join_in_arcs_entries [[nodiscard]] (_Tp && __t) {
    return std::views::join(std::views::transform(
        melon::vertices(__t), [&__t](const vertex_t<_Tp> & t) {
            return std::views::transform(
                melon::out_arcs(__t, t), [&__t, t](const arc_t<_Tp> & a) {
                    return std::make_pair(
                        a, std::make_pair(melon::arc_source(__t, a), t));
                });
        }));
}

template <typename _Tp>
concept __can_join_in_arcs_entries = requires(_Tp & __t) {
    __join_in_arcs_entries(__t);
};

template <typename _Tp>
inline constexpr int __arcs_entries_range_rank() {
    if constexpr(__detail::__specialization_of<_Tp, std::ranges::iota_view>)
        return 3;
    else if constexpr(std::ranges::contiguous_range<_Tp>)
        return 2;
    else if constexpr(std::ranges::viewable_range<_Tp>)
        return 1;
    else
        return 0;
};

struct _ArcsEntries {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_arcs_entries<_Tp>)
            return noexcept(std::declval<_Tp &>().arcs_entries());
        else if constexpr(__adl_arcs_entries<_Tp>)
            return noexcept(arcs_entries(std::declval<_Tp &>()));
        else
            return false;
    }

public:
    template <typename _Tp>
    requires __member_arcs_entries<_Tp> || __adl_arcs_entries<_Tp>
    constexpr auto operator() [[nodiscard]] (_Tp && __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_arcs_entries<_Tp>)
            return __t.arcs_entries();
        else if constexpr(__adl_arcs_entries<_Tp>)
            return arcs_entries(__t);
        // else if constexpr(__can_list_arcs_entries<_Tp> &&
        //                   !__can_join_arcs_entries<_Tp>)
        //     return __list_arcs_entries(__t);
        // else if constexpr(__can_join_arcs_entries<_Tp> &&
        //                   !__can_list_arcs_entries<_Tp>)
        //     return __join_arcs_entries(__t);
        // else {
        // }
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_ArcsEntries arcs_entries{};
}  // namespace __cust

namespace __cust_access {
template <typename _Tp, typename _Incidence, typename _EndPoint>
requires requires(_Tp && __t, vertex_t<_Tp> & __v, arc_t<_Tp> & __a) {
    { _Incidence{}(__t, __v) } -> std::ranges::viewable_range;
    _EndPoint{}(__t, __a);
}
inline constexpr auto __list_incidence_endpoints
    [[nodiscard]] (_Tp && __t, const vertex_t<_Tp> & __v,
                   _Incidence && __incidence, _EndPoint && __end_point) {
    return std::views::transform(
        __incidence(__t, __v),
        [&](const arc_t<_Tp> & a) { return __end_point(__t, a); });
}

template <typename _Tp, typename _Incidence, typename _EndPoint>
concept __can_list_incidence_endpoints = requires(_Tp & __t,
                                                  vertex_t<_Tp> & __v) {
    __list_incidence_endpoints(__t, __v, _Incidence{}, _EndPoint{});
};

template <typename _Tp>
concept __member_out_neighbors = requires(_Tp & __t, vertex_t<_Tp> & __v) {
    { __t.out_neighbors(__v) } -> std::ranges::input_range;
    {
        *std::ranges::begin(__t.out_neighbors(__v))
        } -> std::convertible_to<vertex_t<_Tp>>;
};

template <typename _Tp>
concept __adl_out_neighbors = requires(_Tp & __t, vertex_t<_Tp> & __v) {
    { out_neighbors(__t, __v) } -> std::ranges::input_range;
    {
        *std::ranges::begin(out_neighbors(__t, __v))
        } -> std::convertible_to<vertex_t<_Tp>>;
};

struct _OutNeighbors {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_out_neighbors<_Tp>)
            return noexcept(std::declval<_Tp &>().out_neighbors(
                std::declval<vertex_t<_Tp> &>()));
        else if constexpr(__adl_out_neighbors<_Tp>)
            return noexcept(out_neighbors(std::declval<_Tp &>(),
                                          std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(__list_incidence_endpoints(
                std::declval<_Tp &>(), std::declval<vertex_t<_Tp> &>(),
                _OutArcs{}, _ArcTarget{}));
    }

public:
    template <typename _Tp>
    requires __member_out_neighbors<_Tp> || __adl_out_neighbors<_Tp> ||
        __can_list_incidence_endpoints<_Tp, _OutArcs, _ArcTarget>
    constexpr auto operator()
        [[nodiscard]] (_Tp && __t, const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_out_neighbors<_Tp>)
            return __t.out_neighbors(__v);
        else if constexpr(__adl_out_neighbors<_Tp>)
            return out_neighbors(__t, __v);
        else
            return __list_incidence_endpoints(__t, __v, _OutArcs{},
                                              _ArcTarget{});
    }
};

template <typename _Tp>
concept __member_in_neighbors = requires(_Tp & __t, vertex_t<_Tp> __v) {
    { __t.in_neighbors(__v) } -> std::ranges::input_range;
    {
        *std::ranges::begin(__t.in_neighbors(__v))
        } -> std::convertible_to<vertex_t<_Tp>>;
};

template <typename _Tp>
concept __adl_in_neighbors = requires(_Tp & __t, vertex_t<_Tp> __v) {
    { in_neighbors(__t, __v) } -> std::ranges::input_range;
    {
        *std::ranges::begin(in_neighbors(__t, __v))
        } -> std::convertible_to<vertex_t<_Tp>>;
};

struct _InNeighbors {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_in_neighbors<_Tp>)
            return noexcept(std::declval<_Tp &>().in_neighbors(
                std::declval<vertex_t<_Tp> &>()));
        else if constexpr(__adl_in_neighbors<_Tp>)
            return noexcept(in_neighbors(std::declval<_Tp &>(),
                                         std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(__list_incidence_endpoints(
                std::declval<_Tp &>(), std::declval<vertex_t<_Tp> &>(),
                _InArcs{}, _ArcSource{}));
    }

public:
    template <typename _Tp>
    requires __member_in_neighbors<_Tp> || __adl_in_neighbors<_Tp> ||
        __can_list_incidence_endpoints<_Tp, _InArcs, _ArcSource>
    constexpr auto operator()
        [[nodiscard]] (_Tp && __t, const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_in_neighbors<_Tp>)
            return __t.in_neighbors(__v);
        else if constexpr(__adl_in_neighbors<_Tp>)
            return in_neighbors(__t, __v);
        else
            return __list_incidence_endpoints(__t, __v, _InArcs{},
                                              _ArcSource{});
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_OutNeighbors out_neighbors{};
inline constexpr __cust_access::_InNeighbors in_neighbors{};
}  // namespace __cust

template <typename _Tp>
using out_neighbors_range_t = decltype(melon::out_neighbors(
    std::declval<_Tp &>(), std::declval<vertex_t<_Tp> &>()));

template <typename _Tp>
using in_neighbors_range_t = decltype(melon::in_neighbors(
    std::declval<_Tp &>(), std::declval<vertex_t<_Tp> &>()));

template <typename _Tp>
concept graph = requires(_Tp & __t) {
    melon::vertices(__t);
    melon::arcs(__t);
};

template <typename _Tp>
concept copyable_graph = graph<_Tp> && requires(_Tp & __t) {
    melon::arcs_entries(__t);
};

template <typename _Tp>
concept outward_incidence_graph = graph<_Tp> &&
    requires(_Tp & __t, vertex_t<_Tp> & __v, arc_t<_Tp> & __a) {
    melon::out_arcs(__t, __v);
    melon::arc_target(__t, __a);
} && std::convertible_to<std::ranges::range_value_t<out_arcs_range_t<_Tp>>,
                         arc_t<_Tp>>;

template <typename _Tp>
concept inward_incidence_graph = graph<_Tp> &&
    requires(_Tp & __t, vertex_t<_Tp> & __v, arc_t<_Tp> & __a) {
    melon::in_arcs(__t, __v);
    melon::arc_source(__t, __a);
} && std::convertible_to<std::ranges::range_value_t<in_arcs_range_t<_Tp>>,
                         arc_t<_Tp>>;

template <typename _Tp>
concept outward_adjacency_graph = graph<_Tp> &&
    requires(_Tp & __t, vertex_t<_Tp> & __v) {
    melon::out_neighbors(__t, __v);
};

template <typename _Tp>
concept inward_adjacency_graph = graph<_Tp> &&
    requires(_Tp & __t, vertex_t<_Tp> & __v) {
    melon::in_neighbors(__t, __v);
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_GRAPH_HPP