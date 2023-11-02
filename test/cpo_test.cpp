#undef NDEBUG
#include <gtest/gtest.h>

#include <iostream>
#include <ranges>
#include <vector>

using G = std::vector<std::vector<std::size_t>>;

constexpr auto vertices(const G & g) { return std::views::iota(0ul, g.size()); }
constexpr auto out_arcs(const G & g, std::size_t v) {
    return std::views::iota(g[v].cbegin(), g[v].cend());
}
constexpr auto arc_target(const G & g, const auto & a) { return *a; }
template <typename V>
constexpr auto create_vertex_map(const G & g) {
    return std::vector<V>(g.size());
}
template <typename V>
constexpr auto create_vertex_map(const G & g, const V & d) {
    return std::vector<V>(g.size(), d);
}

#include "melon/graph.hpp"

using namespace fhamonic;

GTEST_TEST(CPO, test) {
    static_assert(melon::graph<G>);
    static_assert(melon::has_nb_vertices<G>);
    static_assert(!melon::has_nb_arcs<G>);
    static_assert(melon::has_out_arcs<G>);
    static_assert(melon::has_out_degree<G>);
    static_assert(melon::outward_incidence_graph<G>);
    static_assert(melon::outward_adjacency_graph<G>);
    static_assert(!melon::has_in_arcs<G>);
    static_assert(!melon::has_in_degree<G>);
    static_assert(!melon::inward_incidence_graph<G>);
    static_assert(!melon::inward_adjacency_graph<G>);
    static_assert(melon::has_vertex_map<G>);
    static_assert(!melon::has_arc_map<G>);

    static_assert(std::ranges::contiguous_range<G>);
    static_assert(std::ranges::contiguous_range<
                  decltype(std::ranges::views::all(std::declval<G>()))>);

    G vec = {{1, 2}, {2}, {0, 1}};
    for(auto && v : melon::vertices(vec)) {
        std::cout << v << ':' << std::endl;
        for(auto && w : melon::out_neighbors(vec, v))
            std::cout << '\t' << w << std::endl;
    }

    for(auto && p : melon::arcs_entries(vec)) {
        std::cout << '(' << *p.first << ",(" << p.second.first << ','
                  << p.second.second << "))" << std::endl;
        ASSERT_EQ(*p.first, p.second.second);
    }

    auto map = melon::create_vertex_map<double>(vec);
}