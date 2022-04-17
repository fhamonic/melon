#ifndef MELON_CONCEPTS_GRAPH_HPP
#define MELON_CONCEPTS_GRAPH_HPP

#include <concepts>
#include <ranges>

#include "melon/concepts/detail/range_of.hpp"

namespace fhamonic {
namespace melon {
namespace concepts {

// clang-format off
template <typename E>
using graph_vertex_t = typename std::remove_reference<E>::type::vertex_t;

template <typename E>
using graph_arc_t = typename std::remove_reference<E>::type::arc_t;


template <typename G>
concept graph = std::semiregular<G> && requires(G g, graph_vertex_t<G> u,
                                        graph_arc_t<G> a) {
    { g.vertices() } -> detail::range_of<graph_vertex_t<G>>;
    { g.arcs() } -> detail::range_of<graph_arc_t<G>>;
    { g.arcs_pairs() }
        -> detail::range_of<std::pair<graph_vertex_t<G>, graph_vertex_t<G>>>;
};

template <typename G>
concept has_arc_source = requires(G g, graph_arc_t<G> a) {
    { g.source(a) } -> std::same_as<graph_vertex_t<G>>;
};

template <typename G>
concept has_arc_target = requires(G g, graph_arc_t<G> a) {
    { g.target(a) } -> std::same_as<graph_vertex_t<G>>;
};


template <typename G>
concept incidence_list_graph = graph<G> && requires(G g, graph_vertex_t<G> u) {
    { g.out_arcs(u) } -> detail::range_of<graph_arc_t<G>>;
};

template <typename G>
concept adjacency_list_graph = graph<G> && requires(G g, graph_vertex_t<G> u,
                                        graph_arc_t<G> a) {
    { g.out_neighbors(u) } -> detail::range_of<graph_vertex_t<G>>;
};


template <typename G>
concept reversible_incidence_list_graph = graph<G> && requires(G g,
                                        graph_vertex_t<G> u) {
    { g.in_arcs(u) } -> detail::range_of<graph_arc_t<G>>;
};

template <typename G>
concept reversible_adjacency_list_graph = graph<G> && requires(G g,
                                    graph_vertex_t<G> u, graph_arc_t<G> a) {
    { g.in_neighbors(u) } -> detail::range_of<graph_vertex_t<G>>;
};
// clang-format on

}  // namespace concepts
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_GRAPH_HPP