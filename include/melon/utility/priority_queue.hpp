#pragma once

#include <concepts>
#include <ranges>

namespace melon {

// clang-format off
// movable + default_initializable, not semiregular: no algorithm copies a
// heap (they are move-only by ruling) but biobjective_dijkstra
// default-constructs one, so demanding copyability only locks out heaps that
// own their buffer through a move-only handle.
template <typename Q>
concept priority_queue = std::movable<Q> && std::default_initializable<Q> &&
    requires(Q q, typename Q::value_type v) {
    q.push(v);
    // convertible_to, not same_as: top() may return by value or by const
    // reference, and same_as<value_type> would reject every heap with
    // std::priority_queue's return type.
    { q.top() } -> std::convertible_to<typename Q::value_type>;
    q.pop();
    { q.size() } -> std::same_as<typename Q::size_type>;
    { q.empty() } -> std::convertible_to<bool>;
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
// clang-format on

}  // namespace melon
