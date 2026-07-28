#pragma once

#include <concepts>
#include <ranges>
#include <type_traits>

#include "melon/detail/specialization_of.hpp"
#include "melon/detail/stdlib_check.hpp"

#include "melon/mapping.hpp"

namespace melon {

namespace cpo {
template <typename T>
concept has_member_vertices = requires(const T & t) {
    { t.vertices() } -> std::ranges::input_range;
};

template <typename T>
concept has_adl_vertices = requires(const T & t) {
    { vertices(t) } -> std::ranges::input_range;
};

struct vertices_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_vertices<T>)
            return noexcept(std::declval<const T &>().vertices());
        else
            return noexcept(vertices(std::declval<const T &>()));
    }

public:
    template <typename T>
        requires has_member_vertices<T> || has_adl_vertices<T>
    constexpr decltype(auto) operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_vertices<T>)
            return t.vertices();
        else if constexpr(has_adl_vertices<T>)
            return vertices(t);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::vertices_fn vertices{};
}  // namespace cust

template <typename T>
using vertices_range_t = decltype(melon::vertices(std::declval<const T &>()));

template <typename T>
using vertex_t = std::ranges::range_value_t<vertices_range_t<T>>;

namespace cpo {
template <typename T>
concept has_member_num_vertices = requires(const T & t) {
    { t.num_vertices() } -> std::integral;
};

template <typename T>
concept has_adl_num_vertices = requires(const T & t) {
    { num_vertices(t) } -> std::integral;
};

struct num_vertices_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_num_vertices<T>)
            return noexcept(std::declval<const T &>().num_vertices());
        else if constexpr(has_adl_num_vertices<T>)
            return noexcept(num_vertices(std::declval<const T &>()));
        else
            return noexcept(
                std::ranges::size(melon::vertices(std::declval<const T &>())));
    }

public:
    template <typename T>
        requires has_member_num_vertices<T> || has_adl_num_vertices<T> ||
                 std::ranges::sized_range<vertices_range_t<T>>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_num_vertices<T>)
            return t.num_vertices();
        else if constexpr(has_adl_num_vertices<T>)
            return num_vertices(t);
        else
            return std::ranges::size(melon::vertices(t));
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::num_vertices_fn num_vertices{};
}  // namespace cust

namespace cpo {
template <typename T>
concept has_member_out_arcs = requires(const T & t, const vertex_t<T> & v) {
    { t.out_arcs(v) } -> std::ranges::input_range;
};

template <typename T>
concept has_adl_out_arcs = requires(const T & t, const vertex_t<T> & v) {
    { out_arcs(t, v) } -> std::ranges::input_range;
};

struct out_arcs_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_out_arcs<T>)
            return noexcept(std::declval<const T &>().out_arcs(
                std::declval<const vertex_t<T> &>()));
        else
            return noexcept(out_arcs(std::declval<const T &>(),
                                     std::declval<const vertex_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_out_arcs<T> || has_adl_out_arcs<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_out_arcs<T>)
            return t.out_arcs(v);
        else
            return out_arcs(t, v);
    }
};

template <typename T>
concept has_member_in_arcs = requires(const T & t, const vertex_t<T> & v) {
    { t.in_arcs(v) } -> std::ranges::input_range;
};

template <typename T>
concept has_adl_in_arcs = requires(const T & t, const vertex_t<T> & v) {
    { in_arcs(t, v) } -> std::ranges::input_range;
};

struct in_arcs_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_in_arcs<T>)
            return noexcept(std::declval<const T &>().in_arcs(
                std::declval<const vertex_t<T> &>()));
        else
            return noexcept(in_arcs(std::declval<const T &>(),
                                    std::declval<const vertex_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_in_arcs<T> || has_adl_in_arcs<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_in_arcs<T>)
            return t.in_arcs(v);
        else
            return in_arcs(t, v);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::out_arcs_fn out_arcs{};
inline constexpr cpo::in_arcs_fn in_arcs{};
}  // namespace cust

template <typename T>
using out_arcs_range_t = decltype(melon::out_arcs(
    std::declval<const T &>(), std::declval<const vertex_t<T> &>()));

template <typename T>
using out_arcs_iterator_t = std::ranges::iterator_t<out_arcs_range_t<T>>;

template <typename T>
using out_arcs_sentinel_t = std::ranges::sentinel_t<out_arcs_range_t<T>>;

template <typename T>
using in_arcs_range_t = decltype(melon::in_arcs(
    std::declval<const T &>(), std::declval<const vertex_t<T> &>()));

template <typename T>
using in_arcs_iterator_t = std::ranges::iterator_t<in_arcs_range_t<T>>;

template <typename T>
using in_arcs_sentinel_t = std::ranges::sentinel_t<in_arcs_range_t<T>>;

