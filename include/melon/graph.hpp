#pragma once

#include <concepts>
#include <cstddef>
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

// `auto`, not `decltype(auto)`: every range-returning CPO in melon
// decay-copies, so no range alias ever names a reference type. A graph that
// stores its vertices in a container returns std::views::all(container) -- a
// ref_view, no copy -- which is the std::ranges idiom and what melon requires
// of arc_targets_map.
struct vertices_fn {
    // The shape shared by every CPO here: one overload per protocol, each
    // carrying the noexcept of the expression written directly beside it, plus
    // a fallback overload where one exists. A shared is_noexcept() helper
    // re-runs the dispatch `if constexpr` in a second place and drifts into
    // measuring an expression its overload never evaluates.
    template <typename T>
        requires has_member_vertices<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(t.vertices())) {
        return t.vertices();
    }

    template <typename T>
        requires(!has_member_vertices<T>) && has_adl_vertices<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(vertices(t))) {
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
    template <typename T>
        requires has_member_num_vertices<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(t.num_vertices())) {
        return t.num_vertices();
    }

    template <typename T>
        requires(!has_member_num_vertices<T>) && has_adl_num_vertices<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(num_vertices(t))) {
        return num_vertices(t);
    }

    template <typename T>
        requires(!has_member_num_vertices<T>) && (!has_adl_num_vertices<T>) &&
                std::ranges::sized_range<vertices_range_t<T>>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(std::ranges::size(melon::vertices(t)))) {
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
    template <typename T>
        requires has_member_out_arcs<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(t.out_arcs(v))) {
        return t.out_arcs(v);
    }

    template <typename T>
        requires(!has_member_out_arcs<T>) && has_adl_out_arcs<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(out_arcs(t, v))) {
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
    template <typename T>
        requires has_member_in_arcs<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(t.in_arcs(v))) {
        return t.in_arcs(v);
    }

    template <typename T>
        requires(!has_member_in_arcs<T>) && has_adl_in_arcs<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(in_arcs(t, v))) {
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
    // The graph is captured as a pointer and the incidence tag by value:
    // capturing either by reference captures a *reference parameter*, whose
    // lifetime ends when this function returns while the view produced here
    // outlives it.
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
            if constexpr(detail::range_rank<out_arcs_range_t<T>>() >
                         detail::range_rank<in_arcs_range_t<T>>())
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
    template <typename T>
        requires has_member_out_degree<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(t.out_degree(v))) {
        return t.out_degree(v);
    }

    template <typename T>
        requires(!has_member_out_degree<T>) && has_adl_out_degree<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(out_degree(t, v))) {
        return out_degree(t, v);
    }

    template <typename T>
        requires(!has_member_out_degree<T>) &&
                (!has_adl_out_degree<T>) && has_sized_out_arcs<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(std::ranges::size(melon::out_arcs(t, v)))) {
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
    template <typename T>
        requires has_member_in_degree<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(t.in_degree(v))) {
        return t.in_degree(v);
    }

    template <typename T>
        requires(!has_member_in_degree<T>) && has_adl_in_degree<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(in_degree(t, v))) {
        return in_degree(t, v);
    }

    template <typename T>
        requires(!has_member_in_degree<T>) &&
                (!has_adl_in_degree<T>) && has_sized_in_arcs<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(std::ranges::size(melon::in_arcs(t, v)))) {
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
    template <typename T>
        requires has_member_num_arcs<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(t.num_arcs())) {
        return t.num_arcs();
    }

    template <typename T>
        requires(!has_member_num_arcs<T>) && has_adl_num_arcs<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(num_arcs(t))) {
        return num_arcs(t);
    }

    template <typename T>
        requires(!has_member_num_arcs<T>) && (!has_adl_num_arcs<T>) &&
                std::ranges::sized_range<arcs_range_t<T>>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(std::ranges::size(arcs_fn{}(t)))) {
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
    template <typename T>
        requires has_member_arc_source<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(noexcept(t.arc_source(a))) {
        return t.arc_source(a);
    }

    template <typename T>
        requires(!has_member_arc_source<T>) && has_adl_arc_source<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(noexcept(arc_source(t, a))) {
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
    template <typename T>
        requires has_member_arc_target<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(noexcept(t.arc_target(a))) {
        return t.arc_target(a);
    }

    template <typename T>
        requires(!has_member_arc_target<T>) && has_adl_arc_target<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(noexcept(arc_target(t, a))) {
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

// "Does this graph carry its *own* arcs_entries?", as opposed to one the CPO
// below synthesises from arcs() and the endpoint accessors. A forwarding view
// must forward exactly this case: forwarding the synthesised one too rebuilds a
// transform over a transform, and forwarding neither makes
// `graph<graph_ref_view<G>>` false for a graph whose arcs_entries is its only
// arc protocol.
template <typename T>
concept has_own_arcs_entries =
    has_member_arcs_entries<T> || has_adl_arcs_entries<T>;

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
                         detail::range_rank<arcs_range_t<T>>() >=
                             detail::range_rank<out_arcs_range_t<T>>())
                return list_arcs_entries(t);
            else
                return join_out_arcs_entries(t);
        } else if constexpr(!can_join_out_arcs_entries<T> &&
                            can_join_in_arcs_entries<T>) {
            if constexpr(can_list_arcs_entries<T> &&
                         detail::range_rank<arcs_range_t<T>>() >=
                             detail::range_rank<in_arcs_range_t<T>>())
                return list_arcs_entries(t);
            else
                return join_in_arcs_entries(t);
        } else {
            if constexpr(can_list_arcs_entries<T> &&
                         detail::range_rank<arcs_range_t<T>>() >=
                             std::max(detail::range_rank<out_arcs_range_t<T>>(),
                                      detail::range_rank<in_arcs_range_t<T>>()))
                return list_arcs_entries(t);
            else if constexpr(detail::range_rank<out_arcs_range_t<T>>() >=
                              detail::range_rank<in_arcs_range_t<T>>())
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
    template <typename T>
        requires has_member_create_vertex<T>
    constexpr auto operator() [[nodiscard]] (T & t) const
        noexcept(noexcept(t.create_vertex())) {
        return t.create_vertex();
    }

    template <typename T>
        requires(!has_member_create_vertex<T>) && has_adl_create_vertex<T>
    constexpr auto operator() [[nodiscard]] (T & t) const
        noexcept(noexcept(create_vertex(t))) {
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
    template <typename T>
        requires has_member_remove_vertex<T>
    constexpr auto operator()(T & t, const vertex_t<T> & v) const
        noexcept(noexcept(t.remove_vertex(v))) {
        return t.remove_vertex(v);
    }

    template <typename T>
        requires(!has_member_remove_vertex<T>) && has_adl_remove_vertex<T>
    constexpr auto operator()(T & t, const vertex_t<T> & v) const
        noexcept(noexcept(remove_vertex(t, v))) {
        return remove_vertex(t, v);
    }
};

template <typename T>
concept has_member_is_valid_vertex =
    requires(const T & t, const vertex_t<T> & v) {
        { t.is_valid_vertex(v) } -> std::convertible_to<bool>;
    };

template <typename T>
concept has_adl_is_valid_vertex = requires(const T & t, const vertex_t<T> & v) {
    { is_valid_vertex(t, v) } -> std::convertible_to<bool>;
};

struct is_valid_vertex_fn {
    template <typename T>
        requires has_member_is_valid_vertex<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(t.is_valid_vertex(v))) {
        return t.is_valid_vertex(v);
    }

    template <typename T>
        requires(!has_member_is_valid_vertex<T>) && has_adl_is_valid_vertex<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(noexcept(is_valid_vertex(t, v))) {
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
    template <typename T>
        requires has_member_create_arc<T>
    constexpr auto operator() [[nodiscard]] (T & t, const vertex_t<T> & u,
                                             const vertex_t<T> & v) const
        noexcept(noexcept(t.create_arc(u, v))) {
        return t.create_arc(u, v);
    }

    template <typename T>
        requires(!has_member_create_arc<T>) && has_adl_create_arc<T>
    constexpr auto operator() [[nodiscard]] (T & t, const vertex_t<T> & u,
                                             const vertex_t<T> & v) const
        noexcept(noexcept(create_arc(t, u, v))) {
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
    template <typename T>
        requires has_member_remove_arc<T>
    constexpr auto operator()(T & t, const arc_t<T> & a) const
        noexcept(noexcept(t.remove_arc(a))) {
        return t.remove_arc(a);
    }

    template <typename T>
        requires(!has_member_remove_arc<T>) && has_adl_remove_arc<T>
    constexpr auto operator()(T & t, const arc_t<T> & a) const
        noexcept(noexcept(remove_arc(t, a))) {
        return remove_arc(t, a);
    }
};

template <typename T>
concept has_member_is_valid_arc = requires(const T & t, const arc_t<T> & a) {
    { t.is_valid_arc(a) } -> std::convertible_to<bool>;
};

template <typename T>
concept has_adl_is_valid_arc = requires(const T & t, const arc_t<T> & a) {
    { is_valid_arc(t, a) } -> std::convertible_to<bool>;
};

struct is_valid_arc_fn {
    template <typename T>
        requires has_member_is_valid_arc<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(noexcept(t.is_valid_arc(a))) {
        return t.is_valid_arc(a);
    }

    template <typename T>
        requires(!has_member_is_valid_arc<T>) && has_adl_is_valid_arc<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(noexcept(is_valid_arc(t, a))) {
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
    template <typename T>
        requires has_member_change_arc_source<T>
    constexpr auto operator()(T & t, const arc_t<T> & a,
                              const vertex_t<T> & v) const
        noexcept(noexcept(t.change_arc_source(a, v))) {
        return t.change_arc_source(a, v);
    }

    template <typename T>
        requires(!has_member_change_arc_source<T>) &&
                has_adl_change_arc_source<T>
    constexpr auto operator()(T & t, const arc_t<T> & a,
                              const vertex_t<T> & v) const
        noexcept(noexcept(change_arc_source(t, a, v))) {
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
    template <typename T>
        requires has_member_change_arc_target<T>
    constexpr auto operator()(T & t, const arc_t<T> & a,
                              const vertex_t<T> & v) const
        noexcept(noexcept(t.change_arc_target(a, v))) {
        return t.change_arc_target(a, v);
    }

    template <typename T>
        requires(!has_member_change_arc_target<T>) &&
                has_adl_change_arc_target<T>
    constexpr auto operator()(T & t, const arc_t<T> & a,
                              const vertex_t<T> & v) const
        noexcept(noexcept(change_arc_target(t, a, v))) {
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

// The probes below take `G & g` where the operation mutates and `const G & g`
// where it does not, matching both the CPO they call and the `const T & t`
// every other concept in this header uses. A by-value `G` parameter is a
// different declaration for an array or function type and reads as though the
// concept needed a copy.
template <typename G>
concept has_vertex_creation = graph<G> && requires(G & g) {
    { melon::create_vertex(g) } -> std::same_as<vertex_t<G>>;
};

// "Can this graph answer whether a handle is still live?" -- separate from
// has_vertex_removal, which additionally demands remove_vertex. A read-only
// view over a mutable graph forwards the question without forwarding the
// mutation, so views::subgraph has to ask this one; asking has_vertex_removal
// instead silently drops the underlying graph's answer.
template <typename G>
concept has_is_valid_vertex =
    graph<G> && requires(const G & g, const vertex_t<G> & v) {
        { melon::is_valid_vertex(g, v) } -> std::convertible_to<bool>;
    };
template <typename G>
concept has_is_valid_arc =
    graph<G> && requires(const G & g, const arc_t<G> & a) {
        { melon::is_valid_arc(g, a) } -> std::convertible_to<bool>;
    };

template <typename G>
concept has_vertex_removal =
    has_is_valid_vertex<G> &&
    requires(G & g, const vertex_t<G> & v) { melon::remove_vertex(g, v); };

template <typename G>
concept has_arc_creation = graph<G> && requires(G & g, const vertex_t<G> & v) {
    { melon::create_arc(g, v, v) } -> std::same_as<arc_t<G>>;
};
template <typename G>
concept has_arc_removal =
    has_is_valid_arc<G> &&
    requires(G & g, const arc_t<G> & a) { melon::remove_arc(g, a); };

template <typename G>
concept has_change_arc_source =
    graph<G> && requires(G & g, const arc_t<G> & a, const vertex_t<G> & s) {
        melon::change_arc_source(g, a, s);
    };
template <typename G>
concept has_change_arc_target =
    graph<G> && requires(G & g, const arc_t<G> & a, const vertex_t<G> & t) {
        melon::change_arc_target(g, a, t);
    };

namespace cpo {
template <typename T>
concept has_member_arc_sources_map = requires(const T & t) {
    { t.arc_sources_map() } -> mapping_of<arc_t<T>, vertex_t<T>>;
};

template <typename T>
concept has_adl_arc_sources_map = requires(const T & t) {
    { arc_sources_map(t) } -> mapping_of<arc_t<T>, vertex_t<T>>;
};

// The fallback branch of both endpoint-map CPOs, parameterised on which
// endpoint it reads. A caller's noexcept must measure this construction: the
// map built here only calls melon::arc_source / arc_target later, when it is
// subscripted.
template <typename T, typename EndPoint>
[[nodiscard]] inline constexpr auto endpoint_map(const T & t,
                                                 EndPoint end_point_fn) {
    return maps::map([g = std::addressof(t), end_point_fn](const arc_t<T> & a) {
        return end_point_fn(*g, a);
    });
}

struct arc_sources_map_fn {
public:
    template <typename T>
        requires has_member_arc_sources_map<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(t.arc_sources_map())) {
        return t.arc_sources_map();
    }

    template <typename T>
        requires(!has_member_arc_sources_map<T>) && has_adl_arc_sources_map<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(arc_sources_map(t))) {
        return arc_sources_map(t);
    }

    template <typename T>
        requires(!has_member_arc_sources_map<T>) &&
                (!has_adl_arc_sources_map<T>) && has_arc_source<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(endpoint_map(t, arc_source_fn{}))) {
        return endpoint_map(t, arc_source_fn{});
    }
};

template <typename T>
concept has_member_arc_targets_map = requires(const T & t) {
    { t.arc_targets_map() } -> mapping_of<arc_t<T>, vertex_t<T>>;
};

template <typename T>
concept has_adl_arc_targets_map = requires(const T & t) {
    { arc_targets_map(t) } -> mapping_of<arc_t<T>, vertex_t<T>>;
};

struct arc_targets_map_fn {
public:
    template <typename T>
        requires has_member_arc_targets_map<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(t.arc_targets_map())) {
        return t.arc_targets_map();
    }

    template <typename T>
        requires(!has_member_arc_targets_map<T>) && has_adl_arc_targets_map<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(arc_targets_map(t))) {
        return arc_targets_map(t);
    }

    template <typename T>
        requires(!has_member_arc_targets_map<T>) &&
                (!has_adl_arc_targets_map<T>) && has_arc_target<T>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(noexcept(endpoint_map(t, arc_target_fn{}))) {
        return endpoint_map(t, arc_target_fn{});
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::arc_sources_map_fn arc_sources_map{};
inline constexpr cpo::arc_targets_map_fn arc_targets_map{};
}  // namespace cust

template <typename G>
concept has_arc_sources_map =
    requires(const G & g) { melon::arc_sources_map(g); };
template <typename G>
concept has_arc_targets_map =
    requires(const G & g) { melon::arc_targets_map(g); };

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

    // The default-value overload probes the call it actually makes: sharing
    // is_noexcept() with the 0-argument one claims noexcept for a throwing
    // create_vertex_map<V>(g, d).
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

// Parameterised on ValueType so the public name can be a variable template,
// invisible to ADL, for the reason spelled out on create_vertex_map_fn above.
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

// The default ValueType probes std::size_t only, so `has_vertex_map<G>` means
// "G maps *some* value type"; algorithms constrained on it go on to create maps
// of their own types. A create_vertex_map that handles only one value type
// satisfies the concept and then hard-errors inside the algorithm.
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
