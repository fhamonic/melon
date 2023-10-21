#ifndef MELON_GRAPH_HPP
#define MELON_GRAPH_HPP

#include <concepts>
#include <ranges>
#include <type_traits>

#include "melon/utility/value_map.hpp"

namespace fhamonic {
namespace melon {

namespace __detail {
template <typename _Tp, template <typename...> typename _Primary>
struct __is_specialization_of : std::false_type {};

template <template <typename...> typename _Primary, typename... _Args>
struct __is_specialization_of<_Primary<_Args...>, _Primary> : std::true_type {};

template <typename _Tp, template <typename...> typename _Primary>
concept __specialization_of = __is_specialization_of<_Tp, _Primary>::value;

template <typename _Tp>
inline constexpr int _range_rank() {
    if constexpr(__detail::__specialization_of<_Tp, std::ranges::iota_view>)
        return 3;
    else if constexpr(std::ranges::contiguous_range<_Tp>)
        return 2;
    else if constexpr(std::ranges::viewable_range<_Tp>)
        return 1;
    else
        return 0;
};
}  // namespace __detail

namespace __cust_access {
template <typename _Tp>
concept __member_vertices = requires(const _Tp & __t) {
                                { __t.vertices() } -> std::ranges::input_range;
                            };

template <typename _Tp>
concept __adl_vertices = requires(const _Tp & __t) {
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
    constexpr decltype(auto) operator() [[nodiscard]] (const _Tp & __t) const
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
concept __member_nb_vertices = requires(const _Tp & __t) {
                                   { __t.nb_vertices() } -> std::integral;
                               };

template <typename _Tp>
concept __adl_nb_vertices = requires(const _Tp & __t) {
                                { nb_vertices(__t) } -> std::integral;
                            };

struct _NbVertices {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_nb_vertices<_Tp>)
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
    constexpr auto operator() [[nodiscard]] (const _Tp & __t) const
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
concept __member_out_arcs =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { __t.out_arcs(__v) } -> std::ranges::input_range;
    };

template <typename _Tp>
concept __adl_out_arcs = requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
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
        [[nodiscard]] (const _Tp & __t, const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_out_arcs<_Tp>)
            return __t.out_arcs(__v);
        else
            return out_arcs(__t, __v);
    }
};

template <typename _Tp>
concept __member_in_arcs =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { __t.in_arcs(__v) } -> std::ranges::input_range;
    };

template <typename _Tp>
concept __adl_in_arcs = requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
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
        [[nodiscard]] (const _Tp & __t, const vertex_t<_Tp> & __v) const
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
using out_arcs_iterator_t = std::ranges::iterator_t<out_arcs_range_t<_Tp>>;

template <typename _Tp>
using out_arcs_sentinel_t = std::ranges::sentinel_t<out_arcs_range_t<_Tp>>;

template <typename _Tp>
using in_arcs_range_t = decltype(melon::in_arcs(
    std::declval<_Tp &>(), std::declval<vertex_t<_Tp> &>()));

template <typename _Tp>
using in_arcs_iterator_t = std::ranges::iterator_t<in_arcs_range_t<_Tp>>;

template <typename _Tp>
using in_arcs_sentinel_t = std::ranges::sentinel_t<in_arcs_range_t<_Tp>>;

namespace __cust_access {
template <typename _Tp>
concept __member_arcs = requires(const _Tp & __t) {
                            { __t.arcs() } -> std::ranges::input_range;
                        };

template <typename _Tp>
concept __adl_arcs = requires(const _Tp & __t) {
                         { arcs(__t) } -> std::ranges::input_range;
                     };

template <typename _Tp, typename _Incidence>
    requires requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
                 { _Incidence{}(__t, __v) } -> std::ranges::viewable_range;
             }
inline constexpr auto __join_incidence
    [[nodiscard]] (const _Tp & __t, _Incidence && __incidence) {
    return std::views::join(std::views::transform(
        melon::vertices(__t),
        [&](const vertex_t<_Tp> & v) { return __incidence(__t, v); }));
}

template <typename _Tp, typename _Incidence>
concept __can_join_incidence =
    requires(const _Tp & __t) { __join_incidence(__t, _Incidence{}); };

struct _Arcs {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_arcs<_Tp>)
            return noexcept(std::declval<_Tp &>().arcs());
        else if constexpr(__adl_arcs<_Tp>)
            return noexcept(arcs(std::declval<_Tp &>()));
        else
            return false;
    }

public:
    template <typename _Tp>
        requires __member_arcs<_Tp> || __adl_arcs<_Tp> ||
                 __can_join_incidence<_Tp, _OutArcs> ||
                 __can_join_incidence<_Tp, _InArcs>
    constexpr auto operator() [[nodiscard]] (const _Tp & __t) const
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
            if(__detail::_range_rank<out_arcs_range_t<_Tp>>() >
               __detail::_range_rank<in_arcs_range_t<_Tp>>())
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
concept __member_out_degree =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { __t.out_degree(__v) } -> std::integral;
    };

