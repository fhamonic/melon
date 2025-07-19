#ifndef MELON_DETAIL_consumable_view_HPP
#define MELON_DETAIL_consumable_view_HPP

#include <ranges>
#include <type_traits>

namespace fhamonic {
namespace melon {

template <typename _Iterator, typename _Sentinel>
class consumable_iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = std::iter_value_t<_Iterator>;
    using reference = std::iter_reference_t<_Iterator>;
    using pointer = void;
    using difference_type = std::iter_difference_t<_Iterator>;

private:
    _Iterator * _it;

public:
    explicit consumable_iterator(_Iterator & it) : _it(&it) {}

    consumable_iterator() = default;
    consumable_iterator(consumable_iterator &&) = default;
    consumable_iterator(const consumable_iterator &) = default;

    constexpr consumable_iterator & operator=(
        const consumable_iterator & that) noexcept {
        _it = that._it;
        return *this;
    }

    constexpr reference operator*() { return *(*_it); }
    constexpr reference operator*() const { return *(*_it); }

    constexpr void operator++(int) noexcept { ++(*_it); }
    constexpr consumable_iterator & operator++() noexcept {
        ++(*_it);
        return *this;
    }

    [[nodiscard]] constexpr friend bool operator==(
        const consumable_iterator & it1,
        const consumable_iterator & it2) noexcept
        requires std::equality_comparable<_Iterator>
    {
        return (*it1._it) == (*it2._it);
    }

    [[nodiscard]] constexpr friend bool operator==(
        const consumable_iterator & iterator,
        const _Sentinel & sentinel) noexcept {
        return (*iterator._it) == sentinel;
    }
};

template <std::ranges::range R>
class consumable_view {
private:
    R _range;
    std::ranges::iterator_t<R> _it;

public:
    template <typename _R>
    explicit consumable_view(_R && r)
        : _range(std::views::all(std::forward<_R>(r)))
        , _it(std::ranges::begin(_range)) {}

    consumable_view() = default;
    consumable_view(const consumable_view &) = default;
    consumable_view(consumable_view &&) = default;

    constexpr consumable_view & operator=(const consumable_view &) = default;
    constexpr consumable_view & operator=(consumable_view &&) = default;

    constexpr consumable_view & operator=(R & r) {
        _range = r;
        _it = std::ranges::begin(_range);
    }

    bool empty() { return _it == std::ranges::end(_range); }
    void advance() { ++_it; }
    decltype(auto) current() { return *_it; }
    decltype(auto) current() const { return *_it; }

    constexpr auto begin() {
        return consumable_iterator<std::ranges::iterator_t<R>,
                                   std::ranges::sentinel_t<R>>(_it);
    }
    constexpr auto end() { return std::ranges::end(_range); }

    // constexpr std::size_t size() const { return std::ranges::end(_range) - _it; }
};

template <std::ranges::borrowed_range R>
class consumable_view<R> : public std::ranges::view_base {
private:
    std::ranges::iterator_t<R> _it;
    [[no_unique_address]] std::ranges::sentinel_t<R> _sentinel;

public:
    template <typename _R>
    consumable_view(_R && r)
        : _it(std::ranges::begin(r)), _sentinel(std::ranges::end(r)) {}

    consumable_view() = default;
    consumable_view(const consumable_view &) = default;
    consumable_view(consumable_view &&) = default;

    constexpr consumable_view & operator=(const consumable_view &) = default;
    constexpr consumable_view & operator=(consumable_view &&) = default;

    constexpr consumable_view & operator=(R & r) {
        _it = r.begin();
        _sentinel = r.end();
    }

    bool empty() { return _it == _sentinel; }
    void advance() { ++_it; }
    decltype(auto) current() { return *_it; }
    decltype(auto) current() const { return *_it; }

    constexpr auto begin() {
        return consumable_iterator<std::ranges::iterator_t<R>,
                                   std::ranges::sentinel_t<R>>(_it);
    }
    constexpr auto end() { return _sentinel; }
};

template <std::ranges::viewable_range R>
consumable_view(R &&) -> consumable_view<std::views::all_t<R>>;

template <typename R>
using consumable_view_t =
    std::decay_t<decltype(consumable_view(std::declval<R>()))>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DETAIL_consumable_view_HPP