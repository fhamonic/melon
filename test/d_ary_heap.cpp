#undef NDEBUG
#include <gtest/gtest.h>

#include <array>
#include <numeric>
#include <queue>

#include "melon/container/d_ary_heap.hpp"
#include "melon/maps/element.hpp"
#include "melon/utility/priority_queue.hpp"

#include "random_ranges_helper.hpp"
#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// d_ary_heap pops pushed elements in priority order, for any arity
////////////////////////////////////////////////////////////////////////////////

// top() has the std::priority_queue shape -- a reference into the heap
// array, invalidated by push/pop/promote/demote -- not a copy per call.
static_assert(
    std::same_as<decltype(std::declval<const d_ary_heap<2, int> &>().top()),
                 const int &>);

GTEST_TEST(d_ary_heap, 2_heap_push_pop_test) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    d_ary_heap<2, int> heap;

    static_assert(priority_queue<decltype(heap)>);

    for(auto && e : datas) {
        heap.push(e);
    }

    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 11);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 7);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 6);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 5);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 3);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 0);
    heap.pop();
    ASSERT_TRUE(heap.empty());
}

GTEST_TEST(d_ary_heap, 2_heap_fuzzy_push_pop_test) {
    for(int it = 0; it < 10; ++it) {
        std::size_t size = 127;
        std::vector<int> datas = random_vector_all_diff(size, 0, 1000);
        std::vector<std::size_t> permuted_id(size);
        std::iota(permuted_id.begin(), permuted_id.end(), 0);
        auto zip_view = std::views::zip(datas, permuted_id);

        d_ary_heap<2, std::pair<std::size_t, int>, std::greater<int>,
                   maps::element<1>>
            heap;
        for(std::size_t i = 0; i < size; ++i) {
            heap.push(std::make_pair(i, datas[i]));
        }

        std::ranges::sort(zip_view, [](auto p1, auto p2) {
            return std::get<0>(p1) > std::get<0>(p2);
        });
        for(std::size_t i = 0; i < size; ++i) {
            ASSERT_FALSE(heap.empty());
            ASSERT_EQ(heap.top(), std::make_pair(permuted_id[i], datas[i]));
            heap.pop();
        }
        ASSERT_TRUE(heap.empty());
    }
}
GTEST_TEST(d_ary_heap, 3_heap_fuzzy_push_pop_test) {
    for(int it = 0; it < 10; ++it) {
        std::size_t size = 127;
        std::vector<int> datas = random_vector_all_diff(size, 0, 1000);
        std::vector<std::size_t> permuted_id(size);
        std::iota(permuted_id.begin(), permuted_id.end(), 0);
        auto zip_view = std::views::zip(datas, permuted_id);

        d_ary_heap<3, std::pair<std::size_t, int>, std::greater<int>,
                   maps::element<1>>
            heap;
        for(std::size_t i = 0; i < size; ++i) {
            heap.push(std::make_pair(i, datas[i]));
        }

        std::ranges::sort(zip_view, [](auto p1, auto p2) {
            return std::get<0>(p1) > std::get<0>(p2);
        });
        for(std::size_t i = 0; i < size; ++i) {
            ASSERT_FALSE(heap.empty());
            ASSERT_EQ(heap.top(), std::make_pair(permuted_id[i], datas[i]));
            heap.pop();
        }
        ASSERT_TRUE(heap.empty());
    }
}
GTEST_TEST(d_ary_heap, 4_heap_fuzzy_push_pop_test) {
    for(int it = 0; it < 10; ++it) {
        std::size_t size = 127;
        std::vector<int> datas = random_vector_all_diff(size, 0, 1000);
        std::vector<std::size_t> permuted_id(size);
        std::iota(permuted_id.begin(), permuted_id.end(), 0);
        auto zip_view = std::views::zip(datas, permuted_id);

        d_ary_heap<4, std::pair<std::size_t, int>, std::greater<int>,
                   maps::element<1>>
            heap;
        for(std::size_t i = 0; i < size; ++i) {
            heap.push(std::make_pair(i, datas[i]));
        }

        std::ranges::sort(zip_view, [](auto p1, auto p2) {
            return std::get<0>(p1) > std::get<0>(p2);
        });
        for(std::size_t i = 0; i < size; ++i) {
            ASSERT_FALSE(heap.empty());
            ASSERT_EQ(heap.top(), std::make_pair(permuted_id[i], datas[i]));
            heap.pop();
        }
        ASSERT_TRUE(heap.empty());
    }
}

