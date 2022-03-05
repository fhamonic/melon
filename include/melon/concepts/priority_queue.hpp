#ifndef MELON_CONCEPTS_PRIORITY_QUEUE_HPP
#define MELON_CONCEPTS_PRIORITY_QUEUE_HPP

#include <concepts>
#include <ranges>

#include "melon/concepts/detail/range_of.hpp"

namespace fhamonic {
namespace melon {
namespace concepts {

// clang-format off
template <typename P>
concept priority_queue = std::semiregular<P> && requires(P p, typename P::key k, typename P::value v) {
    { g.vertices() } -> detail::range_of<typename G::vertex>;
    { g.arcs() } -> detail::range_of<typename G::arc>;
};
// clang-format on

}  // namespace concepts
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_PRIORITY_QUEUE_HPP