namespace cpo {
template <typename T>
concept has_member_arcs = requires(const T & t) {
    { t.arcs() } -> std::ranges::input_range;
};

template <typename T>
concept has_adl_arcs = requires(const T & t) {
    { arcs(t) } -> std::ranges::input_range;
};

template <typename T, typename Incidence>
    requires requires(const T & t, const vertex_t<T> & v) {
        { Incidence{}(t, v) } -> std::ranges::viewable_range;
    }
inline constexpr auto join_incidence
    [[nodiscard]] (const T & t, Incidence incidence_fn) {
    // The graph is captured as a pointer and the incidence tag by value.
    // Capturing either by reference captured a *reference parameter*, whose
    // lifetime ends when this function returns, while the view produced here
    // outlives it -- and the Incidence temporary is gone by then too.
    return std::views::join(std::views::transform(
        melon::vertices(t),
        [g = std::addressof(t), incidence_fn](const vertex_t<T> & v) {
            return incidence_fn(*g, v);
        }));
}

template <typename T, typename Incidence>
concept can_join_incidence =
    requires(const T & t) { join_incidence(t, Incidence{}); };

struct arcs_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_arcs<T>)
            return noexcept(std::declval<const T &>().arcs());
        else if constexpr(has_adl_arcs<T>)
            return noexcept(arcs(std::declval<const T &>()));
        else
            return false;
    }

public:
    template <typename T>
        requires has_member_arcs<T> || has_adl_arcs<T> ||
                 can_join_incidence<T, out_arcs_fn> ||
                 can_join_incidence<T, in_arcs_fn>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_arcs<T>)
            return t.arcs();
        else if constexpr(has_adl_arcs<T>)
            return arcs(t);
        else if constexpr(can_join_incidence<T, out_arcs_fn> &&
                          !can_join_incidence<T, in_arcs_fn>)
            return join_incidence(t, out_arcs_fn{});
        else if constexpr(can_join_incidence<T, in_arcs_fn> &&
                          !can_join_incidence<T, out_arcs_fn>)
            return join_incidence(t, in_arcs_fn{});
        else {
            if constexpr(detail::_range_rank<out_arcs_range_t<T>>() >
                         detail::_range_rank<in_arcs_range_t<T>>())
                return join_incidence(t, out_arcs_fn{});
            else
                return join_incidence(t, in_arcs_fn{});
        }
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::arcs_fn arcs{};
}  // namespace cust

template <typename T>
using arcs_range_t = decltype(melon::arcs(std::declval<const T &>()));

template <typename T>
using arc_t = std::ranges::range_value_t<arcs_range_t<T>>;

namespace cpo {
template <typename T>
concept has_member_out_degree = requires(const T & t, const vertex_t<T> & v) {
    { t.out_degree(v) } -> std::integral;
};

template <typename T>
concept has_adl_out_degree = requires(const T & t, const vertex_t<T> & v) {
    { out_degree(t, v) } -> std::integral;
};

template <typename T>
concept has_sized_out_arcs = requires(const T & t, const vertex_t<T> & v) {
    { out_arcs_fn{}(t, v) } -> std::ranges::sized_range;
};

struct out_degree_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_out_degree<T>)
            return noexcept(std::declval<const T &>().out_degree(
                std::declval<const vertex_t<T> &>()));
        else if constexpr(has_adl_out_degree<T>)
            return noexcept(out_degree(std::declval<const T &>(),
                                       std::declval<const vertex_t<T> &>()));
        else
            return noexcept(std::ranges::size(
                melon::out_arcs(std::declval<const T &>(),
                                std::declval<const vertex_t<T> &>())));
    }

public:
    template <typename T>
        requires has_member_out_degree<T> || has_adl_out_degree<T> ||
                 has_sized_out_arcs<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_out_degree<T>)
            return t.out_degree(v);
        else if constexpr(has_adl_out_degree<T>)
            return out_degree(t, v);
        else
            return std::ranges::size(melon::out_arcs(t, v));
    }
};

template <typename T>
concept has_member_in_degree = requires(const T & t, const vertex_t<T> & v) {
    { t.in_degree(v) } -> std::integral;
};

template <typename T>
concept has_adl_in_degree = requires(const T & t, const vertex_t<T> & v) {
    { in_degree(t, v) } -> std::integral;
};

template <typename T>
concept has_sized_in_arcs = requires(const T & t, const vertex_t<T> & v) {
    { in_arcs_fn{}(t, v) } -> std::ranges::sized_range;
};

struct in_degree_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_in_degree<T>)
            return noexcept(std::declval<const T &>().in_degree(
                std::declval<const vertex_t<T> &>()));
        else if constexpr(has_adl_in_degree<T>)
            return noexcept(in_degree(std::declval<const T &>(),
                                      std::declval<const vertex_t<T> &>()));
        else
            return noexcept(std::ranges::size(
                melon::in_arcs(std::declval<const T &>(),
                               std::declval<const vertex_t<T> &>())));
    }

public:
    template <typename T>
        requires has_member_in_degree<T> || has_adl_in_degree<T> ||
                 has_sized_in_arcs<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_in_degree<T>)
            return t.in_degree(v);
        else if constexpr(has_adl_in_degree<T>)
            return in_degree(t, v);
        else
            return std::ranges::size(melon::in_arcs(t, v));
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::out_degree_fn out_degree{};
inline constexpr cpo::in_degree_fn in_degree{};
}  // namespace cust

namespace cpo {
template <typename T>
concept has_member_num_arcs = requires(const T & t) {
    { t.num_arcs() } -> std::integral;
};

template <typename T>
concept has_adl_num_arcs = requires(const T & t) {
    { num_arcs(t) } -> std::integral;
};

struct num_arcs_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_num_arcs<T>)
            return noexcept(std::declval<const T &>().num_arcs());
        else if constexpr(has_adl_num_arcs<T>)
            return noexcept(num_arcs(std::declval<const T &>()));
        else
            return noexcept(
                std::ranges::size(arcs_fn{}(std::declval<const T &>())));
    }

