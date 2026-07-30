#pragma once

#include <concepts>
#include <ranges>

namespace melon {

// clang-format off
template <typename Q>
concept priority_queue = std::semiregular<Q> &&
    requires(Q q, typename Q::value_type v) {
    q.push(v);
    // convertible_to, not same_as: top() may return by value or by
    // const reference. same_as<value_type> baked the copy-returning shape
    // into the concept and rejected every STL-shaped heap -- including
    // d_ary_heap once its top() adopted std::priority_queue's return type.
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
    { q.priority(i) } -> std::same_as<typename Q::priority_type>;
    q.promote(i, p);
    q.demote(i, p);
};
// clang-format on

}  // namespace melon