template <typename _Tp>
concept __adl_out_degree =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { out_degree(__t, __v) } -> std::integral;
    };

template <typename _Tp>
concept __has_sized_out_arcs =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { _OutArcs{}(__t, __v) } -> std::ranges::sized_range;
    };

struct _OutDegree {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_out_degree<_Tp>)
            return noexcept(std::declval<_Tp &>().out_degree(
                std::declval<vertex_t<_Tp> &>()));
        else if constexpr(__adl_out_degree<_Tp>)
            return noexcept(out_degree(std::declval<_Tp &>(),
                                       std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(std::ranges::size(melon::out_arcs(
                std::declval<_Tp &>(), std::declval<vertex_t<_Tp> &>())));
    }

public:
    template <typename _Tp>
        requires __member_out_degree<_Tp> || __adl_out_degree<_Tp> ||
                 __has_sized_out_arcs<_Tp>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_out_degree<_Tp>)
            return __t.out_degree(__v);
        else if constexpr(__adl_out_degree<_Tp>)
            return out_degree(__t, __v);
        else
            return std::ranges::size(melon::out_arcs(__t, __v));
    }
};

template <typename _Tp>
concept __member_in_degree =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { __t.in_degree(__v) } -> std::integral;
    };

template <typename _Tp>
concept __adl_in_degree = requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
                              { in_degree(__t, __v) } -> std::integral;
                          };

template <typename _Tp>
concept __has_sized_in_arcs =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { _InArcs{}(__t, __v) } -> std::ranges::sized_range;
    };

struct _InDegree {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_in_degree<_Tp>)
            return noexcept(std::declval<_Tp &>().in_degree(
                std::declval<vertex_t<_Tp> &>()));
        else if constexpr(__adl_in_degree<_Tp>)
            return noexcept(in_degree(std::declval<_Tp &>(),
                                      std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(std::ranges::size(melon::in_arcs(
                std::declval<_Tp &>(), std::declval<vertex_t<_Tp> &>())));
    }

public:
    template <typename _Tp>
        requires __member_in_degree<_Tp> || __adl_in_degree<_Tp> ||
                 __has_sized_in_arcs<_Tp>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_in_degree<_Tp>)
            return __t.in_degree(__v);
        else if constexpr(__adl_in_degree<_Tp>)
            return in_degree(__t, __v);
        else
            return std::ranges::size(melon::in_arcs(__t, __v));
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_OutDegree out_degree{};
inline constexpr __cust_access::_InDegree in_degree{};
}  // namespace __cust

namespace __cust_access {
template <typename _Tp>
concept __member_nb_arcs = requires(const _Tp & __t) {
                               { __t.nb_arcs() } -> std::integral;
                           };

template <typename _Tp>
concept __adl_nb_arcs = requires(const _Tp & __t) {
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
    constexpr auto operator() [[nodiscard]] (const _Tp & __t) const
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
concept __member_arc_source =
    requires(const _Tp & __t, const arc_t<_Tp> & __a) {
        { __t.arc_source(__a) } -> std::convertible_to<vertex_t<_Tp>>;
    };

template <typename _Tp>
concept __adl_arc_source = requires(const _Tp & __t, const arc_t<_Tp> & __a) {
                               {
                                   arc_source(__t, __a)
                                   } -> std::convertible_to<vertex_t<_Tp>>;
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
        [[nodiscard]] (const _Tp & __t, const arc_t<_Tp> & __a) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_arc_source<_Tp>)
            return __t.arc_source(__a);
        else
            return arc_source(__t, __a);
    }
};

template <typename _Tp>
concept __member_arc_target =
    requires(const _Tp & __t, const arc_t<_Tp> & __a) {
        { __t.arc_target(__a) } -> std::convertible_to<vertex_t<_Tp>>;
    };