public:
    template <typename T>
        requires has_member_num_arcs<T> || has_adl_num_arcs<T> ||
                 std::ranges::sized_range<arcs_range_t<T>>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_num_arcs<T>)
            return t.num_arcs();
        else if constexpr(has_adl_num_arcs<T>)
            return num_arcs(t);
        else
            return std::ranges::size(arcs_fn{}(t));
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::num_arcs_fn num_arcs{};
}  // namespace cust

namespace cpo {
template <typename T>
concept has_member_arc_source = requires(const T & t, const arc_t<T> & a) {
    { t.arc_source(a) } -> std::convertible_to<vertex_t<T>>;
};

template <typename T>
concept has_adl_arc_source = requires(const T & t, const arc_t<T> & a) {
    { arc_source(t, a) } -> std::convertible_to<vertex_t<T>>;
};

struct arc_source_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_arc_source<T>)
            return noexcept(std::declval<const T &>().arc_source(
                std::declval<const arc_t<T> &>()));
        else
            return noexcept(arc_source(std::declval<const T &>(),
                                       std::declval<const arc_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_arc_source<T> || has_adl_arc_source<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_arc_source<T>)
            return t.arc_source(a);
        else
            return arc_source(t, a);
    }
};

template <typename T>
concept has_member_arc_target = requires(const T & t, const arc_t<T> & a) {
    { t.arc_target(a) } -> std::convertible_to<vertex_t<T>>;
};

template <typename T>
concept has_adl_arc_target = requires(const T & t, const arc_t<T> & a) {
    { arc_target(t, a) } -> std::convertible_to<vertex_t<T>>;
};

struct arc_target_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_arc_target<T>)
            return noexcept(std::declval<const T &>().arc_target(
                std::declval<const arc_t<T> &>()));
        else
            return noexcept(arc_target(std::declval<const T &>(),
                                       std::declval<const arc_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_arc_target<T> || has_adl_arc_target<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_arc_target<T>)
            return t.arc_target(a);
        else
            return arc_target(t, a);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::arc_source_fn arc_source{};
inline constexpr cpo::arc_target_fn arc_target{};
}  // namespace cust

namespace cpo {
template <typename T>
concept has_member_arcs_entries = requires(const T & t) {
    { t.arcs_entries() } -> std::ranges::input_range;
};

template <typename T>
concept has_adl_arcs_entries = requires(const T & t) {
    { arcs_entries(t) } -> std::ranges::input_range;
};

template <typename T>
    requires requires(const T & t, const arc_t<T> & a) {
        { arcs_fn{}(t) } -> std::ranges::viewable_range;
        arc_source_fn{}(t, a);
        arc_target_fn{}(t, a);
    }
inline constexpr auto list_arcs_entries [[nodiscard]] (const T & t) {
    return std::views::transform(
        melon::arcs(t), [g = std::addressof(t)](const arc_t<T> & a) {
            return std::make_pair(a, std::make_pair(arc_source_fn{}(*g, a),
                                                    arc_target_fn{}(*g, a)));
        });
}

template <typename T>
concept can_list_arcs_entries = requires(const T & t) { list_arcs_entries(t); };

template <typename T>
    requires requires(const T & t, const vertex_t<T> & v, arc_t<T> & a) {
        { out_arcs_fn{}(t, v) } -> std::ranges::viewable_range;
        arc_target_fn{}(t, a);
    }
inline constexpr auto join_out_arcs_entries [[nodiscard]] (const T & t) {
    return std::views::join(std::views::transform(
        melon::vertices(t), [g = std::addressof(t)](const vertex_t<T> & s) {
            return std::views::transform(
                melon::out_arcs(*g, s), [g, s](const arc_t<T> & a) {
                    return std::make_pair(
                        a, std::make_pair(s, melon::arc_target(*g, a)));
                });
        }));
}

template <typename T>
concept can_join_out_arcs_entries =
    requires(const T & t) { join_out_arcs_entries(t); };

template <typename T>
    requires requires(const T & t, const vertex_t<T> & v, arc_t<T> & a) {
        { in_arcs_fn{}(t, v) } -> std::ranges::viewable_range;
        arc_source_fn{}(t, a);
    }
inline constexpr auto join_in_arcs_entries [[nodiscard]] (const T & t) {
    return std::views::join(std::views::transform(
        melon::vertices(t), [g = std::addressof(t)](const vertex_t<T> & v) {
            return std::views::transform(
                melon::in_arcs(*g, v), [g, v](const arc_t<T> & a) {
                    return std::make_pair(
                        a, std::make_pair(melon::arc_source(*g, a), v));
                });
        }));
}

template <typename T>
concept can_join_in_arcs_entries =
    requires(const T & t) { join_in_arcs_entries(t); };

struct arcs_entries_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_arcs_entries<T>)
            return noexcept(std::declval<const T &>().arcs_entries());
        else if constexpr(has_adl_arcs_entries<T>)
            return noexcept(arcs_entries(std::declval<const T &>()));
        else
            return false;
    }

