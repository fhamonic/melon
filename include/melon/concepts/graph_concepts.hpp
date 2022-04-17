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
concept graph = std::semiregular<G> && requires(G g, typename G::vertex_t u,
                                        typename G::arc_t a) {
    { g.vertices() } -> detail::range_of<typename G::vertex_t>;
    { g.arcs() } -> detail::range_of<typename G::arc_t>;
    { g.arcs_pairs() }
        -> detail::range_of<std::pair<typename G::vertex_t, typename G::vertex_t>>;
};

template <typename G>
concept has_arc_source = requires(G g, typename G::arc_t a) {
    { g.source(a) } -> std::same_as<typename G::vertex_t>;
};

template <typename G>
concept has_arc_target = requires(G g, typename G::arc_t a) {
    { g.target(a) } -> std::same_as<typename G::vertex_t>;
};


template <typename G>
concept incidence_list_graph = graph<G> && requires(G g, typename G::vertex_t u) {
    { g.out_arcs(u) } -> detail::range_of<typename G::arc_t>;
};

template <typename G>
concept adjacency_list_graph = graph<G> && requires(G g, typename G::vertex_t u,
                                        typename G::arc_t a) {
    { g.out_neighbors(u) } -> detail::range_of<typename G::vertex_t>;
};


template <typename G>
concept reversible_incidence_list_graph = graph<G> && requires(G g,
                                        typename G::vertex_t u) {
    { g.in_arcs(u) } -> detail::range_of<typename G::arc_t>;
};

template <typename G>
concept reversible_adjacency_list_graph = graph<G> && requires(G g,
                                    typename G::vertex_t u, typename G::arc_t a) {
    { g.in_neighbors(u) } -> detail::range_of<typename G::vertex_t>;
};
// clang-format on

}  // namespace concepts
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_GRAPH_HPP