////////////////////////////////////////////////////////////////////////////////
// updatable_d_ary_heap re-orders itself when a priority is promoted or demoted
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(updatable_d_ary_heap, 2_heap_promote_test) {
    std::vector<int> datas = {0, 7, 3, 5, 6, 11};
    constexpr std::size_t num_elements = 6;
    updatable_d_ary_heap<2, std::pair<unsigned int, int>, std::greater<int>,
                         std::array<std::size_t, num_elements>,
                         maps::element<1>, maps::element<0>>
        heap;

    static_assert(updatable_priority_queue<decltype(heap)>);

    for(unsigned int i = 0; i < num_elements; ++i) {
        heap.push(std::make_pair(i, datas[i]));
    }
    heap.promote(3u, 8);

    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(5u, 11));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(3u, 8));
    heap.pop();

    heap.promote(0u, 9);

    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(0u, 9));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(1u, 7));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(4u, 6));
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), std::make_pair(2u, 3));
    heap.pop();
    ASSERT_TRUE(heap.empty());
}

// regression: the maps are constructor parameters, so the heap can hold bare
// ids while the priorities live outside. Constructors that default-construct
// both maps shut every stateful priority map out.
GTEST_TEST(updatable_d_ary_heap, 2_heap_promote_external_priority_test) {
    std::vector<int> priorities = {0, 7, 3, 5, 6, 11};
    constexpr std::size_t num_elements = 6;
    auto priority_map = maps::function(
        [&priorities](const unsigned int i) -> int & { return priorities[i]; });
    updatable_d_ary_heap<2, unsigned int, std::greater<int>,
                         std::array<std::size_t, num_elements>,
                         decltype(priority_map), maps::identity>
        heap(std::greater<int>(), std::array<std::size_t, num_elements>{},
             priority_map);

    for(unsigned int i = 0; i < num_elements; ++i) {
        heap.push(i);
    }
    heap.promote(3u, 8);
    // The write went through to the external vector, not to a copy.
    ASSERT_EQ(priorities[3], 8);

    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 5u);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 3u);
    heap.pop();

    heap.promote(0u, 9);
    ASSERT_EQ(priorities[0], 9);

    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 0u);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 1u);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 4u);
    heap.pop();
    ASSERT_FALSE(heap.empty());
    ASSERT_EQ(heap.top(), 2u);
    heap.pop();
    ASSERT_TRUE(heap.empty());
}

////////////////////////////////////////////////////////////////////////////////
// promote/demote require a priority map that writes into the entry
////////////////////////////////////////////////////////////////////////////////

// regression: promote()/demote() rewrite the priority inside an entry via
// `_entry_priority_map[e] = p`. With maps::identity -- the default
// EntryPriorityMap -- operator[] returns a prvalue, so the write lands on a
// temporary and is discarded: the heap silently keeps the old priority and is
// never re-ordered. The operations are constrained on a priority map that
// yields a reference into the entry.
namespace {
struct heap_item {
    int id;
    int prio;
};
struct heap_item_cmp {
    bool operator()(const heap_item & a, const heap_item & b) const {
        return a.prio < b.prio;
    }
};
struct heap_item_id_map : mapping_view_base {
    std::size_t operator[](const heap_item & i) const {
        return static_cast<std::size_t>(i.id);
    }
};

using indices_map = mapping_owning_view<std::vector<std::size_t>>;

// a priority map handing back a reference into the entry: supported
using writable_heap =
    updatable_d_ary_heap<2, std::pair<std::size_t, int>, std::greater<int>,
                         indices_map, maps::element<1>, maps::element<0>>;
// a priority map handing back a copy of the entry: rejected
using copying_heap =
    updatable_d_ary_heap<2, heap_item, heap_item_cmp, indices_map,
                         maps::identity, heap_item_id_map>;
}  // namespace

static_assert(
    mutable_entry_priority_map<maps::element<1>, std::pair<std::size_t, int>>);
static_assert(!mutable_entry_priority_map<maps::identity, heap_item>);

// the constrained members disappear rather than silently misbehaving
// (named concepts so the probe stays dependent and actually SFINAEs)
namespace {
template <typename H>
concept can_promote =
    requires(H h, typename H::id_type k, typename H::priority_type p) {
        h.promote(k, p);
    };
template <typename H>
concept can_demote = requires(H h, typename H::id_type k,
                              typename H::priority_type p) { h.demote(k, p); };
}  // namespace

