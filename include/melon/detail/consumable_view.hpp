#ifndef MELON_DETAIL_consumable_view_HPP
#define MELON_DETAIL_consumable_view_HPP

#include <ranges>
#include <type_traits>

namespace fhamonic {
namespace melon {

template <typename R>
class consumable_view {
private:
    R range;
    std::ranges::iterator_t<R> it;

public:
    consumable_view() = default;
    consumable_view(const consumable_view &) = default;
    consumable_view(consumable_view &&) = default;

    template <typename _R>
    consumable_view(_R && r)
        : range(std::views::all(std::forward<_R>(r))), it(range.begin()) {}

    constexpr consumable_view & operator=(const consumable_view &) = default;
    constexpr consumable_view & operator=(consumable_view &&) = default;

    constexpr consumable_view & operator=(R & r) {
        range = r;
        it = range.begin();
    }

    bool empty() { return it == range.end(); }
    void advance() { ++it; }
    decltype(auto) current() { return *it; }
    decltype(auto) current() const { return *it; }
};

template <std::ranges::viewable_range R>
class consumable_view<R> : public std::ranges::view_base {
private:
    std::ranges::iterator_t<R> it;
    [[no_unique_address]] std::ranges::sentinel_t<R> sentinel;

public:
    consumable_view() = default;
    consumable_view(const consumable_view &) = default;
    consumable_view(consumable_view &&) = default;

    template <typename _R>
    consumable_view(_R && r) : it(r.begin()), sentinel(r.end()) {}

    constexpr consumable_view & operator=(const consumable_view &) = default;
    constexpr consumable_view & operator=(consumable_view &&) = default;

    constexpr consumable_view & operator=(R & r) {
        it = r.begin();
        sentinel = r.end();
    }

    bool empty() { return it == sentinel; }
    void advance() { ++it; }
    decltype(auto) current() { return *it; }
    decltype(auto) current() const { return *it; }
};

template <std::ranges::viewable_range R>
consumable_view(R &&) -> consumable_view<std::views::all_t<R>>;

template <typename R>
using consumable_view_t =
    std::decay_t<decltype(consumable_view(std::declval<R>()))>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DETAIL_consumable_view_HPP