#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <memory>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "melon/detail/specialization_of.hpp"
#include "melon/detail/stdlib_check.hpp"

#include "melon/mapping.hpp"

namespace melon {

// ---- Borrowed graphs -------------------------------------------------------

// Opt-in trait, mirroring std::ranges::enable_borrowed_range: true when the
// ranges a graph hands out stay valid independently of the *graph object*
// itself -- when the object is a handle to storage living elsewhere rather
// than something the ranges point back at.
//
// It is what tells an algorithm whether it may copy a cached incidence range.
// graph_ref_view is a bare pointer, so out_arcs(v) of the graph it names is
// unaffected by relocating the view. views::subgraph is not: its filtered
// ranges capture `this`, so a range obtained from a subgraph that an algorithm
// stores *by value* points back at that member, and a memberwise copy of the
// algorithm aims the new object's cursors at the old object's graph -- a
// use-after-free the moment the original dies.
//
// Not made redundant by detail::consumable_input_view's own counter, which
// covers the other half of the problem: there the iterator refers into a range
// the cursor owns, so it can be re-derived. Here the range refers to something
// the cursor neither owns nor can re-obtain.
//
// Default false, so a graph type is presumed unsafe to copy cached ranges from
// until it says otherwise. Specialise it for a view whose ranges are
// self-contained or refer only to storage outside the view.
template <typename G>
inline constexpr bool enable_borrowed_graph = false;

template <typename G>
concept borrowed_graph = enable_borrowed_graph<std::remove_cvref_t<G>>;

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
    //
    // Every range- or closure-returning CPO takes `T &&` and constrains the
    // category instead of binding `const T &`, which would accept a temporary
    // graph and dangle behind the result. An rvalue is admitted only where
    // the graph's borrowed promise covers the handed-out range -- and never
    // for a synthesized result, which captures the graph object's address
    // regardless of that promise. A constraint, not a deleted overload: the
    // reference compilers hard-error on a deleted selection inside a
    // requires-expression, where an unsatisfied constraint stays probeable.
    template <typename T, typename G = std::remove_cvref_t<T>>
        requires has_member_vertices<G> &&
                 (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator() [[nodiscard]] (T && t) const
        noexcept(noexcept(t.vertices())) {
        return t.vertices();
    }

    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(!has_member_vertices<G>) && has_adl_vertices<G> &&
                (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator() [[nodiscard]] (T && t) const
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
    template <typename T, typename G = std::remove_cvref_t<T>>
        requires has_member_out_arcs<G> &&
                 (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator()
        [[nodiscard]] (T && t, const vertex_t<G> & v) const
        noexcept(noexcept(t.out_arcs(v))) {
        return t.out_arcs(v);
    }

    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(!has_member_out_arcs<G>) && has_adl_out_arcs<G> &&
                (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator()
        [[nodiscard]] (T && t, const vertex_t<G> & v) const
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
    template <typename T, typename G = std::remove_cvref_t<T>>
        requires has_member_in_arcs<G> &&
                 (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator()
        [[nodiscard]] (T && t, const vertex_t<G> & v) const
        noexcept(noexcept(t.in_arcs(v))) {
        return t.in_arcs(v);
    }

    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(!has_member_in_arcs<G>) && has_adl_in_arcs<G> &&
                (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator()
        [[nodiscard]] (T && t, const vertex_t<G> & v) const
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
    template <typename G>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_arcs<G>)
            return noexcept(std::declval<const G &>().arcs());
        else if constexpr(has_adl_arcs<G>)
            return noexcept(arcs(std::declval<const G &>()));
        else
            return false;
    }

public:
    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(has_member_arcs<G> || has_adl_arcs<G> ||
                 can_join_incidence<G, out_arcs_fn> ||
                 can_join_incidence<G, in_arcs_fn>) &&
                (std::is_lvalue_reference_v<T> ||
                 (borrowed_graph<G> && (has_member_arcs<G> || has_adl_arcs<G>)))
    constexpr auto operator() [[nodiscard]] (T && t) const
        noexcept(is_noexcept<G>()) {
        if constexpr(has_member_arcs<G>)
            return t.arcs();
        else if constexpr(has_adl_arcs<G>)
            return arcs(t);
        else if constexpr(can_join_incidence<G, out_arcs_fn> &&
                          !can_join_incidence<G, in_arcs_fn>)
            return join_incidence(t, out_arcs_fn{});
        else if constexpr(can_join_incidence<G, in_arcs_fn> &&
                          !can_join_incidence<G, out_arcs_fn>)
            return join_incidence(t, in_arcs_fn{});
        else {
            if constexpr(detail::range_rank<out_arcs_range_t<G>>() >=
                         detail::range_rank<in_arcs_range_t<G>>())
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
// The entry shape every consumer destructures: std::get<0> names the arc,
// std::get<1> the (source, target) pair, each tuple-like of size 2. The
// `typename std::tuple_size<..>::type` line is not redundant with the
// tuple_size_v comparison next to it: naming the member of the undefined
// primary template is a substitution failure, where instantiating
// tuple_size_v on it is a hard error outside the immediate context.
template <typename P>
concept vertex_pair_shape =
    requires { typename std::tuple_size<std::remove_cvref_t<P>>::type; } &&
    (std::tuple_size_v<std::remove_cvref_t<P>> == 2) &&
    requires(const std::remove_cvref_t<P> & p) {
        std::get<0>(p);
        std::get<1>(p);
    };

template <typename E>
concept arc_entry_shape =
    requires { typename std::tuple_size<std::remove_cvref_t<E>>::type; } &&
    (std::tuple_size_v<std::remove_cvref_t<E>> == 2) &&
    requires(const std::remove_cvref_t<E> & e) {
        std::get<0>(e);
        { std::get<1>(e) } -> vertex_pair_shape;
    };

// Shape only, no arc_t / vertex_t: the protocol must stay detectable on types
// with no arcs() route at all -- a vertex-filtered subgraph of an
// entries-only graph keeps its filtered arcs_entries member while
// deliberately dropping arcs() (see subgraph.hpp). Type coherence is checked
// by `graph`, which requires has_arcs first.
template <typename R>
concept arc_entries_range = std::ranges::input_range<R> &&
                            arc_entry_shape<std::ranges::range_reference_t<R>>;

// The typed half, for `graph`: the entries must name the graph's own arc and
// vertex types, whichever protocol or fallback produced them.
template <typename R, typename T>
concept arc_entries_range_of =
    arc_entries_range<R> &&
    requires(const std::remove_cvref_t<std::ranges::range_reference_t<R>> & e) {
        { std::get<0>(e) } -> std::convertible_to<arc_t<T>>;
        { std::get<0>(std::get<1>(e)) } -> std::convertible_to<vertex_t<T>>;
        { std::get<1>(std::get<1>(e)) } -> std::convertible_to<vertex_t<T>>;
    };

// The shape is checked on protocol detection the way std::ranges::begin
// ignores a member begin() that returns a non-iterator: a wrong-shaped
// arcs_entries is not the protocol, so the graph falls back to the
// synthesised entries or fails `graph` -- instead of satisfying `graph` and
// erroring inside the transform lambda of whichever view or algorithm first
// touches an entry.
template <typename T>
concept has_member_arcs_entries = requires(const T & t) {
    { t.arcs_entries() } -> arc_entries_range;
};

template <typename T>
concept has_adl_arcs_entries = requires(const T & t) {
    { arcs_entries(t) } -> arc_entries_range;
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

// The three synthesisers below carry copy_constructible constraints on the
// handles their lambdas pair up: the return types are deduced, so probing
// `graph<T>` for a move-only handle type would otherwise instantiate the
// bodies and hard-error inside make_pair -- outside the immediate context,
// where no requires-expression can catch it -- instead of answering false.
template <typename T>
    requires std::copy_constructible<arc_t<T>> &&
             requires(const T & t, const arc_t<T> & a) {
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
    requires std::copy_constructible<arc_t<T>> &&
             std::copy_constructible<vertex_t<T>> &&
             requires(const T & t, const vertex_t<T> & v, arc_t<T> & a) {
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
    requires std::copy_constructible<arc_t<T>> &&
             std::copy_constructible<vertex_t<T>> &&
             requires(const T & t, const vertex_t<T> & v, arc_t<T> & a) {
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
    template <typename G>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_arcs_entries<G>)
            return noexcept(std::declval<const G &>().arcs_entries());
        else if constexpr(has_adl_arcs_entries<G>)
            return noexcept(arcs_entries(std::declval<const G &>()));
        else
            return false;
    }

public:
    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(has_member_arcs_entries<G> || has_adl_arcs_entries<G> ||
                 can_list_arcs_entries<G> || can_join_out_arcs_entries<G> ||
                 can_join_in_arcs_entries<G>) &&
                (std::is_lvalue_reference_v<T> ||
                 (borrowed_graph<G> && has_own_arcs_entries<G>))
    constexpr auto operator() [[nodiscard]] (T && t) const
        noexcept(is_noexcept<G>()) {
        if constexpr(has_member_arcs_entries<G>)
            return t.arcs_entries();
        else if constexpr(has_adl_arcs_entries<G>)
            return arcs_entries(t);
        else if constexpr(!can_join_out_arcs_entries<G> &&
                          !can_join_in_arcs_entries<G>)
            return list_arcs_entries(t);
        else if constexpr(can_join_out_arcs_entries<G> &&
                          !can_join_in_arcs_entries<G>) {
            if constexpr(can_list_arcs_entries<G> &&
                         detail::range_rank<arcs_range_t<G>>() >=
                             detail::range_rank<out_arcs_range_t<G>>())
                return list_arcs_entries(t);
            else
                return join_out_arcs_entries(t);
        } else if constexpr(!can_join_out_arcs_entries<G> &&
                            can_join_in_arcs_entries<G>) {
            if constexpr(can_list_arcs_entries<G> &&
                         detail::range_rank<arcs_range_t<G>>() >=
                             detail::range_rank<in_arcs_range_t<G>>())
                return list_arcs_entries(t);
            else
                return join_in_arcs_entries(t);
        } else {
            if constexpr(can_list_arcs_entries<G> &&
                         detail::range_rank<arcs_range_t<G>>() >=
                             std::max(detail::range_rank<out_arcs_range_t<G>>(),
                                      detail::range_rank<in_arcs_range_t<G>>()))
                return list_arcs_entries(t);
            else if constexpr(detail::range_rank<out_arcs_range_t<G>>() >=
                              detail::range_rank<in_arcs_range_t<G>>())
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

// Element check via range_reference_t, not `*ranges::begin(expr)`: begin() on
// the prvalue range would additionally demand the range be borrowed, silently
// hiding member neighbor ranges that are self-contained but not borrowed.
template <typename R, typename G>
concept vertex_range_of =
    std::ranges::input_range<R> &&
    std::convertible_to<std::ranges::range_reference_t<R>, vertex_t<G>>;

template <typename T>
concept has_member_out_neighbors =
    requires(const T & t, const vertex_t<T> & v) {
        { t.out_neighbors(v) } -> vertex_range_of<T>;
    };

template <typename T>
concept has_adl_out_neighbors = requires(const T & t, const vertex_t<T> & v) {
    { out_neighbors(t, v) } -> vertex_range_of<T>;
};

struct out_neighbors_fn {
private:
    template <typename G>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_out_neighbors<G>)
            return noexcept(std::declval<const G &>().out_neighbors(
                std::declval<const vertex_t<G> &>()));
        else if constexpr(has_adl_out_neighbors<G>)
            return noexcept(out_neighbors(std::declval<const G &>(),
                                          std::declval<const vertex_t<G> &>()));
        else
            return false;
    }

public:
    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(has_member_out_neighbors<G> || has_adl_out_neighbors<G> ||
                 can_list_incidence_endpoints<G, out_arcs_fn, arc_target_fn>) &&
                (std::is_lvalue_reference_v<T> ||
                 (borrowed_graph<G> &&
                  (has_member_out_neighbors<G> || has_adl_out_neighbors<G>)))
    constexpr auto operator()
        [[nodiscard]] (T && t, const vertex_t<G> & v) const
        noexcept(is_noexcept<G>()) {
        if constexpr(has_member_out_neighbors<G>)
            return t.out_neighbors(v);
        else if constexpr(has_adl_out_neighbors<G>)
            return out_neighbors(t, v);
        else
            return list_incidence_endpoints(t, v, out_arcs_fn{},
                                            arc_target_fn{});
    }
};

template <typename T>
concept has_member_in_neighbors = requires(const T & t, const vertex_t<T> & v) {
    { t.in_neighbors(v) } -> vertex_range_of<T>;
};

template <typename T>
concept has_adl_in_neighbors = requires(const T & t, const vertex_t<T> & v) {
    { in_neighbors(t, v) } -> vertex_range_of<T>;
};

struct in_neighbors_fn {
private:
    template <typename G>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_in_neighbors<G>)
            return noexcept(std::declval<const G &>().in_neighbors(
                std::declval<const vertex_t<G> &>()));
        else if constexpr(has_adl_in_neighbors<G>)
            return noexcept(in_neighbors(std::declval<const G &>(),
                                         std::declval<const vertex_t<G> &>()));
        else
            return false;
    }

public:
    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(has_member_in_neighbors<G> || has_adl_in_neighbors<G> ||
                 can_list_incidence_endpoints<G, in_arcs_fn, arc_source_fn>) &&
                (std::is_lvalue_reference_v<T> ||
                 (borrowed_graph<G> &&
                  (has_member_in_neighbors<G> || has_adl_in_neighbors<G>)))
    constexpr auto operator()
        [[nodiscard]] (T && t, const vertex_t<G> & v) const
        noexcept(is_noexcept<G>()) {
        if constexpr(has_member_in_neighbors<G>)
            return t.in_neighbors(v);
        else if constexpr(has_adl_in_neighbors<G>)
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
concept graph = has_vertices<T> && has_arcs<T> && requires(const T & t) {
    { melon::arcs_entries(t) } -> cpo::arc_entries_range_of<T>;
};

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
concept has_vertex_creation = has_vertices<G> && requires(G & g) {
    { melon::create_vertex(g) } -> std::convertible_to<vertex_t<G>>;
};

// "Can this graph answer whether a handle is still live?" -- separate from
// has_vertex_removal, which additionally demands remove_vertex. A read-only
// view over a mutable graph forwards the question without forwarding the
// mutation, so views::subgraph has to ask this one; asking has_vertex_removal
// instead silently drops the underlying graph's answer.
template <typename G>
concept has_is_valid_vertex =
    has_vertices<G> && requires(const G & g, const vertex_t<G> & v) {
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
    { melon::create_arc(g, v, v) } -> std::convertible_to<arc_t<G>>;
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
    return maps::function(
        [g = std::addressof(t), end_point_fn](const arc_t<T> & a) {
            return end_point_fn(*g, a);
        });
}

struct arc_sources_map_fn {
public:
    template <typename T, typename G = std::remove_cvref_t<T>>
        requires has_member_arc_sources_map<G> &&
                 (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator() [[nodiscard]] (T && t) const
        noexcept(noexcept(t.arc_sources_map())) {
        return t.arc_sources_map();
    }

    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(!has_member_arc_sources_map<G>) &&
                has_adl_arc_sources_map<G> &&
                (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator() [[nodiscard]] (T && t) const
        noexcept(noexcept(arc_sources_map(t))) {
        return arc_sources_map(t);
    }

    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(!has_member_arc_sources_map<G>) &&
                (!has_adl_arc_sources_map<G>) &&
                has_arc_source<G> && std::is_lvalue_reference_v<T>
    constexpr auto operator() [[nodiscard]] (T && t) const
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
    template <typename T, typename G = std::remove_cvref_t<T>>
        requires has_member_arc_targets_map<G> &&
                 (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator() [[nodiscard]] (T && t) const
        noexcept(noexcept(t.arc_targets_map())) {
        return t.arc_targets_map();
    }

    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(!has_member_arc_targets_map<G>) &&
                has_adl_arc_targets_map<G> &&
                (std::is_lvalue_reference_v<T> || borrowed_graph<G>)
    constexpr auto operator() [[nodiscard]] (T && t) const
        noexcept(noexcept(arc_targets_map(t))) {
        return arc_targets_map(t);
    }

    template <typename T, typename G = std::remove_cvref_t<T>>
        requires(!has_member_arc_targets_map<G>) &&
                (!has_adl_arc_targets_map<G>) &&
                has_arc_target<G> && std::is_lvalue_reference_v<T>
    constexpr auto operator() [[nodiscard]] (T && t) const
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

// What a map request carries when the caller names no role. An algorithm names
// a role for every map it creates (dijkstra_roles::heap_index, ...); a role is
// what lets a factory that hands out storage tell two same-typed maps apart.
struct default_role {};

}  // namespace melon

// The create-map CPOs live OUTSIDE namespace melon, unlike every other CPO in
// this file. Their unqualified calls must reach, besides ADL, customization
// functions declared at *global scope* before this header is included -- the
// protocol test/cpo.cpp pins for std containers, which ADL cannot serve. Put
// inside melon, that lookup walks through namespace melon, and MSVC re-runs
// it at instantiation time, when the melon::create_vertex_map variable
// further down exists: it finds the variable and re-enters the operator() it
// is still compiling (C3779, C2131). From here the enclosing-scope chain is
// global scope alone, so no compiler's lookup can meet the variable. An
// in-namespace poison pill cannot substitute: it shadows the global-scope
// protocol on every compiler.
namespace melon_create_map_cpo {

// Two factory shapes. A two-parameter template `create_vertex_map<T, Role>`
// answers per role; a one-parameter `create_vertex_map<T>` answers every role
// with its standard map, and is what every container declares. The
// two-parameter shape is probed FIRST: a role-aware member defaults its Role,
// so the one-parameter probe matches it too and, taken first, would hand every
// request to the default role.
template <typename T, typename ValueType, typename Role>
concept has_role_member_create_vertex_map =
    requires(const T & t, const ValueType & d) {
        {
            t.template create_vertex_map<ValueType, Role>()
        } -> melon::output_mapping_of<melon::vertex_t<T>, ValueType>;
        {
            t.template create_vertex_map<ValueType, Role>(d)
        } -> melon::output_mapping_of<melon::vertex_t<T>, ValueType>;
    };

template <typename T, typename ValueType, typename Role>
concept has_role_adl_create_vertex_map =
    requires(const T & t, const ValueType & d) {
        {
            create_vertex_map<ValueType, Role>(t)
        } -> melon::output_mapping_of<melon::vertex_t<T>, ValueType>;
        {
            create_vertex_map<ValueType, Role>(t, d)
        } -> melon::output_mapping_of<melon::vertex_t<T>, ValueType>;
    };

template <typename T, typename ValueType>
concept has_member_create_vertex_map =
    requires(const T & t, const ValueType & d) {
        {
            t.template create_vertex_map<ValueType>()
        } -> melon::output_mapping_of<melon::vertex_t<T>, ValueType>;
        {
            t.template create_vertex_map<ValueType>(d)
        } -> melon::output_mapping_of<melon::vertex_t<T>, ValueType>;
    };

template <typename T, typename ValueType>
concept has_adl_create_vertex_map = requires(const T & t, const ValueType & d) {
    {
        create_vertex_map<ValueType>(t)
    } -> melon::output_mapping_of<melon::vertex_t<T>, ValueType>;
    {
        create_vertex_map<ValueType>(t, d)
    } -> melon::output_mapping_of<melon::vertex_t<T>, ValueType>;
};

template <typename T, typename ValueType, typename Role>
concept can_create_vertex_map =
    has_role_member_create_vertex_map<T, ValueType, Role> ||
    has_role_adl_create_vertex_map<T, ValueType, Role> ||
    has_member_create_vertex_map<T, ValueType> ||
    has_adl_create_vertex_map<T, ValueType>;

// Parameterised on ValueType and Role so that the public name can be a
// *variable* template rather than a function template. A function template
// named create_vertex_map that lives in namespace melon is reachable by ADL
// from has_adl_create_vertex_map for every graph type whose associated
// namespaces include melon (any melon view, e.g. views::reverse), which makes
// the concept depend on itself: "satisfaction of atomic constraint depends on
// itself". Variable templates are not found by ADL, so the loop cannot close.
template <typename ValueType, typename Role>
struct create_vertex_map_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_role_member_create_vertex_map<T, ValueType, Role>)
            return noexcept(std::declval<const T &>()
                                .template create_vertex_map<ValueType, Role>());
        else if constexpr(has_role_adl_create_vertex_map<T, ValueType, Role>)
            return noexcept(
                create_vertex_map<ValueType, Role>(std::declval<const T &>()));
        else if constexpr(has_member_create_vertex_map<T, ValueType>)
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
        if constexpr(has_role_member_create_vertex_map<T, ValueType, Role>)
            return noexcept(std::declval<const T &>()
                                .template create_vertex_map<ValueType, Role>(
                                    std::declval<const ValueType &>()));
        else if constexpr(has_role_adl_create_vertex_map<T, ValueType, Role>)
            return noexcept(create_vertex_map<ValueType, Role>(
                std::declval<const T &>(), std::declval<const ValueType &>()));
        else if constexpr(has_member_create_vertex_map<T, ValueType>)
            return noexcept(
                std::declval<const T &>().template create_vertex_map<ValueType>(
                    std::declval<const ValueType &>()));
        else
            return noexcept(create_vertex_map<ValueType>(
                std::declval<const T &>(), std::declval<const ValueType &>()));
    }

public:
    template <typename T>
        requires can_create_vertex_map<T, ValueType, Role>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_role_member_create_vertex_map<T, ValueType, Role>)
            return t.template create_vertex_map<ValueType, Role>();
        else if constexpr(has_role_adl_create_vertex_map<T, ValueType, Role>)
            return create_vertex_map<ValueType, Role>(t);
        else if constexpr(has_member_create_vertex_map<T, ValueType>)
            return t.template create_vertex_map<ValueType>();
        else
            return create_vertex_map<ValueType>(t);
    }

    template <typename T>
        requires can_create_vertex_map<T, ValueType, Role>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const ValueType & d) const
        noexcept(is_noexcept_default<T>()) {
        if constexpr(has_role_member_create_vertex_map<T, ValueType, Role>)
            return t.template create_vertex_map<ValueType, Role>(d);
        else if constexpr(has_role_adl_create_vertex_map<T, ValueType, Role>)
            return create_vertex_map<ValueType, Role>(t, d);
        else if constexpr(has_member_create_vertex_map<T, ValueType>)
            return t.template create_vertex_map<ValueType>(d);
        else
            return create_vertex_map<ValueType>(t, d);
    }
};

template <typename T, typename ValueType, typename Role>
concept has_role_member_create_arc_map =
    requires(const T & t, const ValueType & d) {
        {
            t.template create_arc_map<ValueType, Role>()
        } -> melon::output_mapping_of<melon::arc_t<T>, ValueType>;
        {
            t.template create_arc_map<ValueType, Role>(d)
        } -> melon::output_mapping_of<melon::arc_t<T>, ValueType>;
    };

template <typename T, typename ValueType, typename Role>
concept has_role_adl_create_arc_map =
    requires(const T & t, const ValueType & d) {
        {
            create_arc_map<ValueType, Role>(t)
        } -> melon::output_mapping_of<melon::arc_t<T>, ValueType>;
        {
            create_arc_map<ValueType, Role>(t, d)
        } -> melon::output_mapping_of<melon::arc_t<T>, ValueType>;
    };

template <typename T, typename ValueType>
concept has_member_create_arc_map = requires(const T & t, const ValueType & d) {
    {
        t.template create_arc_map<ValueType>()
    } -> melon::output_mapping_of<melon::arc_t<T>, ValueType>;
    {
        t.template create_arc_map<ValueType>(d)
    } -> melon::output_mapping_of<melon::arc_t<T>, ValueType>;
};

template <typename T, typename ValueType>
concept has_adl_create_arc_map = requires(const T & t, const ValueType & d) {
    {
        create_arc_map<ValueType>(t)
    } -> melon::output_mapping_of<melon::arc_t<T>, ValueType>;
    {
        create_arc_map<ValueType>(t, d)
    } -> melon::output_mapping_of<melon::arc_t<T>, ValueType>;
};

template <typename T, typename ValueType, typename Role>
concept can_create_arc_map =
    has_role_member_create_arc_map<T, ValueType, Role> ||
    has_role_adl_create_arc_map<T, ValueType, Role> ||
    has_member_create_arc_map<T, ValueType> ||
    has_adl_create_arc_map<T, ValueType>;

// Parameterised on ValueType and Role so the public name can be a variable
// template, invisible to ADL, for the reason spelled out on
// create_vertex_map_fn above.
template <typename ValueType, typename Role>
struct create_arc_map_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_role_member_create_arc_map<T, ValueType, Role>)
            return noexcept(std::declval<const T &>()
                                .template create_arc_map<ValueType, Role>());
        else if constexpr(has_role_adl_create_arc_map<T, ValueType, Role>)
            return noexcept(
                create_arc_map<ValueType, Role>(std::declval<const T &>()));
        else if constexpr(has_member_create_arc_map<T, ValueType>)
            return noexcept(
                std::declval<const T &>().template create_arc_map<ValueType>());
        else
            return noexcept(
                create_arc_map<ValueType>(std::declval<const T &>()));
    }

    template <typename T>
    static constexpr bool is_noexcept_default() {
        if constexpr(has_role_member_create_arc_map<T, ValueType, Role>)
            return noexcept(std::declval<const T &>()
                                .template create_arc_map<ValueType, Role>(
                                    std::declval<const ValueType &>()));
        else if constexpr(has_role_adl_create_arc_map<T, ValueType, Role>)
            return noexcept(create_arc_map<ValueType, Role>(
                std::declval<const T &>(), std::declval<const ValueType &>()));
        else if constexpr(has_member_create_arc_map<T, ValueType>)
            return noexcept(
                std::declval<const T &>().template create_arc_map<ValueType>(
                    std::declval<const ValueType &>()));
        else
            return noexcept(create_arc_map<ValueType>(
                std::declval<const T &>(), std::declval<const ValueType &>()));
    }

public:
    template <typename T>
        requires can_create_arc_map<T, ValueType, Role>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_role_member_create_arc_map<T, ValueType, Role>)
            return t.template create_arc_map<ValueType, Role>();
        else if constexpr(has_role_adl_create_arc_map<T, ValueType, Role>)
            return create_arc_map<ValueType, Role>(t);
        else if constexpr(has_member_create_arc_map<T, ValueType>)
            return t.template create_arc_map<ValueType>();
        else
            return create_arc_map<ValueType>(t);
    }

    template <typename T>
        requires can_create_arc_map<T, ValueType, Role>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const ValueType & d) const
        noexcept(is_noexcept_default<T>()) {
        if constexpr(has_role_member_create_arc_map<T, ValueType, Role>)
            return t.template create_arc_map<ValueType, Role>(d);
        else if constexpr(has_role_adl_create_arc_map<T, ValueType, Role>)
            return create_arc_map<ValueType, Role>(t, d);
        else if constexpr(has_member_create_arc_map<T, ValueType>)
            return t.template create_arc_map<ValueType>(d);
        else
            return create_arc_map<ValueType>(t, d);
    }
};
}  // namespace melon_create_map_cpo

