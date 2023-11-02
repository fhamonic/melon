#undef NDEBUG
#include <gtest/gtest.h>

#include <iostream>
#include <ranges>
#include <vector>

#include "melon/container/static_digraph.hpp"
#include "melon/graph.hpp"
#include "melon/views/graph_view.hpp"

using namespace fhamonic;

using G = melon::static_digraph;

GTEST_TEST(graph_view, test) {
    static_assert(melon::graph<G>);

    static_assert(
        std::same_as<melon::views::graph_all_t<G &>, melon::graph_ref_view<G>>);
    static_assert(std::same_as<melon::views::graph_all_t<const G &>,
                               melon::graph_ref_view<const G>>);
    static_assert(std::same_as<melon::views::graph_all_t<G &&>,
                               melon::graph_owning_view<G>>);
    static_assert(std::same_as<melon::views::graph_all_t<G>,
                               melon::graph_owning_view<G>>);

    static_assert(melon::graph_view<melon::graph_ref_view<G>>);
    static_assert(melon::graph_view<melon::graph_ref_view<const G>>);
    static_assert(melon::graph_view<melon::graph_owning_view<G>>);

    static_assert(
        std::same_as<melon::views::graph_all_t<melon::graph_ref_view<G>>,
                     melon::graph_ref_view<G>>);
    static_assert(
        std::same_as<melon::views::graph_all_t<melon::graph_ref_view<const G>>,
                     melon::graph_ref_view<const G>>);
    static_assert(
        std::same_as<melon::views::graph_all_t<melon::graph_owning_view<G>>,
                     melon::graph_owning_view<G>>);
}