static_assert(can_promote<writable_heap>);
static_assert(can_demote<writable_heap>);
static_assert(!can_promote<copying_heap>);
static_assert(!can_demote<copying_heap>);

// ...which is what makes the whole type stop modelling updatable_priority_queue
static_assert(updatable_priority_queue<writable_heap>);
static_assert(!updatable_priority_queue<copying_heap>);

// std::greater orders the heap so that the largest priority is on top, so
// "promote" raises a priority and "demote" lowers it, matching the existing
// 2_heap_promote_test above.
GTEST_TEST(updatable_d_ary_heap, promote_actually_reorders) {
    writable_heap heap(std::greater<int>{},
                       indices_map(std::vector<std::size_t>(8)));
    heap.push({0u, 50});
    heap.push({1u, 90});
    heap.push({2u, 70});

    ASSERT_EQ(heap.top().first, 1u);
    ASSERT_EQ(heap.priority(0u), 50);

    heap.promote(0u, 100);

    // a write landing on a temporary leaves the priority at 50 and the heap
    // never re-ordered
    ASSERT_EQ(heap.priority(0u), 100);
    ASSERT_EQ(heap.top().first, 0u);
    ASSERT_EQ(heap.top().second, 100);
}

GTEST_TEST(updatable_d_ary_heap, demote_actually_reorders) {
    writable_heap heap(std::greater<int>{},
                       indices_map(std::vector<std::size_t>(8)));
    heap.push({0u, 10});
    heap.push({1u, 20});
    heap.push({2u, 30});

    ASSERT_EQ(heap.top().first, 2u);

    heap.demote(2u, 5);

    ASSERT_EQ(heap.priority(2u), 5);
    ASSERT_EQ(heap.top().first, 1u);
    ASSERT_EQ(heap.top().second, 20);
}

////////////////////////////////////////////////////////////////////////////////
// contains() answers for every entry, not only the one on top
////////////////////////////////////////////////////////////////////////////////

// regression: the index map stores *byte* offsets -- the unit heap_move()
// writes and entry_ref() reads. A contains() that compares one against
// _heap_array.size(), an element count, and then subscripts _heap_array with
// it answers true only for the entry at offset 0; every other live entry
// comes back absent. A test that only queries the top -- exactly that one
// entry -- cannot see the defect, which is why these query every key.
GTEST_TEST(updatable_d_ary_heap, contains_answers_for_every_entry) {
    const std::array<int, 4> priorities = {50, 90, 70, 60};
    writable_heap heap(std::greater<int>{},
                       indices_map(std::vector<std::size_t>(8)));
    for(std::size_t k = 0; k < priorities.size(); ++k)
        heap.push({k, priorities[k]});

    ASSERT_EQ(heap.top().first, 1u);
    for(std::size_t k = 0; k < priorities.size(); ++k) {
        ASSERT_TRUE(heap.contains(k))
            << "contains(" << k << ") on a live entry";
        ASSERT_EQ(heap.priority(k), priorities[k]);
    }

    // a popped key drops out, whichever slot its stale offset now names
    heap.pop();
    ASSERT_FALSE(heap.contains(1u));
    for(std::size_t k : {0u, 2u, 3u}) ASSERT_TRUE(heap.contains(k));

    // and re-ordering loses none of the others
    heap.promote(3u, 100);
    ASSERT_EQ(heap.top().first, 3u);
    for(std::size_t k : {0u, 2u, 3u}) ASSERT_TRUE(heap.contains(k));

    while(!heap.empty()) heap.pop();
    for(std::size_t k = 0; k < priorities.size(); ++k)
        ASSERT_FALSE(heap.contains(k));
}

////////////////////////////////////////////////////////////////////////////////
// the heap's operations declare noexcept honestly
////////////////////////////////////////////////////////////////////////////////

// regression: push() grows a std::vector and sifts through the user's
// comparator and priority map; declaring it noexcept turns a bad_alloc (or a
// throwing comparator) into std::terminate. clear() is genuinely noexcept
// because it uses vector::clear rather than resize(0).
namespace {
using plain_heap = d_ary_heap<2, int, std::greater<int>>;
}  // namespace

static_assert(!noexcept(std::declval<plain_heap &>().push(1)));
static_assert(!noexcept(std::declval<plain_heap &>().pop()));
static_assert(noexcept(std::declval<plain_heap &>().clear()));
static_assert(noexcept(std::declval<const plain_heap &>().size()));
static_assert(noexcept(std::declval<const plain_heap &>().empty()));
static_assert(!noexcept(std::declval<writable_heap &>().promote(
    std::declval<const std::size_t &>(), std::declval<const int &>())));
