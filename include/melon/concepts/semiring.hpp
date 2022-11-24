#ifndef MELON_CONCEPT_SEMIRING_HPP
#define MELON_CONCEPT_SEMIRING_HPP

#include <concepts>

namespace fhamonic {
namespace melon {

// clang-format off
namespace concepts {
template <typename S>
concept semiring = requires(typename S::value_type v) {
    { S::zero } -> std::same_as<const typename S::value_type &>;
    { S::infty } -> std::same_as<const typename S::value_type &>;
    { S::plus(v, v) } -> std::same_as<typename S::value_type>;
    { S::less(v, v) } -> std::convertible_to<bool>;
};
}  // namespace concepts
// clang-format on

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPT_SEMIRING_HPP
