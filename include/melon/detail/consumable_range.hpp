#ifndef MELON_DETAIL_CONSUMABLE_RANGE_HPP
#define MELON_DETAIL_CONSUMABLE_RANGE_HPP

#include <ranges>
#include <type_traits>

#include "melon/detail/constexpr_ternary.hpp"

namespace fhamonic {
namespace melon {

template <typename R>
class consumable_range {
private:
    static constexpr bool store_range = !std::ranges::borrowed_range<R>;

    struct no_stored_range {};
    using stored_range =
        std::conditional<store_range, R, no_stored_range>::type;
    struct no_stored_sentinel {};
    using stored_sentinel =
        std::conditional<!store_range, std::ranges::sentinel_t<R>,
                         no_stored_sentinel>::type;

    [[no_unique_address]] stored_range range;
    std::ranges::iterator_t<R> it;
    [[no_unique_address]] stored_sentinel sentinel;

public:
    consumable_range() = default;
    consumable_range(const consumable_range &) = default;
    consumable_range(consumable_range &&) = default;

    consumable_range(R r)
        : range(constexpr_ternary<store_range>(r, no_stored_range{}))
        , it(r.begin())
        , sentinel(
              constexpr_ternary<store_range>(no_stored_sentinel{}, r.end())) {}

    constexpr consumable_range & operator=(const consumable_range &) = default;
    constexpr consumable_range & operator=(consumable_range &&) = default;

    constexpr consumable_range & operator=(R & r) {
        if constexpr(store_range) {
            range = r;
        } else {
            sentinel = r.end();
        }
        it = r.begin();
    }

    bool empty() {
        if constexpr(store_range)
            return it == range.end();
        else
            return it == sentinel;
    }
    void advance() { ++it; }
    decltype(auto) current() { return *it; }
    decltype(auto) current() const { return *it; }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DETAIL_CONSUMABLE_RANGE_HPP