static_assert(!noexcept(std::declval<writable_heap &>().demote(
    std::declval<const std::size_t &>(), std::declval<const int &>())));

// clear() must still actually empty the heap
GTEST_TEST(d_ary_heap, clear_empties_the_heap) {
    plain_heap heap;
    heap.push(3);
    heap.push(1);
    heap.push(2);
    ASSERT_EQ(heap.size(), 3u);
    heap.clear();
    ASSERT_TRUE(heap.empty());
    ASSERT_EQ(heap.size(), 0u);
    heap.push(7);
    ASSERT_EQ(heap.top(), 7);
}

////////////////////////////////////////////////////////////////////////////////
// a single-argument constructor call cannot hijack the copy constructor
////////////////////////////////////////////////////////////////////////////////

// regression: an unconstrained `template <typename PC> d_ary_heap_base(PC &&)`
// beats the copy constructor for a non-const lvalue of the heap type (exact
// match vs an added const) and tries to build a comparator out of a heap.
namespace {
using heap_base = d_ary_heap_base<d_ary_heap<2, int, std::greater<int>>, 2, int,
                                  std::greater<int>, maps::identity>;
}  // namespace

static_assert(std::copy_constructible<heap_base>);
static_assert(std::constructible_from<heap_base, std::greater<int>>);
static_assert(std::constructible_from<heap_base, heap_base &>);
static_assert(std::constructible_from<heap_base, const heap_base &>);

GTEST_TEST(d_ary_heap, copying_a_mutable_lvalue_uses_the_copy_constructor) {
    d_ary_heap<2, int, std::greater<int>> heap;
    heap.push(1);
    heap.push(9);
    heap.push(5);

    d_ary_heap<2, int, std::greater<int>> from_mutable_lvalue(heap);
    ASSERT_EQ(from_mutable_lvalue.size(), 3u);
    ASSERT_EQ(from_mutable_lvalue.top(), 9);

    // and the comparator constructor is still reachable
    heap_base with_comparator(std::greater<int>{});
    ASSERT_TRUE(with_comparator.empty());
}

////////////////////////////////////////////////////////////////////////////////
// std::priority_queue parity: emplace, push_range, swap and the comparator
// typedef
////////////////////////////////////////////////////////////////////////////////

static_assert(
    std::same_as<d_ary_heap<2, int>::priority_compare, std::greater<int>>);

GTEST_TEST(d_ary_heap, emplace_and_push_range) {
    d_ary_heap<2, std::pair<int, int>> heap;
    heap.emplace(1, 2);
    std::vector<std::pair<int, int>> more = {{3, 4}, {0, 1}};
    heap.push_range(more);
    ASSERT_EQ(heap.size(), 3u);
    ASSERT_EQ(heap.top(), (std::pair{3, 4}));
}

// both the member and the ADL swap, on the plain and the updatable heap --
// the updatable one must carry its id and index maps along
GTEST_TEST(d_ary_heap, member_and_adl_swap) {
    d_ary_heap<2, int> a, b;
    a.push(1);
    a.push(5);
    a.swap(b);
    ASSERT_TRUE(a.empty());
    ASSERT_EQ(b.top(), 5);
    swap(a, b);
    ASSERT_TRUE(b.empty());
    ASSERT_EQ(a.top(), 5);
}

GTEST_TEST(updatable_d_ary_heap, swap_carries_the_maps) {
    using entry = std::pair<unsigned int, int>;
    using heap_t = updatable_d_ary_heap<2, entry, std::greater<int>,
                                        std::array<std::size_t, 4>,
                                        maps::element<1>, maps::element<0>>;
    heap_t a(std::greater<int>(), std::array<std::size_t, 4>{});
    heap_t b(std::greater<int>(), std::array<std::size_t, 4>{});
    a.push({0u, 3});
    a.push({1u, 7});
    a.swap(b);
    ASSERT_TRUE(a.empty());
    ASSERT_TRUE(b.contains(1u));
    ASSERT_EQ(b.priority(1u), 7);
    b.promote(1u, 9);
    ASSERT_EQ(b.priority(1u), 9);
    swap(a, b);
    ASSERT_TRUE(b.empty());
    ASSERT_EQ(a.priority(1u), 9);
}