public:
    template <typename T>
        requires has_member_arcs_entries<T> || has_adl_arcs_entries<T> ||
                 can_list_arcs_entries<T> || can_join_out_arcs_entries<T> ||
                 can_join_in_arcs_entries<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_arcs_entries<T>)
            return t.arcs_entries();
        else if constexpr(has_adl_arcs_entries<T>)
            return arcs_entries(t);
        else if constexpr(!can_join_out_arcs_entries<T> &&
                          !can_join_in_arcs_entries<T>)
            return list_arcs_entries(t);
        else if constexpr(can_join_out_arcs_entries<T> &&
                          !can_join_in_arcs_entries<T>) {
            if constexpr(can_list_arcs_entries<T> &&
                         detail::_range_rank<arcs_range_t<T>>() >=
                             detail::_range_rank<out_arcs_range_t<T>>())
                return list_arcs_entries(t);
            else
                return join_out_arcs_entries(t);
        } else if constexpr(!can_join_out_arcs_entries<T> &&
                            can_join_in_arcs_entries<T>) {
            if constexpr(can_list_arcs_entries<T> &&
                         detail::_range_rank<arcs_range_t<T>>() >=
                             detail::_range_rank<in_arcs_range_t<T>>())
                return list_arcs_entries(t);
            else
                return join_in_arcs_entries(t);
        } else {
            if constexpr(can_list_arcs_entries<T> &&
                         detail::_range_rank<arcs_range_t<T>>() >=
                             std::max(
                                 detail::_range_rank<out_arcs_range_t<T>>(),
                                 detail::_range_rank<in_arcs_range_t<T>>()))
                return list_arcs_entries(t);
            else if constexpr(detail::_range_rank<out_arcs_range_t<T>>() >=
                              detail::_range_rank<in_arcs_range_t<T>>())
                return join_out_arcs_entries(t);
            else
                return join_in_arcs_entries(t);
        }
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::arcs_entries_fn arcs_entries{};
}  // namespace cust

namespace cpo {
template <typename T, typename Incidence, typename EndPoint>
    requires requires(const T & t, const vertex_t<T> & v, arc_t<T> & a) {
        { Incidence{}(t, v) } -> std::ranges::viewable_range;
        EndPoint{}(t, a);
    }
inline constexpr auto list_incidence_endpoints
    [[nodiscard]] (const T & t, const vertex_t<T> & v, Incidence incidence_fn,
                   EndPoint end_point_fn) {
    // See join_incidence: both tags are taken and captured by value, and the
    // graph by address, so nothing in the returned view refers to a parameter.
    return std::views::transform(
        incidence_fn(t, v),
        [g = std::addressof(t), end_point_fn](const arc_t<T> & a) {
            return end_point_fn(*g, a);
        });
}

template <typename T, typename Incidence, typename EndPoint>
concept can_list_incidence_endpoints =
    requires(const T & t, const vertex_t<T> & v) {
        list_incidence_endpoints(t, v, Incidence{}, EndPoint{});
    };

template <typename T>
concept has_member_out_neighbors =
    requires(const T & t, const vertex_t<T> & v) {
        { t.out_neighbors(v) } -> std::ranges::input_range;
        {
            *std::ranges::begin(t.out_neighbors(v))
        } -> std::convertible_to<vertex_t<T>>;
    };

template <typename T>
concept has_adl_out_neighbors = requires(const T & t, const vertex_t<T> & v) {
    { out_neighbors(t, v) } -> std::ranges::input_range;
    {
        *std::ranges::begin(out_neighbors(t, v))
    } -> std::convertible_to<vertex_t<T>>;
};

struct out_neighbors_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_out_neighbors<T>)
            return noexcept(std::declval<const T &>().out_neighbors(
                std::declval<const vertex_t<T> &>()));
        else if constexpr(has_adl_out_neighbors<T>)
            return noexcept(out_neighbors(std::declval<const T &>(),
                                          std::declval<const vertex_t<T> &>()));
        else
            return false;
    }

public:
    template <typename T>
        requires has_member_out_neighbors<T> || has_adl_out_neighbors<T> ||
                 can_list_incidence_endpoints<T, out_arcs_fn, arc_target_fn>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_out_neighbors<T>)
            return t.out_neighbors(v);
        else if constexpr(has_adl_out_neighbors<T>)
            return out_neighbors(t, v);
        else
            return list_incidence_endpoints(t, v, out_arcs_fn{},
                                            arc_target_fn{});
    }
};

template <typename T>
concept has_member_in_neighbors = requires(const T & t, const vertex_t<T> & v) {
    { t.in_neighbors(v) } -> std::ranges::input_range;
    {
        *std::ranges::begin(t.in_neighbors(v))
    } -> std::convertible_to<vertex_t<T>>;
};

template <typename T>
concept has_adl_in_neighbors = requires(const T & t, const vertex_t<T> & v) {
    { in_neighbors(t, v) } -> std::ranges::input_range;
    {
        *std::ranges::begin(in_neighbors(t, v))
    } -> std::convertible_to<vertex_t<T>>;
};

struct in_neighbors_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_in_neighbors<T>)
            return noexcept(std::declval<const T &>().in_neighbors(
                std::declval<const vertex_t<T> &>()));
        else if constexpr(has_adl_in_neighbors<T>)
            return noexcept(in_neighbors(std::declval<const T &>(),
                                         std::declval<const vertex_t<T> &>()));
        else
            return false;
    }

