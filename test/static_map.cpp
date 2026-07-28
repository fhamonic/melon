#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/container/static_digraph.hpp"
#include "melon/container/static_filter_map.hpp"
#include "melon/container/static_map.hpp"
#include "melon/mapping.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

static_assert(std::copyable<static_map<std::size_t, int>>);
static_assert(std::ranges::random_access_range<static_map<std::size_t, int>>);
static_assert(
    output_mapping_of<static_map<std::size_t, int>, std::size_t, int>);

GTEST_TEST(static_map, empty_constructor) {
    static_map<std::size_t, int> map;
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());
}

GTEST_TEST(static_map, size_constructor) {
    static_map<std::size_t, int> map(0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    static_map<std::size_t, int> map2(5);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());
}

GTEST_TEST(static_map, size_init_constructor) {
    static_map<std::size_t, int> map(0, 0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    static_map<std::size_t, int> map2(5, 113);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());

    int value = 113;
    ASSERT_TRUE(EQ_RANGES(map2, {value, value, value, value, value}));
    ASSERT_TRUE(EQ_RANGES(std::as_const(map2), {113, 113, 113, 113, 113}));
}

GTEST_TEST(static_map, range_constructor) {
    static_map<std::size_t, int> map(0, 0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    static_map<std::size_t, int> map2(datas.begin(), datas.end());
    ASSERT_EQ(map2.size(), datas.size());
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());

    ASSERT_TRUE(EQ_RANGES(
        map2, {datas[0], datas[1], datas[2], datas[3], datas[4], datas[5]}));
    ASSERT_TRUE(EQ_RANGES(std::as_const(map2), {0, 7, 3, 5, 6, 11}));
}

GTEST_TEST(static_map, copy_constructor) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    static_map<std::size_t, int> map(datas.begin(), datas.end());
    static_map<std::size_t, int> map2(map);
    ASSERT_TRUE(EQ_RANGES(std::as_const(map), std::as_const(map2)));
}

GTEST_TEST(static_map, move_constructor) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    static_map<std::size_t, int> map(datas.begin(), datas.end());
    static_map<std::size_t, int> map2(map);
    static_map<std::size_t, int> map3(std::move(map));
    ASSERT_TRUE(EQ_RANGES(std::as_const(map2), std::as_const(map3)));
}

GTEST_TEST(static_map, copy_assignement) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    static_map<std::size_t, int> map(datas.begin(), datas.end());
    static_map<std::size_t, int> map2;
    map2 = map;
    ASSERT_TRUE(EQ_RANGES(std::as_const(map), std::as_const(map2)));
}

GTEST_TEST(static_map, move_assignement) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    static_map<std::size_t, int> map(datas.begin(), datas.end());
    static_map<std::size_t, int> map2(map);
    static_map<std::size_t, int> map3;
    map3 = std::move(map);
    ASSERT_TRUE(EQ_RANGES(std::as_const(map2), std::as_const(map3)));
}

GTEST_TEST(static_map, read_operator) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    const static_map<std::size_t, int> map(datas.begin(), datas.end());
    for(std::size_t i = 0; i < map.size(); ++i) {
        ASSERT_EQ(map[i], datas[i]);
    }
}
GTEST_TEST(static_map, write_operator) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    static_map<std::size_t, int> map(datas.begin(), datas.end());
    for(std::size_t i = 0; i < map.size(); ++i) {
        ASSERT_EQ(map[i], datas[i]);
        map[i] = 3 * static_cast<int>(i) + 1;
        ASSERT_EQ(map[i], 3 * i + 1);
    }
}
GTEST_TEST(static_map, for_each_read) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    const static_map<std::size_t, int> map(datas.begin(), datas.end());
    std::size_t cpt = 0;
    for(auto && v : map) {
        ASSERT_EQ(v, datas[cpt]);
        ++cpt;
    }
}
GTEST_TEST(static_map, for_each_write) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    static_map<std::size_t, int> map(datas.begin(), datas.end());
    std::size_t cpt = 0;
    for(auto & v : map) {
        ASSERT_EQ(v, datas[cpt]);
        v = 3 * static_cast<int>(cpt) + 1;
        ASSERT_EQ(map[cpt], 3 * static_cast<int>(cpt) + 1);
        ++cpt;
    }
}
// Renamed from resize(): it reallocates and does NOT preserve the contents the
// way std::vector::resize does, so the old name silently lost callers' data.
GTEST_TEST(static_map, reset) {
    static_map<std::size_t, int> map(20);
    map.reset(10);
    ASSERT_EQ(map.size(), 10);

    // reset() to the same size is a no-op and keeps the contents
    static_map<std::size_t, int> kept(4, 7);
    kept.reset(4);
    ASSERT_EQ(kept.size(), 4);
    for(std::size_t i = 0; i < kept.size(); ++i) ASSERT_EQ(kept[i], 7);
}

