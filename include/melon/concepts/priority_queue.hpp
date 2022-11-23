#ifndef MELON_CONCEPTS_PRIORITY_QUEUE_HPP
#define MELON_CONCEPTS_PRIORITY_QUEUE_HPP

#include <concepts>
#include <ranges>

#include "melon/concepts/range_of.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
namespace concepts {
template <typename Q>
concept priority_queue = //std::semiregular<Q> &&
    requires(Q q, typename Q::key_type k, typename Q::priority_type v, typename Q::entry e) {
    { q.top() } -> std::same_as<typename Q::entry>;
    { e.first } -> std::common_with<typename Q::key_type>;
    { e.second } -> std::common_with<typename Q::priority_type>;
    q.pop();
    q.push(k, v);
    q.clear();
};

template <typename Q>
concept updatable_priority_queue = priority_queue<Q> &&
    requires(Q q, typename Q::key_type k, typename Q::priority_type v) {
    // { q.contains(k) } -> std::convertible_to<bool>;
    { q.priority(k) } -> std::same_as<typename Q::priority_type>;
    // q.increase(k, v);
    q.decrease(k, v);
    // q.update(k, v);
};
}  // namespace concepts
// clang-format on

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_CONCEPTS_PRIORITY_QUEUE_HPP