template <typename _Tp>
concept __adl_arc_target = requires(const _Tp & __t, const arc_t<_Tp> & __a) {
                               {
                                   arc_target(__t, __a)
                                   } -> std::convertible_to<vertex_t<_Tp>>;
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
        [[nodiscard]] (const _Tp & __t, const arc_t<_Tp> & __a) const
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
concept __member_arcs_entries = requires(const _Tp & __t) {
                                    {
                                        __t.arcs_entries()
                                        } -> std::ranges::input_range;
                                };

template <typename _Tp>
concept __adl_arcs_entries = requires(const _Tp & __t) {
                                 {
                                     arcs_entries(__t)
                                     } -> std::ranges::input_range;
                             };

template <typename _Tp>
    requires requires(const _Tp & __t, const arc_t<_Tp> & __a) {
                 { _Arcs{}(__t) } -> std::ranges::viewable_range;
                 _ArcSource{}(__t, __a);
                 _ArcTarget{}(__t, __a);
             }
inline constexpr auto __list_arcs_entries [[nodiscard]] (const _Tp & __t) {
    return std::views::transform(melon::arcs(__t), [&](const arc_t<_Tp> & a) {
        return std::make_pair(
            a, std::make_pair(_ArcSource{}(__t, a), _ArcTarget{}(__t, a)));
    });
}

template <typename _Tp>
concept __can_list_arcs_entries =
    requires(const _Tp & __t) { __list_arcs_entries(__t); };

template <typename _Tp>
    requires requires(const _Tp & __t, const vertex_t<_Tp> & __v,
                      arc_t<_Tp> & __a) {
                 { _OutArcs{}(__t, __v) } -> std::ranges::viewable_range;
                 _ArcTarget{}(__t, __a);
             }
inline constexpr auto __join_out_arcs_entries [[nodiscard]] (const _Tp & __t) {
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
concept __can_join_out_arcs_entries =
    requires(const _Tp & __t) { __join_out_arcs_entries(__t); };

template <typename _Tp>
    requires requires(const _Tp & __t, const vertex_t<_Tp> & __v,
                      arc_t<_Tp> & __a) {
                 { _InArcs{}(__t, __v) } -> std::ranges::viewable_range;
                 _ArcSource{}(__t, __a);
             }
inline constexpr auto __join_in_arcs_entries [[nodiscard]] (const _Tp & __t) {
    return std::views::join(std::views::transform(
        melon::vertices(__t), [&__t](const vertex_t<_Tp> & t) {
            return std::views::transform(
                melon::in_arcs(__t, t), [&__t, t](const arc_t<_Tp> & a) {
                    return std::make_pair(
                        a, std::make_pair(melon::arc_source(__t, a), t));
                });
        }));
}

template <typename _Tp>
concept __can_join_in_arcs_entries =
    requires(const _Tp & __t) { __join_in_arcs_entries(__t); };

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
        requires __member_arcs_entries<_Tp> || __adl_arcs_entries<_Tp> ||
                 __can_list_arcs_entries<_Tp> ||
                 __can_join_out_arcs_entries<_Tp> ||
                 __can_join_in_arcs_entries<_Tp>
    constexpr auto operator() [[nodiscard]] (const _Tp & __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_arcs_entries<_Tp>)
            return __t.arcs_entries();
        else if constexpr(__adl_arcs_entries<_Tp>)
            return arcs_entries(__t);
        else if constexpr(!__can_join_out_arcs_entries<_Tp> &&
                          !__can_join_in_arcs_entries<_Tp>)
            return __list_arcs_entries(__t);
        else if constexpr(__can_join_out_arcs_entries<_Tp> &&
                          !__can_join_in_arcs_entries<_Tp>) {
            if constexpr(__can_list_arcs_entries<_Tp> &&
                         __detail::_range_rank<arcs_range_t<_Tp>>() >=
                             __detail::_range_rank<out_arcs_range_t<_Tp>>())
                return __list_arcs_entries(__t);
            else
                return __join_out_arcs_entries(__t);
        } else if constexpr(!__can_join_out_arcs_entries<_Tp> &&
                            __can_join_in_arcs_entries<_Tp>) {
            if constexpr(__can_list_arcs_entries<_Tp> &&
                         __detail::_range_rank<arcs_range_t<_Tp>>() >=
                             __detail::_range_rank<in_arcs_range_t<_Tp>>())
                return __list_arcs_entries(__t);
            else
                return __join_in_arcs_entries(__t);
        } else {
            if constexpr(__can_list_arcs_entries<_Tp> &&
                         __detail::_range_rank<arcs_range_t<_Tp>>() >=
                             std::max(
                                 __detail::_range_rank<out_arcs_range_t<_Tp>>(),
                                 __detail::_range_rank<in_arcs_range_t<_Tp>>()))
                return __list_arcs_entries(__t);
            else if constexpr(__detail::_range_rank<out_arcs_range_t<_Tp>>() >=
                              __detail::_range_rank<in_arcs_range_t<_Tp>>())
                return __join_out_arcs_entries(__t);
            else
                return __join_in_arcs_entries(__t);
        }
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_ArcsEntries arcs_entries{};
}  // namespace __cust

namespace __cust_access {
template <typename _Tp, typename _Incidence, typename _EndPoint>
    requires requires(const _Tp & __t, const vertex_t<_Tp> & __v,
                      arc_t<_Tp> & __a) {
                 { _Incidence{}(__t, __v) } -> std::ranges::viewable_range;
                 _EndPoint{}(__t, __a);
             }
inline constexpr auto __list_incidence_endpoints
    [[nodiscard]] (const _Tp & __t, const vertex_t<_Tp> & __v,
                   _Incidence && __incidence, _EndPoint && __end_point) {
    return std::views::transform(
        __incidence(__t, __v),
        [&](const arc_t<_Tp> & a) { return __end_point(__t, a); });
}

template <typename _Tp, typename _Incidence, typename _EndPoint>
concept __can_list_incidence_endpoints =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        __list_incidence_endpoints(__t, __v, _Incidence{}, _EndPoint{});
    };

template <typename _Tp>
concept __member_out_neighbors =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { __t.out_neighbors(__v) } -> std::ranges::input_range;
        {
            *std::ranges::begin(__t.out_neighbors(__v))
            } -> std::convertible_to<vertex_t<_Tp>>;
    };

