#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <limits>

namespace melon {

// `zero` and `infty` are the algebraic identities of the semiring, not the
// numbers 0 and infinity: `zero` is the neutral element of `plus` -- the value
// of an empty path -- and `infty` its absorbing element -- the value of a path
// that does not exist. `less` orders values from best to worst, so it is not
// always `std::less`. Whenever `plus` is not addition these take values that
// read as inverted; each struct below says which.
//
// What a model must guarantee, none of it checkable: `plus` associative with
// `zero` neutral and `infty` absorbing, and `less(plus(a, b), a)` false for
// every value the mapped range can produce -- that last one is the precondition
// the label-setting algorithms restate over their own length maps.
//
// All four members are structurally required to be static constexpr
// *variables*, not functions: `{ S::zero } -> same_as<const value_type &>`
// only holds for an object (a function name would yield a function type), and
// `S::plus` / `S::less` must be objects of `plus_t` / `less_t` for the same
// reason -- hence `static constexpr plus_t plus{}` in every model below,
// rather than a static member function.
// clang-format off
template <typename S>
concept semiring = requires(typename S::value_type v) {
    { S::zero } -> std::same_as<const typename S::value_type &>;
    { S::infty } -> std::same_as<const typename S::value_type &>;
    { S::plus } -> std::same_as<const typename S::plus_t &>;
    { S::less } -> std::same_as<const typename S::less_t &>;
    { S::plus(v, v) } -> std::same_as<typename S::value_type>;
    { S::less(v, v) } -> std::convertible_to<bool>;
};
// clang-format on

template <typename T>
struct shortest_path_semiring {
    using value_type = T;
    using plus_t = typename std::plus<T>;
    using less_t = typename std::less<T>;
    static constexpr T zero = static_cast<T>(0);
    static constexpr T infty = std::numeric_limits<T>::max();
    static constexpr plus_t plus{};
    static constexpr less_t less{};
};

// Path value is the product of the arc reliabilities in [0, 1], maximised:
// hence zero == 1, an empty path being perfectly reliable, infty == 0, an
// absent path being hopeless, and less == greater, larger being better.
template <typename T>
struct most_reliable_path_semiring {
    using value_type = T;
    using plus_t = typename std::multiplies<T>;
    using less_t = typename std::greater<T>;
    static constexpr T zero = static_cast<T>(1);
    static constexpr T infty = static_cast<T>(0);
    static constexpr plus_t plus{};
    static constexpr less_t less{};
};

// Path value is the smallest capacity along it, maximised: hence zero == the
// largest representable value, an empty path constraining nothing, infty == 0,
// and less == greater, larger being better.
template <typename T>
struct max_capacity_path_semiring {
    using value_type = T;
    struct plus_t {
        constexpr T operator()(const T & a, const T & b) const {
            return std::min(a, b);
        }
    };
    using less_t = typename std::greater<T>;
    static constexpr T zero = std::numeric_limits<T>::max();
    static constexpr T infty = static_cast<T>(0);
    static constexpr plus_t plus{};
    static constexpr less_t less{};
};

// Not a semiring: `plus` discards the accumulated value and keeps the last
// arc's, so combining can improve a value and the requirement above is broken
// on purpose. Feeding it to dijkstra is what turns that loop into Prim's
// algorithm, and the resulting dist() is an arc weight, not a path value.
template <typename T>
struct minimum_spanning_tree_semiring {
    using value_type = T;
    struct plus_t {
        constexpr T operator()(const T &, const T & b) const { return b; }
    };
    using less_t = typename std::less<T>;
    static constexpr T zero = static_cast<T>(0);
    static constexpr T infty = std::numeric_limits<T>::max();
    static constexpr plus_t plus{};
    static constexpr less_t less{};
};

}  // namespace melon
