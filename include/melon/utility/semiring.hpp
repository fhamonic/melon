#ifndef MELON_UTILITY_SEMIRING_HPP
#define MELON_UTILITY_SEMIRING_HPP

#include <concepts>

namespace fhamonic {
namespace melon {

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

template <typename T>
struct max_capacity_path_semiring {
    using value_type = T;
    struct plus_t {
        T operator()(const T & a, const T & b) const { return std::min(a, b); }
    };
    using less_t = typename std::greater<T>;
    static constexpr T zero = std::numeric_limits<T>::max();
    static constexpr T infty = static_cast<T>(0);
    static constexpr plus_t plus{};
    static constexpr less_t less{};
};

template <typename T>
struct minimum_spanning_tree_semiring {
    // Not an actual semiring but corresponds to the Prim algorithm
    using value_type = T;
    struct plus_t {
        T operator()(const T &, const T & b) { return b; }
    };
    using less_t = typename std::less<T>;
    static constexpr T zero = static_cast<T>(0);
    static constexpr T infty = std::numeric_limits<T>::max();
    static constexpr plus_t plus{};
    static constexpr less_t less{};
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILITY_SEMIRING_HPP
