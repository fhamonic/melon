#undef NDEBUG
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/container/static_map.hpp"
#include "melon/mapping.hpp"
#include "melon/maps/element.hpp"
#include "melon/utility/priority_queue.hpp"

using namespace melon;

// The two concepts of this header are the contract every melon algorithm
// states about the heap it is handed, so what matters is that the shipped
// heaps satisfy them and that near-misses are rejected.

////////////////////////////////////////////////////////////////////////////////
// the shipped heaps model the concepts
////////////////////////////////////////////////////////////////////////////////

using plain_heap = d_ary_heap<2, int>;

// The heap dijkstra instantiates: entries are (vertex, distance) pairs keyed
// by their first element and prioritised by their second.
using dijkstra_heap =
    updatable_d_ary_heap<2, std::pair<unsigned int, int>, std::less<int>,
                         static_map<unsigned int, std::size_t>,
                         maps::element<1>, maps::element<0>>;

static_assert(priority_queue<plain_heap>);
static_assert(priority_queue<d_ary_heap<4, double>>);
static_assert(priority_queue<dijkstra_heap>);

static_assert(updatable_priority_queue<dijkstra_heap>);
// A plain heap has no handle on its entries, so it cannot be updated.
static_assert(!updatable_priority_queue<plain_heap>);

////////////////////////////////////////////////////////////////////////////////
// every concept requirement is load-bearing: near-misses are rejected
////////////////////////////////////////////////////////////////////////////////

// std::priority_queue is the obvious near-miss: it has no clear(), which
// every melon algorithm's reset() needs. (Its by-reference top() is fine --
// see top_by_reference below.)
static_assert(!priority_queue<std::priority_queue<int>>);
static_assert(!priority_queue<std::vector<int>>);
static_assert(!priority_queue<int>);

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

// top() and empty() must be const-callable: the algorithms read them through
// const members (dijkstra's current()/finished()), so a heap declaring them
// non-const would otherwise model the concept and hard-error inside the
// algorithm.
struct nonconst_reads : complete_queue {
    value_type top();
    bool empty();
};
static_assert(!priority_queue<nonconst_reads>);

// The std::priority_queue shape of top() -- by const reference -- is
// accepted: the concept asks for convertible_to<value_type>, not
// same_as, so a heap may hand back either a copy or a reference.
// d_ary_heap itself returns by reference.
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
static_assert(priority_queue<top_by_reference>);

// But a top() whose result does not convert to value_type is still rejected.
struct top_unrelated {
    using value_type = int;
    using size_type = std::size_t;
    void push(value_type);
    void * top() const;
    void pop();
    size_type size() const;
    bool empty() const;
    void clear();
};
static_assert(!priority_queue<top_unrelated>);

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

////////////////////////////////////////////////////////////////////////////////
// driven through the concept surface alone, the heaps behave as an algorithm
// expects
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// priority() may return by const reference, the STL shape -- the concept asks
// convertible_to, not same_as, exactly like top()
////////////////////////////////////////////////////////////////////////////////

namespace {
struct by_const_ref_queue {
    using value_type = int;
    using size_type = std::size_t;
    using id_type = int;
    using priority_type = int;
    void push(int);
    const int & top() const;
    void pop();
    std::size_t size() const;
    bool empty() const;
    void clear();
    bool contains(int) const;
    const int & priority(int) const;
    void promote(int, int);
    void demote(int, int);
};
}  // namespace
static_assert(updatable_priority_queue<by_const_ref_queue>);

////////////////////////////////////////////////////////////////////////////////
// movable + default_initializable, not semiregular: a heap owning its buffer
// through a move-only handle still parameterizes traits
////////////////////////////////////////////////////////////////////////////////

namespace move_only {
struct heap {
    using value_type = int;
    using size_type = std::size_t;
    std::unique_ptr<std::vector<int>> _buffer =
        std::make_unique<std::vector<int>>();
    void push(value_type v) { _buffer->push_back(v); }
    [[nodiscard]] value_type top() const { return _buffer->back(); }
    void pop() { _buffer->pop_back(); }
    [[nodiscard]] size_type size() const { return _buffer->size(); }
    [[nodiscard]] bool empty() const { return _buffer->empty(); }
    void clear() { _buffer->clear(); }
};
}  // namespace move_only
static_assert(!std::copyable<move_only::heap>);
static_assert(priority_queue<move_only::heap>);
