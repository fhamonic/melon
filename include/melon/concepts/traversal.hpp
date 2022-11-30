#ifndef MELON_CONCEPTS_TRAVERSAL_HPP
#define MELON_CONCEPTS_TRAVERSAL_HPP

#include <concepts>

namespace fhamonic {
namespace melon {

// clang-format off
namespace concepts {
template <typename A>
concept traversal_algorithm = requires(A alg, typename A::traversal_entry) {
    { alg.finished() } -> std::convertible_to<bool>;
    { alg.current() } -> std::same_as<typename A::traversal_entry>;
    alg.advance();
};
}  // namespace concepts
// clang-format on

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_TRAVERSAL_HPP
