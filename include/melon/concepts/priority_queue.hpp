#ifndef MELON_CONCEPTS_PRIORITY_QUEUE_HPP
#define MELON_CONCEPTS_PRIORITY_QUEUE_HPP

#include <concepts>
#include <ranges>

#include "melon/concepts/detail/range_of.hpp"

namespace fhamonic {
namespace melon {
namespace concepts {

// clang-format off
template <typename Q>
concept priority_queue = std::semiregular<Q> &&
    requires(Q q, typename Q::key k, typename Q::priority v) {
    { q.top() } 
        -> std::same_as<std::pair<typename Q::key, typename Q::priority>>;
    q.pop();
    q.emplace(k, v);
    q.clear();
};

template <typename Q>
concept updatable_priority_queue = priority_queue<Q> &&
    requires(Q q, typename Q::key k, typename Q::priority v) {
    { q.contains(k) } -> std::convertible_to<bool>;
    { q.priority(k) } -> std::same_as<typename Q::priority>;
    q.promote(k, v);
    q.demote(k, v);
    q.update(k, v);
};
// clang-format on

}  // namespace concepts
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_PRIORITY_QUEUE_HPP