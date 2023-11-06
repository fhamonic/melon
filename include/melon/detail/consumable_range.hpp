#ifndef MELON_DETAIL_CONSUMABLE_RANGE_HPP
#define MELON_DETAIL_CONSUMABLE_RANGE_HPP

#include <ranges>
#include <type_traits>

#include <range/v3/view/concat.hpp>

namespace fhamonic {
namespace melon {

template <typename R>
    requires std::ranges::view<R> || ranges::view_<R>
class consumable_range {
private:
    R range;
    std::ranges::iterator_t<R> it;

public:
    consumable_range() = default;
    consumable_range(const consumable_range &) = default;
    consumable_range(consumable_range &&) = default;

    consumable_range(R && r) : range(std::forward<R>(r)), it(range.begin()) {}

    constexpr consumable_range & operator=(const consumable_range &) = default;
    constexpr consumable_range & operator=(consumable_range &&) = default;

    constexpr consumable_range & operator=(R & r) {
        range = r;
        it = range.begin();
    }

    bool empty() { return it == range.end(); }
    void advance() { ++it; }
    decltype(auto) current() { return *it; }
    decltype(auto) current() const { return *it; }
};

template <typename R>
    requires (std::ranges::view<R> || ranges::view_<R>) && (std::ranges::borrowed_range<R> || ranges::borrowed_range<R>)
class consumable_range<R> : public std::ranges::view_base {
private:
    std::ranges::iterator_t<R> it;
    [[no_unique_address]] std::ranges::sentinel_t<R> sentinel;

public:
    consumable_range() = default;
    consumable_range(const consumable_range &) = default;
    consumable_range(consumable_range &&) = default;

    consumable_range(R && r) : it(r.begin()), sentinel(r.end()) {}

    constexpr consumable_range & operator=(const consumable_range &) = default;
    constexpr consumable_range & operator=(consumable_range &&) = default;

    constexpr consumable_range & operator=(R & r) {
        it = r.begin();
        sentinel = r.end();
    }

    bool empty() { return it == sentinel; }
    void advance() { ++it; }
    decltype(auto) current() { return *it; }
    decltype(auto) current() const { return *it; }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DETAIL_CONSUMABLE_RANGE_HPP