template <typename _Tp>
concept __adl_out_neighbors =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
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
            return false;
    }

public:
    template <typename _Tp>
        requires __member_out_neighbors<_Tp> || __adl_out_neighbors<_Tp> ||
                 __can_list_incidence_endpoints<_Tp, _OutArcs, _ArcTarget>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const vertex_t<_Tp> & __v) const
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
concept __member_in_neighbors =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { __t.in_neighbors(__v) } -> std::ranges::input_range;
        {
            *std::ranges::begin(__t.in_neighbors(__v))
            } -> std::convertible_to<vertex_t<_Tp>>;
    };

template <typename _Tp>
concept __adl_in_neighbors =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
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
            return false;
    }

public:
    template <typename _Tp>
        requires __member_in_neighbors<_Tp> || __adl_in_neighbors<_Tp> ||
                 __can_list_incidence_endpoints<_Tp, _InArcs, _ArcSource>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const vertex_t<_Tp> & __v) const
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
concept graph = requires(const _Tp & __t) {
                    melon::vertices(__t);
                    melon::arcs(__t);
                    melon::arcs_entries(__t);
                };

template <typename _Tp>
concept has_nb_vertices =
    graph<_Tp> && requires(const _Tp & __t) { melon::nb_vertices(__t); };

template <typename _Tp>
concept has_nb_arcs =
    graph<_Tp> && requires(const _Tp & __t) { melon::nb_arcs(__t); };

template <typename _Tp>
concept has_arc_target =
    graph<_Tp> && requires(const _Tp & __t, const arc_t<_Tp> & __a) {
                      melon::arc_target(__t, __a);
                  };

template <typename _Tp>
concept has_arc_source =
    graph<_Tp> && requires(const _Tp & __t, const arc_t<_Tp> & __a) {
                      melon::arc_source(__t, __a);
                  };

template <typename _Tp>
concept has_out_arcs =
    graph<_Tp> &&
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        melon::out_arcs(__t, __v);
    } &&
    std::convertible_to<std::ranges::range_value_t<out_arcs_range_t<_Tp>>,
                        arc_t<_Tp>>;

template <typename _Tp>
concept has_in_arcs =
    graph<_Tp> &&
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        melon::in_arcs(__t, __v);
    } &&
    std::convertible_to<std::ranges::range_value_t<in_arcs_range_t<_Tp>>,
                        arc_t<_Tp>>;

template <typename _Tp>
concept has_out_degree =
    graph<_Tp> && requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
                      melon::out_degree(__t, __v);
                  };

template <typename _Tp>
concept has_in_degree =
    graph<_Tp> && requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
                      melon::in_degree(__t, __v);
                  };

template <typename _Tp>
concept outward_incidence_graph =
    graph<_Tp> && has_out_arcs<_Tp> && has_arc_target<_Tp>;

template <typename _Tp>
concept inward_incidence_graph =
    graph<_Tp> && has_in_arcs<_Tp> && has_arc_source<_Tp>;

template <typename _Tp>
concept outward_adjacency_graph =
    graph<_Tp> && requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
                      melon::out_neighbors(__t, __v);
                  };

template <typename _Tp>
concept inward_adjacency_graph =
    graph<_Tp> && requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
                      melon::in_neighbors(__t, __v);
                  };

namespace __cust_access {
template <typename _Tp>
concept __member_create_vertex =
    requires(_Tp & __t) {
        { __t.create_vertex() } -> std::convertible_to<vertex_t<_Tp>>;
    };

template <typename _Tp>
concept __adl_create_vertex = requires(_Tp & __t) {
                                  {
                                      create_vertex(__t)
                                      } -> std::convertible_to<vertex_t<_Tp>>;
                              };

struct _CreateVertex {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_create_vertex<_Tp>)
            return noexcept(std::declval<_Tp &>().create_vertex());
        else
            return noexcept(create_vertex(std::declval<_Tp &>()));
    }

public:
    template <typename _Tp>
        requires __member_create_vertex<_Tp> || __adl_create_vertex<_Tp>
    constexpr auto operator() [[nodiscard]] (_Tp & __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_create_vertex<_Tp>)
            return __t.create_vertex();
        else
            return create_vertex(__t);
    }
};

template <typename _Tp>
concept __member_remove_vertex =
    requires(_Tp & __t, const vertex_t<_Tp> & __v) { __t.remove_vertex(__v); };

