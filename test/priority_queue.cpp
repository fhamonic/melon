#undef NDEBUG
#include <gtest/gtest.h>

#include <functional>
#include <queue>
#include <utility>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/container/static_map.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/priority_queue.hpp"

using namespace melon;

// The two concepts of this header are the contract every melon algorithm
// states about the heap it is handed, so what matters is that the shipped
// heaps satisfy them and that near-misses are rejected.

using plain_heap = d_ary_heap<2, int>;

// The heap dijkstra instantiates: entries are (vertex, distance) pairs keyed
// by their first element and prioritised by their second.
using dijkstra_heap =
    updatable_d_ary_heap<2, std::pair<unsigned int, int>, std::less<int>,
                         static_map<unsigned int, std::size_t>,
                         views::element_map<1>, views::element_map<0>>;

static_assert(priority_queue<plain_heap>);
static_assert(priority_queue<d_ary_heap<4, double>>);
static_assert(priority_queue<dijkstra_heap>);

static_assert(updatable_priority_queue<dijkstra_heap>);
// A plain heap has no handle on its entries, so it cannot be updated.
static_assert(!updatable_priority_queue<plain_heap>);

// std::priority_queue is the obvious near-miss: it has no clear(), and its
// top() returns a reference rather than a value_type.
static_assert(!priority_queue<std::priority_queue<int>>);
static_assert(!priority_queue<std::vector<int>>);
static_assert(!priority_queue<int>);

// Every requirement of the concept is load-bearing: dropping any single one
// must make the concept reject the type.
namespace {
struct complete_queue {
    using value_type = int;
    using size_type = std::size_t;
    void push(value_type);
    value_type top() const;
    void pop();
    size_type size() const;
    bool empty() const;
    void clear();
};
static_assert(priority_queue<complete_queue>);

struct no_clear : complete_queue {
    void clear() = delete;
};
static_assert(!priority_queue<no_clear>);

struct top_by_reference {
    using value_type = int;
    using size_type = std::size_t;
    void push(value_type);
    const value_type & top() const;
    void pop();
    size_type size() const;
    bool empty() const;
    void clear();
};
static_assert(!priority_queue<top_by_reference>);

// std::semiregular is part of the concept: an algorithm default-constructs
// its heap before filling it.
struct not_default_constructible : complete_queue {
    not_default_constructible() = delete;
};
static_assert(!priority_queue<not_default_constructible>);

struct updatable : complete_queue {
    using id_type = unsigned int;
    using priority_type = int;
    bool contains(id_type) const;
    priority_type priority(id_type) const;
    void promote(id_type, priority_type);
    void demote(id_type, priority_type);
};
static_assert(updatable_priority_queue<updatable>);

struct no_demote : updatable {
    void demote(id_type, priority_type) = delete;
};
static_assert(priority_queue<no_demote>);
static_assert(!updatable_priority_queue<no_demote>);
}  // namespace

// The concept only describes the surface; these check the surface actually
// behaves as an algorithm would expect when driven through it alone.
template <priority_queue Q>
void drain(Q & q, std::vector<typename Q::value_type> & out) {
    while(!q.empty()) {
        out.push_back(q.top());
        q.pop();
    }
}

GTEST_TEST(priority_queue_concept, drives_a_plain_heap) {
    plain_heap heap;
    ASSERT_TRUE(heap.empty());
    ASSERT_EQ(heap.size(), 0u);

    for(int v : {3, 1, 4, 1, 5}) heap.push(v);
    ASSERT_EQ(heap.size(), 5u);

    std::vector<int> popped;
    drain(heap, popped);
    ASSERT_EQ(popped, (std::vector<int>{5, 4, 3, 1, 1}));
    ASSERT_TRUE(heap.empty());

    heap.push(2);
    heap.clear();
    ASSERT_TRUE(heap.empty());
}

GTEST_TEST(updatable_priority_queue_concept, drives_the_dijkstra_heap) {
    dijkstra_heap heap(std::less<int>(),
                       static_map<unsigned int, std::size_t>(4u));

    heap.push({0u, 10});
    heap.push({1u, 5});
    heap.push({2u, 7});

    ASSERT_TRUE(heap.contains(1u));
    ASSERT_FALSE(heap.contains(3u));
    ASSERT_EQ(heap.priority(0u), 10);
    ASSERT_EQ(heap.top().first, 1u);

    // Lowering a distance must move that entry to the top.
    heap.promote(2u, 1);
    ASSERT_EQ(heap.priority(2u), 1);
    ASSERT_EQ(heap.top(), std::make_pair(2u, 1));

    heap.demote(1u, 42);
    ASSERT_EQ(heap.priority(1u), 42);

    std::vector<unsigned int> order;
    while(!heap.empty()) {
        order.push_back(heap.top().first);
        heap.pop();
    }
    ASSERT_EQ(order, (std::vector<unsigned int>{2u, 0u, 1u}));
}