namespace melon {

inline namespace cust {
template <typename ValueType, typename Role = default_role>
inline constexpr melon_create_map_cpo::create_vertex_map_fn<ValueType, Role>
    create_vertex_map{};

template <typename ValueType, typename Role = default_role>
inline constexpr melon_create_map_cpo::create_arc_map_fn<ValueType, Role>
    create_arc_map{};
}  // namespace cust

template <typename T, typename ValueType, typename Role = default_role>
using vertex_map_t = decltype(melon::create_vertex_map<ValueType, Role>(
    std::declval<const T &>()));
template <typename T, typename ValueType, typename Role = default_role>
using arc_map_t =
    decltype(melon::create_arc_map<ValueType, Role>(std::declval<const T &>()));

// The default ValueType probes std::size_t only, so `has_vertex_map<G>` means
// "G maps *some* value type"; algorithms constrained on it go on to create maps
// of their own types. A create_vertex_map that handles only one value type
// satisfies the concept and then hard-errors inside the algorithm.
template <typename T, typename ValueType = std::size_t,
          typename Role = default_role>
concept has_vertex_map =
    has_vertices<T> && requires(const T & t, const ValueType & d) {
        melon::create_vertex_map<ValueType, Role>(t);
        melon::create_vertex_map<ValueType, Role>(t, d);
    };

template <typename T, typename ValueType = std::size_t,
          typename Role = default_role>
concept has_arc_map =
    has_arcs<T> && requires(const T & t, const ValueType & d) {
        melon::create_arc_map<ValueType, Role>(t);
        melon::create_arc_map<ValueType, Role>(t, d);
    };

}  // namespace melon

// ---- Undirected protocol ---------------------------------------------------

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
// contract above: `const T &` would accept a temporary graph and dangle
// behind the returned view. The body reads through as_const: forwarding `t`
// as vertices_fn does would let a non-const edges() overload answer with a
// type other than edges_range_t, which is computed from a const graph.
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
// contract above; as_const in the body per edges_fn above.
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

// Reopened outside namespace melon for the reason given where the namespace
// first opens above: the unqualified calls must reach global-scope
// customization functions without MSVC's instantiation-time lookup meeting
// the melon::create_edge_map variable below.
namespace melon_create_map_cpo {
// The two factory shapes and their probe order, as on create_vertex_map_fn
// above.
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

#include "melon/views/graph_view.hpp"
