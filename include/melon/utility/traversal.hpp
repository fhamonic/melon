#ifndef MELON_CONCEPTS_TRAVERSAL_HPP
#define MELON_CONCEPTS_TRAVERSAL_HPP

#include <concepts>

namespace fhamonic {
namespace melon {

// clang-format off
template <typename A>
concept traversal_algorithm = requires(A alg) {
    { alg.finished() } -> std::convertible_to<bool>;
    alg.current();
    alg.advance();
};
// clang-format on

template <typename A>
    requires traversal_algorithm<A>
using traversal_entry_t = std::decay_t<decltype(std::declval<A &&>().current())>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_TRAVERSAL_HPP
