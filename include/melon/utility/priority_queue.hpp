#ifndef MELON_UTILITY_PRIORITY_QUEUE_HPP
#define MELON_UTILITY_PRIORITY_QUEUE_HPP

#include <concepts>
#include <ranges>

namespace fhamonic {
namespace melon {

// clang-format off
template <typename Q>
concept priority_queue = std::semiregular<Q> &&
    requires(Q q, typename Q::value_type v) {
    q.push(v);
    { q.top() } -> std::same_as<typename Q::value_type>;
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
}  // namespace fhamonic

#endif  // MELON_UTILITY_PRIORITY_QUEUE_HPP