public:
    template <typename T>
        requires has_member_in_neighbors<T> || has_adl_in_neighbors<T> ||
                 can_list_incidence_endpoints<T, in_arcs_fn, arc_source_fn>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_in_neighbors<T>)
            return t.in_neighbors(v);
        else if constexpr(has_adl_in_neighbors<T>)
            return in_neighbors(t, v);
        else
            return list_incidence_endpoints(t, v, in_arcs_fn{},
                                            arc_source_fn{});
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::out_neighbors_fn out_neighbors{};
inline constexpr cpo::in_neighbors_fn in_neighbors{};
}  // namespace cust

template <typename T>
using out_neighbors_range_t = decltype(melon::out_neighbors(
    std::declval<const T &>(), std::declval<const vertex_t<T> &>()));

template <typename T>
using in_neighbors_range_t = decltype(melon::in_neighbors(
    std::declval<const T &>(), std::declval<const vertex_t<T> &>()));

template <typename T>
concept has_vertices = requires(const T & t) { melon::vertices(t); };

template <typename T>
concept has_num_vertices =
    has_vertices<T> && requires(const T & t) { melon::num_vertices(t); };

template <typename T>
concept has_arcs = requires(const T & t) { melon::arcs(t); };

template <typename T>
concept has_num_arcs =
    has_arcs<T> && requires(const T & t) { melon::num_arcs(t); };

template <typename T>
concept graph = has_vertices<T> && has_arcs<T> &&
                requires(const T & t) { melon::arcs_entries(t); };

template <typename T>
concept has_arc_target = graph<T> && requires(const T & t, const arc_t<T> & a) {
    melon::arc_target(t, a);
};

template <typename T>
concept has_arc_source = graph<T> && requires(const T & t, const arc_t<T> & a) {
    melon::arc_source(t, a);
};

template <typename T>
concept has_out_arcs =
    graph<T> &&
    requires(const T & t, const vertex_t<T> & v) { melon::out_arcs(t, v); } &&
    std::convertible_to<std::ranges::range_value_t<out_arcs_range_t<T>>,
                        arc_t<T>>;

template <typename T>
concept has_in_arcs =
    graph<T> &&
    requires(const T & t, const vertex_t<T> & v) { melon::in_arcs(t, v); } &&
    std::convertible_to<std::ranges::range_value_t<in_arcs_range_t<T>>,
                        arc_t<T>>;

template <typename T>
concept has_out_degree =
    graph<T> &&
    requires(const T & t, const vertex_t<T> & v) { melon::out_degree(t, v); };

template <typename T>
concept has_in_degree =
    graph<T> &&
    requires(const T & t, const vertex_t<T> & v) { melon::in_degree(t, v); };

template <typename T>
concept outward_incidence_graph =
    graph<T> && has_out_arcs<T> && has_arc_target<T>;

template <typename T>
concept inward_incidence_graph =
    graph<T> && has_in_arcs<T> && has_arc_source<T>;

template <typename T>
concept outward_adjacency_graph =
    graph<T> && requires(const T & t, const vertex_t<T> & v) {
        melon::out_neighbors(t, v);
    };

template <typename T>
concept inward_adjacency_graph =
    graph<T> &&
    requires(const T & t, const vertex_t<T> & v) { melon::in_neighbors(t, v); };

namespace cpo {
template <typename T>
concept has_member_create_vertex = requires(T & t) {
    { t.create_vertex() } -> std::convertible_to<vertex_t<T>>;
};

template <typename T>
concept has_adl_create_vertex = requires(T & t) {
    { create_vertex(t) } -> std::convertible_to<vertex_t<T>>;
};

struct create_vertex_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_create_vertex<T>)
            return noexcept(std::declval<T &>().create_vertex());
        else
            return noexcept(create_vertex(std::declval<T &>()));
    }

public:
    template <typename T>
        requires has_member_create_vertex<T> || has_adl_create_vertex<T>
    constexpr auto operator() [[nodiscard]] (T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_create_vertex<T>)
            return t.create_vertex();
        else
            return create_vertex(t);
    }
};

template <typename T>
concept has_member_remove_vertex =
    requires(T & t, const vertex_t<T> & v) { t.remove_vertex(v); };

template <typename T>
concept has_adl_remove_vertex =
    requires(T & t, const vertex_t<T> & v) { remove_vertex(t, v); };

struct remove_vertex_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_remove_vertex<T>)
            return noexcept(std::declval<T &>().remove_vertex(
                std::declval<const vertex_t<T> &>()));
        else
            return noexcept(remove_vertex(std::declval<T &>(),
                                          std::declval<const vertex_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_remove_vertex<T> || has_adl_remove_vertex<T>
    constexpr auto operator() [[nodiscard]] (T & t, const vertex_t<T> & v) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_remove_vertex<T>)
            return t.remove_vertex(v);
        else
            return remove_vertex(t, v);
    }
};

template <typename T>
concept has_member_is_valid_vertex =
    requires(const T & t, const vertex_t<T> & v) {
        { t.is_valid_vertex(v) } -> std::convertible_to<vertex_t<T>>;
    };