// The misleading name is gone from both containers.
template <typename M>
concept has_resize = requires(M & m) { m.resize(std::size_t{1}); };
template <typename M>
concept has_reset = requires(M & m) { m.reset(std::size_t{1}); };
static_assert(!has_resize<static_map<std::size_t, int>>);
static_assert(has_reset<static_map<std::size_t, int>>);
static_assert(!has_resize<static_filter_map<std::size_t>>);
static_assert(has_reset<static_filter_map<std::size_t>>);

// ################## regression: data() const-correctness #####################

// data() const used to return a non-const mapped_type*, so a const static_map
// handed out a mutable window onto its own storage. It was shaped that way to
// satisfy contiguous_mapping, which demanded exactly `V *`; the concept was
// relaxed to accept `const V *` instead.
GTEST_TEST(static_map, data_is_const_correct) {
    static_map<std::size_t, int> map(3, 7);
    static_assert(std::is_same_v<decltype(map.data()), int *>);
    static_assert(
        std::is_same_v<decltype(std::as_const(map).data()), const int *>);

    map.data()[0] = 999;
    ASSERT_EQ(std::as_const(map).data()[0], 999);
    ASSERT_EQ(std::as_const(map)[0], 999);
}

// The relaxed concept must still hold for the container itself and, crucially,
// through a const-carrying mapping_ref_view -- that is the shape
// arc_targets_map returns, and the prefetch fast path in dijkstra keys off it.
static_assert(contiguous_mapping<static_map<std::size_t, int>, std::size_t>);
static_assert(
    contiguous_mapping_of<static_map<std::size_t, int>, std::size_t, int>);
static_assert(
    contiguous_mapping<mapping_ref_view<const static_map<std::size_t, int>>,
                       std::size_t>);
static_assert(contiguous_mapping<mapping_ref_view<static_map<std::size_t, int>>,
                                 std::size_t>);

// ############# regression: iterator-pair constructor deduction ##############

// The parameters were `IT && it_begin, IT && it_end`. Deducing IT from lvalues
// gave IT = T &, which does not model random_access_iterator (so the
// constructor silently vanished), and mixing value categories gave
// "deduced conflicting types for parameter 'IT'".
GTEST_TEST(static_map, iterator_pair_constructor_accepts_any_value_category) {
    std::vector<int> v = {4, 5, 6};
    auto b = v.begin();
    auto e = v.end();

    static_map<std::size_t, int> from_lvalues(b, e);
    static_map<std::size_t, int> from_mixed(b, v.end());
    static_map<std::size_t, int> from_rvalues(v.begin(), v.end());
    static_map<std::size_t, int> from_range(v);
    static_map<std::size_t, int> from_temporary_range(
        std::vector<int>{4, 5, 6});

    for(const auto * map : {&from_lvalues, &from_mixed, &from_rvalues,
                            &from_range, &from_temporary_range}) {
        ASSERT_EQ(map->size(), 3u);
        ASSERT_TRUE(EQ_RANGES(*map, v));
    }
}

// ################## regression: fill() noexcept honesty #####################

namespace {
struct throwing_assign {
    throwing_assign() = default;
    throwing_assign(const throwing_assign &) = default;
    throwing_assign & operator=(const throwing_assign &) { return *this; }
};
}  // namespace

static_assert(noexcept(std::declval<static_map<std::size_t, int> &>().fill(0)));
static_assert(
    !noexcept(std::declval<static_map<std::size_t, throwing_assign> &>().fill(
        std::declval<const throwing_assign &>())));

// ################### regression: typedef lies ###############################

// value_type said std::pair<const K, V &> and reference said the same, while
// begin()/end() are plain V pointers -- so the container's own typedefs
// contradicted std::iterator_traits and std::ranges::range_value_t.
namespace {
using probe_map = static_map<std::size_t, int>;
}  // namespace

static_assert(std::same_as<probe_map::value_type, int>);
static_assert(std::same_as<probe_map::reference, int &>);
static_assert(std::same_as<probe_map::const_reference, const int &>);
static_assert(
    std::same_as<probe_map::value_type, std::ranges::range_value_t<probe_map>>);
static_assert(std::same_as<probe_map::reference,
                           std::ranges::range_reference_t<probe_map>>);
static_assert(std::same_as<probe_map::const_reference,
                           std::ranges::range_reference_t<const probe_map>>);
static_assert(std::same_as<probe_map::value_type,
                           std::iter_value_t<probe_map::iterator>>);
static_assert(std::same_as<probe_map::reference,
                           std::iter_reference_t<probe_map::iterator>>);
static_assert(std::same_as<probe_map::const_reference,
                           std::iter_reference_t<probe_map::const_iterator>>);
static_assert(std::same_as<probe_map::difference_type,
                           std::iter_difference_t<probe_map::iterator>>);