template <typename _Tp>
concept __adl_remove_vertex =
    requires(_Tp & __t, const vertex_t<_Tp> & __v) { remove_vertex(__t, __v); };

struct _RemoveVertex {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_remove_vertex<_Tp>)
            return noexcept(std::declval<_Tp &>().remove_vertex(
                std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(remove_vertex(std::declval<_Tp &>(),
                                          std::declval<vertex_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_remove_vertex<_Tp> || __adl_remove_vertex<_Tp>
    constexpr auto operator()
        [[nodiscard]] (_Tp & __t, const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_remove_vertex<_Tp>)
            return __t.remove_vertex(__v);
        else
            return remove_vertex(__t, __v);
    }
};

template <typename _Tp>
concept __member_is_valid_vertex =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { __t.is_valid_vertex(__v) } -> std::convertible_to<vertex_t<_Tp>>;
    };

template <typename _Tp>
concept __adl_is_valid_vertex =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        { is_valid_vertex(__t, __v) } -> std::convertible_to<vertex_t<_Tp>>;
    };

struct _IsValidVertex {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_is_valid_vertex<_Tp>)
            return noexcept(std::declval<_Tp &>().is_valid_vertex(
                std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(is_valid_vertex(std::declval<_Tp &>(),
                                            std::declval<vertex_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_is_valid_vertex<_Tp> || __adl_is_valid_vertex<_Tp>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_is_valid_vertex<_Tp>)
            return __t.is_valid_vertex(__v);
        else
            return is_valid_vertex(__t, __v);
    }
};

template <typename _Tp>
concept __member_create_arc = requires(_Tp & __t, const vertex_t<_Tp> & __v) {
                                  {
                                      __t.create_arc(__v, __v)
                                      } -> std::convertible_to<arc_t<_Tp>>;
                              };

template <typename _Tp>
concept __adl_create_arc = requires(_Tp & __t, const vertex_t<_Tp> & __v) {
                               {
                                   create_arc(__t, __v, __v)
                                   } -> std::convertible_to<arc_t<_Tp>>;
                           };

struct _CreateArc {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_create_arc<_Tp>)
            return noexcept(std::declval<_Tp &>().create_arc(
                std::declval<vertex_t<_Tp> &>(),
                std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(create_arc(std::declval<_Tp &>(),
                                       std::declval<vertex_t<_Tp> &>(),
                                       std::declval<vertex_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_create_arc<_Tp> || __adl_create_arc<_Tp>
    constexpr auto operator()
        [[nodiscard]] (_Tp & __t, const vertex_t<_Tp> & __u,
                       const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_create_arc<_Tp>)
            return __t.create_arc(__u, __v);
        else
            return create_arc(__t, __u, __v);
    }
};

template <typename _Tp>
concept __member_remove_arc =
    requires(_Tp & __t, const arc_t<_Tp> & __a) { __t.remove_arc(__a); };

template <typename _Tp>
concept __adl_remove_arc =
    requires(_Tp & __t, const arc_t<_Tp> & __a) { remove_arc(__t, __a); };

struct _RemoveArc {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_remove_arc<_Tp>)
            return noexcept(
                std::declval<_Tp &>().remove_arc(std::declval<arc_t<_Tp> &>()));
        else
            return noexcept(remove_arc(std::declval<_Tp &>(),
                                       std::declval<arc_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_remove_arc<_Tp> || __adl_remove_arc<_Tp>
    constexpr auto operator()
        [[nodiscard]] (_Tp & __t, const arc_t<_Tp> & __a) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_remove_arc<_Tp>)
            return __t.remove_arc(__a);
        else
            return remove_arc(__t, __a);
    }
};

template <typename _Tp>
concept __member_is_valid_arc =
    requires(const _Tp & __t, const arc_t<_Tp> & __a) {
        { __t.is_valid_arc(__a) } -> std::convertible_to<arc_t<_Tp>>;
    };

template <typename _Tp>
concept __adl_is_valid_arc = requires(const _Tp & __t, const arc_t<_Tp> & __a) {
                                 {
                                     is_valid_arc(__t, __a)
                                     } -> std::convertible_to<arc_t<_Tp>>;
                             };

struct _IsValidArc {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_is_valid_arc<_Tp>)
            return noexcept(std::declval<_Tp &>().is_valid_arc(
                std::declval<arc_t<_Tp> &>()));
        else
            return noexcept(is_valid_arc(std::declval<_Tp &>(),
                                         std::declval<arc_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_is_valid_arc<_Tp> || __adl_is_valid_arc<_Tp>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const arc_t<_Tp> & __a) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_is_valid_arc<_Tp>)
            return __t.is_valid_arc(__a);
        else
            return is_valid_arc(__t, __a);
    }
};

template <typename _Tp>
concept __member_change_arc_source =
    requires(_Tp & __t, const arc_t<_Tp> & __a, const vertex_t<_Tp> & __v) {
        __t.change_arc_source(__a, __v);
    };