template <typename T>
concept has_adl_is_valid_vertex = requires(const T & t, const vertex_t<T> & v) {
    { is_valid_vertex(t, v) } -> std::convertible_to<vertex_t<T>>;
};

struct is_valid_vertex_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_is_valid_vertex<T>)
            return noexcept(std::declval<const T &>().is_valid_vertex(
                std::declval<const vertex_t<T> &>()));
        else
            return noexcept(
                is_valid_vertex(std::declval<const T &>(),
                                std::declval<const vertex_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_is_valid_vertex<T> || has_adl_is_valid_vertex<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_is_valid_vertex<T>)
            return t.is_valid_vertex(v);
        else
            return is_valid_vertex(t, v);
    }
};

template <typename T>
concept has_member_create_arc = requires(T & t, const vertex_t<T> & v) {
    { t.create_arc(v, v) } -> std::convertible_to<arc_t<T>>;
};

template <typename T>
concept has_adl_create_arc = requires(T & t, const vertex_t<T> & v) {
    { create_arc(t, v, v) } -> std::convertible_to<arc_t<T>>;
};

struct create_arc_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_create_arc<T>)
            return noexcept(std::declval<T &>().create_arc(
                std::declval<const vertex_t<T> &>(),
                std::declval<const vertex_t<T> &>()));
        else
            return noexcept(create_arc(std::declval<T &>(),
                                       std::declval<const vertex_t<T> &>(),
                                       std::declval<const vertex_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_create_arc<T> || has_adl_create_arc<T>
    constexpr auto operator() [[nodiscard]] (T & t, const vertex_t<T> & u,
                                             const vertex_t<T> & v) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_create_arc<T>)
            return t.create_arc(u, v);
        else
            return create_arc(t, u, v);
    }
};

template <typename T>
concept has_member_remove_arc =
    requires(T & t, const arc_t<T> & a) { t.remove_arc(a); };

template <typename T>
concept has_adl_remove_arc =
    requires(T & t, const arc_t<T> & a) { remove_arc(t, a); };

struct remove_arc_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_remove_arc<T>)
            return noexcept(std::declval<T &>().remove_arc(
                std::declval<const arc_t<T> &>()));
        else
            return noexcept(remove_arc(std::declval<T &>(),
                                       std::declval<const arc_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_remove_arc<T> || has_adl_remove_arc<T>
    constexpr auto operator() [[nodiscard]] (T & t, const arc_t<T> & a) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_remove_arc<T>)
            return t.remove_arc(a);
        else
            return remove_arc(t, a);
    }
};

template <typename T>
concept has_member_is_valid_arc = requires(const T & t, const arc_t<T> & a) {
    { t.is_valid_arc(a) } -> std::convertible_to<arc_t<T>>;
};

template <typename T>
concept has_adl_is_valid_arc = requires(const T & t, const arc_t<T> & a) {
    { is_valid_arc(t, a) } -> std::convertible_to<arc_t<T>>;
};

struct is_valid_arc_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_is_valid_arc<T>)
            return noexcept(std::declval<const T &>().is_valid_arc(
                std::declval<const arc_t<T> &>()));
        else
            return noexcept(is_valid_arc(std::declval<const T &>(),
                                         std::declval<const arc_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_is_valid_arc<T> || has_adl_is_valid_arc<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_is_valid_arc<T>)
            return t.is_valid_arc(a);
        else
            return is_valid_arc(t, a);
    }
};

template <typename T>
concept has_member_change_arc_source =
    requires(T & t, const arc_t<T> & a, const vertex_t<T> & v) {
        t.change_arc_source(a, v);
    };

template <typename T>
concept has_adl_change_arc_source =
    requires(T & t, const arc_t<T> & a, const vertex_t<T> & v) {
        change_arc_source(t, a, v);
    };

struct change_arc_source_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_change_arc_source<T>)
            return noexcept(std::declval<T &>().change_arc_source(
                std::declval<const arc_t<T> &>(),
                std::declval<const vertex_t<T> &>()));
        else
            return noexcept(change_arc_source(
                std::declval<T &>(), std::declval<const arc_t<T> &>(),
                std::declval<const vertex_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_change_arc_source<T> || has_adl_change_arc_source<T>
    constexpr auto operator()
        [[nodiscard]] (T & t, const arc_t<T> & a, const vertex_t<T> & v) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_change_arc_source<T>)
            return t.change_arc_source(a, v);
        else
            return change_arc_source(t, a, v);
    }
};

template <typename T>
concept has_member_change_arc_target =
    requires(T & t, const arc_t<T> & a, const vertex_t<T> & v) {
        t.change_arc_target(a, v);
    };

template <typename T>
concept has_adl_change_arc_target =
    requires(T & t, const arc_t<T> & a, const vertex_t<T> & v) {
        change_arc_target(t, a, v);
    };

