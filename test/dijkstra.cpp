#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/algorithm/dijkstra.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

GTEST_TEST(dijkstra, test) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 7)
        .add_arc(0, 2, 9)
        .add_arc(0, 5, 14)
        .add_arc(1, 0, 7)
        .add_arc(1, 2, 10)
        .add_arc(1, 3, 15)
        .add_arc(2, 0, 9)
        .add_arc(2, 1, 10)
        .add_arc(2, 3, 12)
        .add_arc(2, 5, 2)
        .add_arc(3, 1, 15)
        .add_arc(3, 2, 12)
        .add_arc(3, 4, 6)
        .add_arc(4, 3, 6)
        .add_arc(4, 5, 9)
        .add_arc(5, 0, 14)
        .add_arc(5, 2, 2)
        .add_arc(5, 4, 9);

    auto [graph, length_map] = builder.build();

    dijkstra alg(graph, length_map);

    static_assert(std::copyable<decltype(alg)>);
    std::cout << "dijkstra size: " << sizeof(decltype(alg)) << std::endl;

    alg.add_source(0);
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(0u, 0));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(1u, 7));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(2u, 9));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(5u, 11));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(4u, 20));
    alg.advance();
    ASSERT_FALSE(alg.finished());
    ASSERT_EQ(alg.current(), std::make_pair(3u, 21));
    alg.advance();
    ASSERT_TRUE(alg.finished());
    alg.reset();
}

GTEST_TEST(dijkstra, algorithm_iterator) {
    static_digraph_builder<static_digraph, int> builder(6);

    builder.add_arc(0, 1, 7)
        .add_arc(0, 2, 9)
        .add_arc(0, 5, 14)
        .add_arc(1, 0, 7)
        .add_arc(1, 2, 10)
        .add_arc(1, 3, 15)
        .add_arc(2, 0, 9)
        .add_arc(2, 1, 10)
        .add_arc(2, 3, 12)
        .add_arc(2, 5, 2)
        .add_arc(3, 1, 15)
        .add_arc(3, 2, 12)
        .add_arc(3, 4, 6)
        .add_arc(4, 3, 6)
        .add_arc(4, 5, 9)
        .add_arc(5, 0, 14)
        .add_arc(5, 2, 2)
        .add_arc(5, 4, 9);

    auto [graph, length_map] = builder.build();

    dijkstra alg(graph, length_map);
    alg.add_source(0u);

    static_assert(std::ranges::input_range<decltype(alg)>);
    static_assert(std::ranges::viewable_range<decltype(alg)>);

    std::vector traversal = {std::make_pair(0u, 0),  std::make_pair(1u, 7),
                             std::make_pair(2u, 9),  std::make_pair(5u, 11),
                             std::make_pair(4u, 20), std::make_pair(3u, 21)};

    std::size_t cpt = 0;
    for(const auto v : alg) {
        ASSERT_EQ(v, traversal[cpt]);
        ++cpt;
    }
    ASSERT_EQ(cpt, traversal.size());
}

struct dijkstra_traits {
    using semiring = shortest_path_semiring<int>;
    using heap =
        updatable_d_ary_heap<2, std::pair<vertex_t<static_digraph>, int>,
                             semiring::less_t,
                             vertex_map_t<static_digraph, std::size_t>,
                             views::element_map<1>, views::element_map<0>>;

    static constexpr bool store_distances = false;
    static constexpr bool store_paths = true;
};

GTEST_TEST(dijkstra, path_to) {
    static_digraph_builder<static_digraph, int, int> builder(6);

    builder.add_arc(0, 1, 7, 1)
        .add_arc(0, 2, 9, 2)
        .add_arc(0, 5, 14, 3)
        .add_arc(1, 0, 7, 3)
        .add_arc(1, 2, 10, 4)
        .add_arc(1, 3, 15, 5)
        .add_arc(2, 0, 9, 6)
        .add_arc(2, 1, 10, 7)
        .add_arc(2, 3, 12, 8)
        .add_arc(2, 5, 2, 9)
        .add_arc(3, 1, 15, 10)
        .add_arc(3, 2, 12, 11)
        .add_arc(3, 4, 6, 12)
        .add_arc(4, 3, 6, 13)
        .add_arc(4, 5, 9, 14)
        .add_arc(5, 0, 14, 15)
        .add_arc(5, 2, 2, 16)
        .add_arc(5, 4, 9, 17);

    auto [graph, length_map, id] = builder.build();

    auto alg = dijkstra(dijkstra_traits{}, graph, length_map);
    alg.add_source(0u).run();

    auto path = alg.path_to(3u);
    static_assert(std::ranges::forward_range<decltype(path)>);
    static_assert(std::ranges::borrowed_range<decltype(path)>);
    static_assert(std::ranges::viewable_range<decltype(path)>);

    ASSERT_TRUE(EQ_MULTISETS(
        std::views::transform(path, [&id](const auto & a) { return id[a]; }),
        {2, 8}));
}

// ################## regression: noexcept honesty ############################

// advance(), add_source(), run() and reset() push into the heap -- which
// allocates -- and run the user's length map and semiring, so an unconditional
// noexcept turned any throw from those into std::terminate. The same applied
// to algorithm_iterator, which forwards straight into advance().
namespace {
using probe_dijkstra =
    dijkstra<graph_ref_view<static_digraph>,
             mapping_ref_view<const static_map<unsigned int, int>>,
             dijkstra_default_traits<static_digraph, int>>;
using probe_iterator = decltype(std::declval<probe_dijkstra &>().begin());
}  // namespace

static_assert(!noexcept(std::declval<probe_dijkstra &>().advance()));
static_assert(!noexcept(std::declval<probe_dijkstra &>().run()));
static_assert(!noexcept(std::declval<probe_dijkstra &>().reset()));
static_assert(!noexcept(std::declval<probe_dijkstra &>().add_source(
    std::declval<const unsigned int &>())));
static_assert(!noexcept(++std::declval<probe_iterator &>()));
static_assert(noexcept(std::declval<const probe_dijkstra &>().finished()));

// ########## regression: current_dist was gated on store_distances ###########

// current_dist reads the heap, not _distances_map, so requiring
// store_distances kept it out of reach of every default-configured dijkstra.
namespace {
struct traits_without_distances : dijkstra_default_traits<static_digraph, int> {
    static constexpr bool store_distances = false;
    static constexpr bool store_paths = false;
};
template <typename D>
concept has_current_dist =
    requires(const D & d, const unsigned int & u) { d.current_dist(u); };
}  // namespace

static_assert(dijkstra_trait<traits_without_distances>);

GTEST_TEST(dijkstra, current_dist_without_store_distances) {
    static_digraph_builder<static_digraph, int> builder(3);
    builder.add_arc(0u, 1u, 4);
    builder.add_arc(1u, 2u, 6);
    auto [graph, length_map] = builder.build();

    dijkstra algo(traits_without_distances{}, graph, length_map, 0u);
    // the whole point: available even though store_distances is false
    static_assert(!traits_without_distances::store_distances);
    static_assert(has_current_dist<decltype(algo)>);
    algo.advance();  // settles 0, puts 1 in the heap at distance 4
    ASSERT_TRUE(algo.reached(1u));
    ASSERT_FALSE(algo.visited(1u));
    ASSERT_EQ(algo.current_dist(1u), 4);
}