template <typename _Tp>
concept __adl_change_arc_source =
    requires(_Tp & __t, const arc_t<_Tp> & __a, const vertex_t<_Tp> & __v) {
        change_arc_source(__t, __a, __v);
    };

struct _ChangeArcSource {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_change_arc_source<_Tp>)
            return noexcept(std::declval<_Tp &>().change_arc_source(
                std::declval<arc_t<_Tp> &>(), std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(change_arc_source(std::declval<_Tp &>(),
                                              std::declval<arc_t<_Tp> &>(),
                                              std::declval<vertex_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_change_arc_source<_Tp> || __adl_change_arc_source<_Tp>
    constexpr auto operator() [[nodiscard]] (_Tp & __t, const arc_t<_Tp> & __a,
                                             const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_change_arc_source<_Tp>)
            return __t.change_arc_source(__a, __v);
        else
            return change_arc_source(__t, __a, __v);
    }
};

template <typename _Tp>
concept __member_change_arc_target =
    requires(_Tp & __t, const arc_t<_Tp> & __a, const vertex_t<_Tp> & __v) {
        __t.change_arc_target(__a, __v);
    };

template <typename _Tp>
concept __adl_change_arc_target =
    requires(_Tp & __t, const arc_t<_Tp> & __a, const vertex_t<_Tp> & __v) {
        change_arc_target(__t, __a, __v);
    };

struct _ChangeArcTarget {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_change_arc_target<_Tp>)
            return noexcept(std::declval<_Tp &>().change_arc_target(
                std::declval<arc_t<_Tp> &>(), std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(change_arc_target(std::declval<_Tp &>(),
                                              std::declval<arc_t<_Tp> &>(),
                                              std::declval<vertex_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_change_arc_target<_Tp> || __adl_change_arc_target<_Tp>
    constexpr auto operator() [[nodiscard]] (_Tp & __t, const arc_t<_Tp> & __a,
                                             const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_change_arc_target<_Tp>)
            return __t.change_arc_target(__a, __v);
        else
            return change_arc_target(__t, __a, __v);
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_CreateVertex create_vertex{};
inline constexpr __cust_access::_RemoveVertex remove_vertex{};
inline constexpr __cust_access::_IsValidVertex is_valid_vertex{};
inline constexpr __cust_access::_CreateArc create_arc{};
inline constexpr __cust_access::_RemoveArc remove_arc{};
inline constexpr __cust_access::_IsValidArc is_valid_arc{};
inline constexpr __cust_access::_ChangeArcTarget change_arc_target{};
inline constexpr __cust_access::_ChangeArcSource change_arc_source{};
}  // namespace __cust

template <typename G>
concept has_vertex_creation =
    graph<G> && requires(G g) {
                    { melon::create_vertex(g) } -> std::same_as<vertex_t<G>>;
                };
template <typename G>
concept has_vertex_removal = graph<G> && requires(G g, vertex_t<G> v) {
                                             melon::remove_vertex(g, v);
                                             {
                                                 melon::is_valid_vertex(g, v)
                                                 } -> std::convertible_to<bool>;
                                         };

template <typename G>
concept has_arc_creation = graph<G> && requires(G g, vertex_t<G> v) {
                                           {
                                               melon::create_arc(g, v, v)
                                               } -> std::same_as<arc_t<G>>;
                                       };
template <typename G>
concept has_arc_removal = graph<G> && requires(G g, arc_t<G> a) {
                                          remove_arc(g, a);
                                          {
                                              melon::is_valid_arc(g, a)
                                              } -> std::convertible_to<bool>;
                                      };

template <typename G>
concept has_change_arc_source =
    graph<G> && requires(G g, arc_t<G> a, vertex_t<G> s) {
                    melon::change_arc_source(g, a, s);
                };
template <typename G>
concept has_change_arc_target =
    graph<G> && requires(G g, arc_t<G> a, vertex_t<G> t) {
                    melon::change_arc_target(g, a, t);
                };

namespace __cust_access {
template <typename _Tp>
concept __member_arc_sources_map = requires(const _Tp & __t) {
                                       { __t.arc_sources_map() };
                                       // -> input_value_map<arc_t<_Tp>,
                                       // vertex_t<_Tp>>;
                                   };

template <typename _Tp>
concept __adl_arc_sources_map = requires(const _Tp & __t) {
                                    { arc_sources_map(__t) };
                                    // -> input_value_map<arc_t<_Tp>,
                                    // vertex_t<_Tp>>;
                                };

struct _ArcSourcesMap {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_arc_sources_map<_Tp>)
            return noexcept(std::declval<_Tp &>().arc_sources_map());
        else if constexpr(__adl_arc_sources_map<_Tp>)
            return noexcept(arc_sources_map(std::declval<_Tp &>()));
        else
            return noexcept(melon::arc_source(std::declval<_Tp &>(),
                                              std::declval<arc_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_arc_sources_map<_Tp> || __adl_arc_sources_map<_Tp> ||
                 has_arc_source<_Tp>
    constexpr auto operator() [[nodiscard]] (const _Tp & __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_arc_sources_map<_Tp>)
            return __t.arc_sources_map();
        else if constexpr(__adl_arc_sources_map<_Tp>)
            return arc_sources_map(__t);
        else
            return views::map([&__t](const arc_t<_Tp> & a) {
                return melon::arc_source(__t, a);
            });
    }
};

template <typename _Tp>
concept __member_arc_targets_map =
    requires(const _Tp & __t) {
        {
            __t.arc_targets_map()
            } -> input_value_map_of<arc_t<_Tp>, vertex_t<_Tp>>;
    };

template <typename _Tp>
concept __adl_arc_targets_map =
    requires(const _Tp & __t) {
        {
            arc_targets_map(__t)
            } -> input_value_map_of<arc_t<_Tp>, vertex_t<_Tp>>;
    };

struct _ArcTargetsMap {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_arc_targets_map<_Tp>)
            return noexcept(std::declval<_Tp &>().arc_targets_map());
        else if constexpr(__adl_arc_targets_map<_Tp>)
            return noexcept(arc_targets_map(std::declval<_Tp &>()));
        else
            return noexcept(melon::arc_target(std::declval<_Tp &>(),
                                              std::declval<arc_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_arc_targets_map<_Tp> || __adl_arc_targets_map<_Tp> ||
                 has_arc_target<_Tp>
    constexpr auto operator() [[nodiscard]] (const _Tp & __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_arc_targets_map<_Tp>)
            return __t.arc_targets_map();
        else if constexpr(__adl_arc_targets_map<_Tp>)
            return arc_targets_map(__t);
        else
            return views::map([&__t](const arc_t<_Tp> & a) {
                return melon::arc_target(__t, a);
            });
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_ArcSourcesMap arc_sources_map{};
inline constexpr __cust_access::_ArcTargetsMap arc_targets_map{};
}  // namespace __cust

namespace __cust_access {
template <typename _Tp, typename _ValueType>
concept __member_create_vertex_map =
    requires(const _Tp & __t, const _ValueType & __d) {
        {
            __t.template create_vertex_map<_ValueType>()
            } -> output_value_map_of<vertex_t<_Tp>, _ValueType>;
        {
            __t.template create_vertex_map<_ValueType>(__d)
            } -> output_value_map_of<vertex_t<_Tp>, _ValueType>;
    };

template <typename _Tp, typename _ValueType>
concept __adl_create_vertex_map =
    requires(const _Tp & __t, const _ValueType & __d) {
        {
            create_vertex_map<_ValueType>(__t)
            } -> output_value_map_of<vertex_t<_Tp>, _ValueType>;
        {
            create_vertex_map<_ValueType>(__t, __d)
            } -> output_value_map_of<vertex_t<_Tp>, _ValueType>;
    };

struct _CreateVertexMap {
private:
    template <typename _ValueType, typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_create_vertex_map<_Tp, _ValueType>)
            return noexcept(
                std::declval<_Tp &>().template create_vertex_map<_ValueType>());
        else
            return noexcept(
                create_vertex_map<_ValueType>(std::declval<_Tp &>()));
    }

public:
    template <typename _ValueType, typename _Tp>
        requires __member_create_vertex_map<_Tp, _ValueType> ||
                 __adl_create_vertex_map<_Tp, _ValueType>
    constexpr auto operator() [[nodiscard]] (const _Tp & __t) const
        noexcept(_S_noexcept<_ValueType, _Tp &>()) {
        if constexpr(__member_create_vertex_map<_Tp, _ValueType>)
            return __t.template create_vertex_map<_ValueType>();
        else
            return create_vertex_map<_ValueType>(__t);
    }

    template <typename _ValueType, typename _Tp>
        requires __member_create_vertex_map<_Tp, _ValueType> ||
                 __adl_create_vertex_map<_Tp, _ValueType>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const _ValueType & __d) const
        noexcept(_S_noexcept<_ValueType, _Tp &>()) {
        if constexpr(__member_create_vertex_map<_Tp, _ValueType>)
            return __t.template create_vertex_map<_ValueType>(__d);
        else
            return create_vertex_map<_ValueType>(__t, __d);
    }
};

template <typename _Tp, typename _ValueType>
concept __member_create_arc_map =
    requires(const _Tp & __t, const _ValueType & __d) {
        {
            __t.template create_arc_map<_ValueType>()
            } -> output_value_map_of<arc_t<_Tp>, _ValueType>;
        {
            __t.template create_arc_map<_ValueType>(__d)
            } -> output_value_map_of<arc_t<_Tp>, _ValueType>;
    };

template <typename _Tp, typename _ValueType>
concept __adl_create_arc_map =
    requires(const _Tp & __t, const _ValueType & __d) {
        {
            create_arc_map<_ValueType>(__t)
            } -> output_value_map_of<arc_t<_Tp>, _ValueType>;
        {
            create_arc_map<_ValueType>(__t, __d)
            } -> output_value_map_of<arc_t<_Tp>, _ValueType>;
    };

struct _CreateArcMap {
private:
    template <typename _ValueType, typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_create_arc_map<_Tp, _ValueType>)
            return noexcept(
                std::declval<_Tp &>().template create_arc_map<_ValueType>());
        else
            return noexcept(create_arc_map<_ValueType>(std::declval<_Tp &>()));
    }

public:
    template <typename _ValueType, typename _Tp>
        requires __member_create_arc_map<_Tp, _ValueType> ||
                 __adl_create_arc_map<_Tp, _ValueType>
    constexpr auto operator() [[nodiscard]] (const _Tp & __t) const
        noexcept(_S_noexcept<_ValueType, _Tp &>()) {
        if constexpr(__member_create_arc_map<_Tp, _ValueType>)
            return __t.template create_arc_map<_ValueType>();
        else
            return create_arc_map<_ValueType>(__t);
    }

    template <typename _ValueType, typename _Tp>
        requires __member_create_arc_map<_Tp, _ValueType> ||
                 __adl_create_arc_map<_Tp, _ValueType>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const _ValueType & __d) const
        noexcept(_S_noexcept<_ValueType, _Tp &>()) {
        if constexpr(__member_create_arc_map<_Tp, _ValueType>)
            return __t.template create_arc_map<_ValueType>(__d);
        else
            return create_arc_map<_ValueType>(__t, __d);
    }
};
}  // namespace __cust_access

inline namespace __cust {
template <typename _ValueType, typename _Tp>
    requires requires(const _Tp & __t) {
                 __cust_access::_CreateVertexMap{}
                     .template operator()<_ValueType>(__t);
             }
inline constexpr auto create_vertex_map(const _Tp & __t) noexcept(
    noexcept(__cust_access::_CreateVertexMap{}.template operator()<_ValueType>(
        std::declval<_Tp &>()))) {
    return __cust_access::_CreateVertexMap{}.template operator()<_ValueType>(
        __t);
}
template <typename _ValueType, typename _Tp>
    requires requires(const _Tp & __t, const _ValueType & __d) {
                 __cust_access::_CreateVertexMap{}
                     .template operator()<_ValueType>(__t, __d);
             }
inline constexpr auto
create_vertex_map(const _Tp & __t, const _ValueType & __d) noexcept(
    noexcept(__cust_access::_CreateVertexMap{}.template operator()<_ValueType>(
        std::declval<_Tp &>(), std::declval<_ValueType &>()))) {
    return __cust_access::_CreateVertexMap{}.template operator()<_ValueType>(
        __t, __d);
}
template <typename _ValueType, typename _Tp>
    requires requires(const _Tp & __t) {
                 __cust_access::_CreateArcMap{}.template operator()<_ValueType>(
                     __t);
             }
inline constexpr auto create_arc_map(const _Tp & __t) noexcept(
    noexcept(__cust_access::_CreateArcMap{}.template operator()<_ValueType>(
        std::declval<_Tp &>()))) {
    return __cust_access::_CreateArcMap{}.template operator()<_ValueType>(__t);
}
template <typename _ValueType, typename _Tp>
    requires requires(const _Tp & __t, const _ValueType & __d) {
                 __cust_access::_CreateArcMap{}.template operator()<_ValueType>(
                     __t, __d);
             }
inline constexpr auto
create_arc_map(const _Tp & __t, const _ValueType & __d) noexcept(
    noexcept(__cust_access::_CreateArcMap{}.template operator()<_ValueType>(
        std::declval<_Tp &>(), std::declval<_ValueType &>()))) {
    return __cust_access::_CreateArcMap{}.template operator()<_ValueType>(__t,
                                                                          __d);
}
}  // namespace __cust

template <typename _Tp, typename _ValueType>
using vertex_map_t =
    decltype(melon::create_vertex_map<_ValueType>(std::declval<_Tp &>()));
template <typename _Tp, typename _ValueType>
using arc_map_t =
    decltype(melon::create_arc_map<_ValueType>(std::declval<_Tp &>()));

template <typename _Tp, typename _ValueType = std::size_t>
concept has_vertex_map =
    graph<_Tp> && requires(const _Tp & __t, const _ValueType & __d) {
                      melon::create_vertex_map<_ValueType>(__t);
                      melon::create_vertex_map<_ValueType>(__t, __d);
                  };

template <typename _Tp, typename _ValueType = std::size_t>
concept has_arc_map =
    graph<_Tp> && requires(const _Tp & __t, const _ValueType & __d) {
                      melon::create_arc_map<_ValueType>(__t);
                      melon::create_arc_map<_ValueType>(__t, __d);
                  };

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_GRAPH_HPP