struct change_arc_target_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_change_arc_target<T>)
            return noexcept(std::declval<T &>().change_arc_target(
                std::declval<const arc_t<T> &>(),
                std::declval<const vertex_t<T> &>()));
        else
            return noexcept(change_arc_target(
                std::declval<T &>(), std::declval<const arc_t<T> &>(),
                std::declval<const vertex_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_change_arc_target<T> || has_adl_change_arc_target<T>
    constexpr auto operator()
        [[nodiscard]] (T & t, const arc_t<T> & a, const vertex_t<T> & v) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_change_arc_target<T>)
            return t.change_arc_target(a, v);
        else
            return change_arc_target(t, a, v);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::create_vertex_fn create_vertex{};
inline constexpr cpo::remove_vertex_fn remove_vertex{};
inline constexpr cpo::is_valid_vertex_fn is_valid_vertex{};
inline constexpr cpo::create_arc_fn create_arc{};
inline constexpr cpo::remove_arc_fn remove_arc{};
inline constexpr cpo::is_valid_arc_fn is_valid_arc{};
inline constexpr cpo::change_arc_target_fn change_arc_target{};
inline constexpr cpo::change_arc_source_fn change_arc_source{};
}  // namespace cust

template <typename G>
concept has_vertex_creation = graph<G> && requires(G g) {
    { melon::create_vertex(g) } -> std::same_as<vertex_t<G>>;
};
template <typename G>
concept has_vertex_removal = graph<G> && requires(G g, vertex_t<G> v) {
    melon::remove_vertex(g, v);
    { melon::is_valid_vertex(g, v) } -> std::convertible_to<bool>;
};

template <typename G>
concept has_arc_creation = graph<G> && requires(G g, vertex_t<G> v) {
    { melon::create_arc(g, v, v) } -> std::same_as<arc_t<G>>;
};
template <typename G>
concept has_arc_removal = graph<G> && requires(G g, arc_t<G> a) {
    remove_arc(g, a);
    { melon::is_valid_arc(g, a) } -> std::convertible_to<bool>;
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

namespace cpo {
template <typename T>
concept has_member_arc_sources_map = requires(const T & t) {
    { t.arc_sources_map() } -> input_mapping_of<arc_t<T>, vertex_t<T>>;
};

template <typename T>
concept has_adl_arc_sources_map = requires(const T & t) {
    { arc_sources_map(t) } -> input_mapping_of<arc_t<T>, vertex_t<T>>;
};

struct arc_sources_map_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_arc_sources_map<T>)
            return noexcept(std::declval<const T &>().arc_sources_map());
        else if constexpr(has_adl_arc_sources_map<T>)
            return noexcept(arc_sources_map(std::declval<const T &>()));
        else
            return noexcept(melon::arc_source(
                std::declval<const T &>(), std::declval<const arc_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_arc_sources_map<T> || has_adl_arc_sources_map<T> ||
                 has_arc_source<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_arc_sources_map<T>)
            return t.arc_sources_map();
        else if constexpr(has_adl_arc_sources_map<T>)
            return arc_sources_map(t);
        else
            return views::map([g = std::addressof(t)](const arc_t<T> & a) {
                return melon::arc_source(*g, a);
            });
    }
};

template <typename T>
concept has_member_arc_targets_map = requires(const T & t) {
    { t.arc_targets_map() } -> input_mapping_of<arc_t<T>, vertex_t<T>>;
};

template <typename T>
concept has_adl_arc_targets_map = requires(const T & t) {
    { arc_targets_map(t) } -> input_mapping_of<arc_t<T>, vertex_t<T>>;
};

struct arc_targets_map_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_arc_targets_map<T>)
            return noexcept(std::declval<const T &>().arc_targets_map());
        else if constexpr(has_adl_arc_targets_map<T>)
            return noexcept(arc_targets_map(std::declval<const T &>()));
        else
            return noexcept(melon::arc_target(
                std::declval<const T &>(), std::declval<const arc_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_arc_targets_map<T> || has_adl_arc_targets_map<T> ||
                 has_arc_target<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_arc_targets_map<T>)
            return t.arc_targets_map();
        else if constexpr(has_adl_arc_targets_map<T>)
            return arc_targets_map(t);
        else
            return views::map([g = std::addressof(t)](const arc_t<T> & a) {
                return melon::arc_target(*g, a);
            });
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::arc_sources_map_fn arc_sources_map{};
inline constexpr cpo::arc_targets_map_fn arc_targets_map{};
}  // namespace cust

namespace cpo {
template <typename T, typename ValueType>
concept has_member_create_vertex_map =
    requires(const T & t, const ValueType & d) {
        {
            t.template create_vertex_map<ValueType>()
        } -> output_mapping_of<vertex_t<T>, ValueType>;
        {
            t.template create_vertex_map<ValueType>(d)
        } -> output_mapping_of<vertex_t<T>, ValueType>;
    };

template <typename T, typename ValueType>
concept has_adl_create_vertex_map = requires(const T & t, const ValueType & d) {
    {
        create_vertex_map<ValueType>(t)
    } -> output_mapping_of<vertex_t<T>, ValueType>;
    {
        create_vertex_map<ValueType>(t, d)
    } -> output_mapping_of<vertex_t<T>, ValueType>;
};

// Parameterised on ValueType so that the public name can be a *variable*
// template rather than a function template. A function template named
// create_vertex_map that lives in namespace melon is reachable by ADL from
// has_adl_create_vertex_map for every graph type whose associated namespaces
// include melon (any melon view, e.g. views::reverse), which makes the
// concept depend on itself: "satisfaction of atomic constraint depends on
// itself". Variable templates are not found by ADL, so the loop cannot close.
template <typename ValueType>
struct create_vertex_map_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_create_vertex_map<T, ValueType>)
            return noexcept(std::declval<const T &>()
                                .template create_vertex_map<ValueType>());
        else
            return noexcept(
                create_vertex_map<ValueType>(std::declval<const T &>()));
    }

    // The default-value overload has to probe the call it actually makes:
    // sharing is_noexcept() with the 0-argument one claimed noexcept for a
    // throwing create_vertex_map<V>(g, d).
    template <typename T>
    static constexpr bool is_noexcept_default() {
        if constexpr(has_member_create_vertex_map<T, ValueType>)
            return noexcept(
                std::declval<const T &>().template create_vertex_map<ValueType>(
                    std::declval<const ValueType &>()));
        else
            return noexcept(create_vertex_map<ValueType>(
                std::declval<const T &>(), std::declval<const ValueType &>()));
    }

