#undef NDEBUG
#include <gtest/gtest.h>

#include <new>
#include <utility>
#include <vector>

#include "melon/container/static_digraph.hpp"
#include "melon/container/static_filter_map.hpp"
#include "melon/container/static_map.hpp"
#include "melon/mapping.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// static_map is a copyable random-access range whose typedefs tell the truth
////////////////////////////////////////////////////////////////////////////////

static_assert(std::copyable<static_map<std::size_t, int>>);
static_assert(std::ranges::random_access_range<static_map<std::size_t, int>>);
static_assert(
    output_mapping_of<static_map<std::size_t, int>, std::size_t, int>);

// regression: value_type said std::pair<const K, V &> and reference said the
// same, while begin()/end() are plain V pointers -- so the container's own
// typedefs contradicted std::iterator_traits and std::ranges::range_value_t.
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

////////////////////////////////////////////////////////////////////////////////
// static_map is constructible empty, sized, value-filled, from ranges, and by
// copy/move

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

// regression: the parameters were `IT && it_begin, IT && it_end`. Deducing IT
// from lvalues gave IT = T &, which does not model random_access_iterator (so
// the constructor silently vanished), and mixing value categories gave
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

GTEST_TEST(static_map, copy_constructor) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    static_map<std::size_t, int> map(datas.begin(), datas.end());
    static_map<std::size_t, int> map2(map);
    ASSERT_TRUE(EQ_RANGES(std::as_const(map), std::as_const(map2)));
}

// regression: greedy single-argument constructor. static_map's range
// constructor has the same shape and used to win against the copy constructor
// for a non-const lvalue. It is the one site where the hijack was harmless --
// both constructors delegate to the very same iterator constructor -- so this
// pins the behaviour rather than repairing a break, and checks that guarding
// it did not cost the range constructor its real job.
GTEST_TEST(static_map, copying_a_mutable_lvalue_uses_the_copy_constructor) {
    static_map<std::size_t, int> map(3, 7);
    map[1] = 5;

    static_map<std::size_t, int> from_mutable_lvalue(map);
    ASSERT_EQ(from_mutable_lvalue.size(), 3u);
    ASSERT_TRUE(EQ_RANGES(from_mutable_lvalue, map));

    from_mutable_lvalue[0] = 42;
    ASSERT_EQ(map[0], 7);  // an independent copy

    static_map<std::size_t, int> from_const_lvalue(std::as_const(map));
    ASSERT_TRUE(EQ_RANGES(from_const_lvalue, map));

    // a plain range, and a *different* static_map specialization, still go
    // through the range constructor
    std::vector<int> values = {1, 2, 3};
    static_map<std::size_t, int> from_range(values);
    ASSERT_TRUE(EQ_RANGES(from_range, values));

    static_map<unsigned int, short> shorts(3u, short{9});
    static_map<std::size_t, int> from_other_specialization(shorts);
    ASSERT_TRUE(EQ_RANGES(from_other_specialization, {9, 9, 9}));
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

////////////////////////////////////////////////////////////////////////////////
// operator[] and range iteration read and write the mapped values
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// data() exposes the contiguous storage, const-correctly
////////////////////////////////////////////////////////////////////////////////

// regression: data() const used to return a non-const mapped_type*, so a const
// static_map handed out a mutable window onto its own storage. It was shaped
// that way to satisfy contiguous_mapping, which demanded exactly `V *`; the
// concept was relaxed to accept `const V *` instead.
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

////////////////////////////////////////////////////////////////////////////////
// fill() reports noexcept only when the value's copy assignment cannot throw
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// reset() keeps nothing; resize() keeps the elements that still fit
////////////////////////////////////////////////////////////////////////////////

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

// The two names now mean two different things on static_map: reset() keeps
// nothing, resize() keeps what still fits. static_filter_map has only reset().
template <typename M>
concept has_resize = requires(M & m) { m.resize(std::size_t{1}); };
template <typename M>
concept has_reset = requires(M & m) { m.reset(std::size_t{1}); };
static_assert(has_resize<static_map<std::size_t, int>>);
static_assert(has_reset<static_map<std::size_t, int>>);
static_assert(!has_resize<static_filter_map<std::size_t>>);
static_assert(has_reset<static_filter_map<std::size_t>>);

GTEST_TEST(static_map, resize_preserves_the_overlapping_prefix) {
    static_map<std::size_t, int> map(4);
    for(std::size_t i = 0; i < 4; ++i) map[i] = static_cast<int>(i) + 1;

    map.resize(2);  // shrink
    ASSERT_EQ(map.size(), 2u);
    ASSERT_EQ(map[0], 1);
    ASSERT_EQ(map[1], 2);

    map.resize(5);  // grow: the prefix survives, the tail is indeterminate
    ASSERT_EQ(map.size(), 5u);
    ASSERT_EQ(map[0], 1);
    ASSERT_EQ(map[1], 2);

    map.resize(5);  // no-op at the same size
    ASSERT_EQ(map.size(), 5u);
    ASSERT_EQ(map[0], 1);
}

namespace resize_probes {
// counts its own copy assignments, so `std::copy` and `std::move` over the
// surviving prefix can be told apart -- both produce the same values
struct counted {
    static inline int copy_assigns = 0;
    int v = 0;
    counted() = default;
    counted(const counted &) = default;
    counted(counted &&) = default;
    counted & operator=(const counted & o) {
        v = o.v;
        ++copy_assigns;
        return *this;
    }
    counted & operator=(counted && o) noexcept {
        v = o.v;
        return *this;
    }
};

// throws out of the default constructor, which is what
// make_unique_for_overwrite runs for a class type -- the only way to make the
// allocation step of resize() fail on demand
struct throwing {
    static inline bool armed = false;
    int v = 0;
    throwing() {
        if(armed) throw std::bad_alloc{};
    }
    throwing(const throwing &) = default;
    throwing(throwing &&) = default;
    throwing & operator=(const throwing &) = default;
    throwing & operator=(throwing &&) = default;
};
}  // namespace resize_probes

// It moves the surviving prefix rather than copying it: the old buffer is
// about to be destroyed.
GTEST_TEST(static_map, resize_moves_its_elements) {
    using counted = resize_probes::counted;
    static_map<std::size_t, counted> map(2);
    map[0].v = 1;
    map[1].v = 2;

    counted::copy_assigns = 0;
    map.resize(4);
    ASSERT_EQ(map.size(), 4u);
    ASSERT_EQ(map[0].v, 1);
    ASSERT_EQ(map[1].v, 2);
    ASSERT_EQ(counted::copy_assigns, 0);
}

// And it allocates before touching _data. Moving _data out first left the map
// holding a null buffer and its *old* size when the allocation threw, so the
// next operator[] dereferenced null.
GTEST_TEST(static_map, a_failed_resize_leaves_the_map_untouched) {
    using throwing = resize_probes::throwing;
    static_map<std::size_t, throwing> map(2);
    map[0].v = 1;
    map[1].v = 2;

    throwing::armed = true;
    ASSERT_THROW(map.resize(4), std::bad_alloc);
    throwing::armed = false;

    ASSERT_EQ(map.size(), 2u);
    ASSERT_NE(map.data(), nullptr);
    ASSERT_EQ(map[0].v, 1);
    ASSERT_EQ(map[1].v, 2);
}
