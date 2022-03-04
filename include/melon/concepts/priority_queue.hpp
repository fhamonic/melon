#ifndef MELON_CONCEPTS_PRIORITY_QUEUE_HPP
#define MELON_CONCEPTS_PRIORITY_QUEUE_HPP

#include <concepts>
#include <ranges>

#include "melon/concepts/detail/range_of.hpp"

namespace fhamonic {
namespace melon {
namespace concepts {

// clang-format off
// template <typename P>
// concept priority_queue = std::semiregular<P> && requires(P p) {
//     { g.nodes() } -> detail::range_of<typename G::Node>;
//     { g.arcs() } -> detail::range_of<typename G::Arc>;
//     { g.source(a) } -> std::same_as<typename G::Node>;
//     { g.target(a) } -> std::same_as<typename G::Node>;
//     { g.arcs_pairs() }
//         -> detail::range_of<std::pair<typename G::Node, typename G::Node>>;
// };
// clang-format on

}  // namespace concepts
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_PRIORITY_QUEUE_HPP