public:
    template <typename T>
        requires has_member_create_vertex_map<T, ValueType> ||
                 has_adl_create_vertex_map<T, ValueType>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_create_vertex_map<T, ValueType>)
            return t.template create_vertex_map<ValueType>();
        else
            return create_vertex_map<ValueType>(t);
    }

    template <typename T>
        requires has_member_create_vertex_map<T, ValueType> ||
                 has_adl_create_vertex_map<T, ValueType>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const ValueType & d) const
        noexcept(is_noexcept_default<T>()) {
        if constexpr(has_member_create_vertex_map<T, ValueType>)
            return t.template create_vertex_map<ValueType>(d);
        else
            return create_vertex_map<ValueType>(t, d);
    }
};

template <typename T, typename ValueType>
concept has_member_create_arc_map = requires(const T & t, const ValueType & d) {
    {
        t.template create_arc_map<ValueType>()
    } -> output_mapping_of<arc_t<T>, ValueType>;
    {
        t.template create_arc_map<ValueType>(d)
    } -> output_mapping_of<arc_t<T>, ValueType>;
};

template <typename T, typename ValueType>
concept has_adl_create_arc_map = requires(const T & t, const ValueType & d) {
    { create_arc_map<ValueType>(t) } -> output_mapping_of<arc_t<T>, ValueType>;
    {
        create_arc_map<ValueType>(t, d)
    } -> output_mapping_of<arc_t<T>, ValueType>;
};

// Parameterised on ValueType for the same reason as create_vertex_map_fn above.
template <typename ValueType>
struct create_arc_map_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_create_arc_map<T, ValueType>)
            return noexcept(
                std::declval<const T &>().template create_arc_map<ValueType>());
        else
            return noexcept(
                create_arc_map<ValueType>(std::declval<const T &>()));
    }

    // See create_vertex_map_fn::is_noexcept_default.
    template <typename T>
    static constexpr bool is_noexcept_default() {
        if constexpr(has_member_create_arc_map<T, ValueType>)
            return noexcept(
                std::declval<const T &>().template create_arc_map<ValueType>(
                    std::declval<const ValueType &>()));
        else
            return noexcept(create_arc_map<ValueType>(
                std::declval<const T &>(), std::declval<const ValueType &>()));
    }

public:
    template <typename T>
        requires has_member_create_arc_map<T, ValueType> ||
                 has_adl_create_arc_map<T, ValueType>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_create_arc_map<T, ValueType>)
            return t.template create_arc_map<ValueType>();
        else
            return create_arc_map<ValueType>(t);
    }

    template <typename T>
        requires has_member_create_arc_map<T, ValueType> ||
                 has_adl_create_arc_map<T, ValueType>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const ValueType & d) const
        noexcept(is_noexcept_default<T>()) {
        if constexpr(has_member_create_arc_map<T, ValueType>)
            return t.template create_arc_map<ValueType>(d);
        else
            return create_arc_map<ValueType>(t, d);
    }
};
}  // namespace cpo

inline namespace cust {
// Variable templates, not function templates: `create_vertex_map<T>(g)` reads
// the same at every call site, but the name is now invisible to ADL, so the
// has_adl_create_vertex_map probe inside the CPO can only ever find a *user's*
// create_vertex_map. See the comment on create_vertex_map_fn.
template <typename ValueType>
inline constexpr cpo::create_vertex_map_fn<ValueType> create_vertex_map{};

template <typename ValueType>
inline constexpr cpo::create_arc_map_fn<ValueType> create_arc_map{};
}  // namespace cust

template <typename T, typename ValueType>
using vertex_map_t =
    decltype(melon::create_vertex_map<ValueType>(std::declval<const T &>()));
template <typename T, typename ValueType>
using arc_map_t =
    decltype(melon::create_arc_map<ValueType>(std::declval<const T &>()));

template <typename T, typename ValueType = std::size_t>
concept has_vertex_map =
    has_vertices<T> && requires(const T & t, const ValueType & d) {
        melon::create_vertex_map<ValueType>(t);
        melon::create_vertex_map<ValueType>(t, d);
    };

template <typename T, typename ValueType = std::size_t>
concept has_arc_map =
    has_arcs<T> && requires(const T & t, const ValueType & d) {
        melon::create_arc_map<ValueType>(t);
        melon::create_arc_map<ValueType>(t, d);
    };

}  // namespace melon

#include "views/graph_view.hpp"
