#ifndef MELON_UTILITY_PRIORITY_QUEUE_HPP
#define MELON_UTILITY_PRIORITY_QUEUE_HPP

#include <concepts>
#include <ranges>

namespace fhamonic {
namespace melon {

// clang-format off
template <typename Q>
concept priority_queue = std::semiregular<Q> &&
    requires(Q q, typename Q::entry e) {
    { q.top() } -> std::same_as<typename Q::entry>;
    q.pop();
    q.push(e);
    q.clear();
};

template <typename Q>
concept updatable_priority_queue = priority_queue<Q> &&
    requires(Q q, typename Q::id_type i, typename Q::priority_type v) {
    { q.contains(i) } -> std::convertible_to<bool>;
    { q.priority(i) } -> std::same_as<typename Q::priority_type>;
    q.promote(i, v);
    q.demote(i, v);
};
// clang-format on

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILITY_PRIORITY_QUEUE_HPP