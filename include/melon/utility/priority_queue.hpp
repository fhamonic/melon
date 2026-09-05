#pragma once

#include <concepts>
#include <ranges>

namespace melon {

// clang-format off
// movable + default_initializable, not semiregular: no algorithm copies a
// heap (every algorithm is move-only) but biobjective_dijkstra
// default-constructs one, so demanding copyability only locks out heaps that
// own their buffer through a move-only handle.
template <typename Q>
concept priority_queue = std::movable<Q> && std::default_initializable<Q> &&
    requires(Q q, const Q cq, typename Q::value_type v) {
    q.push(v);
    // top() and empty() are probed on a const Q: the algorithms read them
    // through const members (dijkstra's current()/finished()), so a heap
    // declaring them non-const would model the concept and hard-error inside
    // the algorithm.
    // convertible_to, not same_as: top() may return by value or by const
    // reference, and same_as<value_type> would reject every heap with
    // std::priority_queue's return type.
    { cq.top() } -> std::convertible_to<typename Q::value_type>;
    q.pop();
    { q.size() } -> std::same_as<typename Q::size_type>;
    { cq.empty() } -> std::convertible_to<bool>;
    q.clear();
};

template <typename Q>
concept updatable_priority_queue = priority_queue<Q> &&
    requires(Q q, typename Q::id_type i, typename Q::priority_type p) {
    { q.contains(i) } -> std::convertible_to<bool>;
    { q.priority(i) } -> std::convertible_to<typename Q::priority_type>;
    q.promote(i, p);
    q.demote(i, p);
};

// For the heap-carrying algorithms' static_assert: a heap that publishes
// index_map_type (updatable_d_ary_heap does) must be built on the very map the
// algorithm creates for its heap_index role. A different but
// range-constructible map compiles and leaves the heap indexing a private copy
// the algorithm never sees. A heap without the alias is not checked.
template <typename Q, typename IndexMap>
concept heap_index_map_agrees =
    !requires { typename Q::index_map_type; } ||
    std::same_as<typename Q::index_map_type, IndexMap>;
// clang-format on

}  // namespace melon
