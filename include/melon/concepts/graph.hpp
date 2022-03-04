#ifndef MELON_CONCEPTS_GRAPH_HPP
#define MELON_CONCEPTS_GRAPH_HPP

#include <concepts>
#include <ranges>

#include "melon/concepts/detail/range_of.hpp"

namespace fhamonic {
namespace melon {
namespace concepts {

// clang-format off
template <typename G>
concept graph = std::semiregular<G> && requires(G g, typename G::Node u,
                                        typename G::Arc a) {
    { g.nodes() } -> detail::range_of<typename G::Node>;
    { g.arcs() } -> detail::range_of<typename G::Arc>;
    { g.source(a) } -> std::same_as<typename G::Node>;
    { g.target(a) } -> std::same_as<typename G::Node>;
    { g.arcs_pairs() }
        -> detail::range_of<std::pair<typename G::Node, typename G::Node>>;
};

template <typename G>
concept adjacency_list_graph = graph<G> && requires(G g, typename G::Node u,
                                        typename G::Arc a) {
    { g.out_arcs(u) } -> detail::range_of<typename G::Arc>;
};
// clang-format on

}  // namespace concepts
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_GRAPH_HPP