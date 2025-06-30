#ifndef MELON_DETAIL_INTRUSIVE_VIEW_HPP
#define MELON_DETAIL_INTRUSIVE_VIEW_HPP

#include <functional>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>

namespace fhamonic {
namespace melon {

template <typename I, typename Incr, typename Deref, typename Cond>
class intrusive_view : std::ranges::view_base {
public:
    using reference = std::invoke_result_t<Deref, const I &>;
    using value_type = std::decay_t<reference>;
    using const_reference = const value_type;
    using pointer = void;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

private:
    I _begin;
    std::optional<Deref> _deref;
    std::optional<Incr> _incr;
    std::optional<Cond> _cond;

public:
    [[nodiscard]] constexpr intrusive_view(I begin, Deref && deref,
                                           Incr && incr, Cond && cond)
        : _begin(begin)
        , _deref(std::forward<Deref>(deref))
        , _incr(std::forward<Incr>(incr))
        , _cond(std::forward<Cond>(cond)) {}

    [[nodiscard]] constexpr intrusive_view() = default;
    [[nodiscard]] constexpr intrusive_view(const intrusive_view &) = default;
    [[nodiscard]] constexpr intrusive_view(intrusive_view &&) = default;

    // intrusive_view would not be a viewable_range without operator=
    // https://www.fluentcpp.com/2020/10/02/how-to-implement-operator-when-a-data-member-is-a-lambda/
    constexpr intrusive_view & operator=(const intrusive_view & that) noexcept {
        _begin = that._begin;
        _deref.reset();
        if(that._deref) _deref.emplace(*that._deref);
        _incr.reset();
        if(that._incr) _incr.emplace(*that._incr);
        _cond.reset();
        if(that._cond) _cond.emplace(*that._cond);
        return *this;
    }
    constexpr intrusive_view & operator=(intrusive_view && that) {
        _begin = std::move(that._begin);
        _deref.reset();
        if(that._deref) _deref.emplace(std::move(*that._deref));
        _incr.reset();
        if(that._incr) _incr.emplace(std::move(*that._incr));
        _cond.reset();
        if(that._cond) _cond.emplace(std::move(*that._cond));
        return *this;
    }

    struct sentinel {};

    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using reference = std::invoke_result_t<Deref, const I &>;
        using value_type = std::decay_t<reference>;
        using pointer = void;
        using difference_type = std::ptrdiff_t;

    private:
        I _index;
        std::optional<Deref> _deref;
        std::optional<Incr> _incr;
        std::optional<Cond> _cond;

    public:
        [[nodiscard]] constexpr iterator(const I & index, const Deref & deref,
                                         const Incr & incr, const Cond & cond)
            : _index(index), _deref(deref), _incr(incr), _cond(cond) {}

        [[nodiscard]] constexpr iterator() = default;
        [[nodiscard]] constexpr iterator(const iterator &) = default;
        [[nodiscard]] constexpr iterator(iterator &&) = default;

        constexpr iterator & operator=(const iterator & that) noexcept {
            _index = that._index;
            _deref.reset();
            if(that._deref) _deref.emplace(*that._deref);
            _incr.reset();
            if(that._incr) _incr.emplace(*that._incr);
            _cond.reset();
            if(that._cond) _cond.emplace(*that._cond);
            return *this;
        }
        constexpr iterator & operator=(iterator && that) {
            _index = std::move(that._index);
            _deref.reset();
            if(that._deref) _deref.emplace(std::move(*that._deref));
            _incr.reset();
            if(that._incr) _incr.emplace(std::move(*that._incr));
            _cond.reset();
            if(that._cond) _cond.emplace(std::move(*that._cond));
            return *this;
        }

        [[nodiscard]] constexpr friend bool operator==(const iterator & it,
                                                       sentinel) noexcept {
            return !it._cond.value()(it._index);
        }

        [[nodiscard]] constexpr reference operator*() const noexcept {
            return _deref.value()(_index);
        }
        constexpr void operator++(int) noexcept {
            _index = _incr.value()(_index);
        }
        constexpr iterator & operator++() noexcept {
            _index = _incr.value()(_index);
            return *this;
        }
    };

    [[nodiscard]] constexpr iterator begin() const {
        return iterator(_begin, *_deref, *_incr, *_cond);
    }
    [[nodiscard]] constexpr sentinel end() const { return sentinel(); }
};

}  // namespace melon
}  // namespace fhamonic

template <typename I, typename Incr, typename Deref, typename Cond>
inline constexpr bool std::ranges::enable_borrowed_range<
    fhamonic::melon::intrusive_view<I, Incr, Deref, Cond>> = true;

template <typename I, typename Incr, typename Deref, typename Cond>
inline constexpr bool std::ranges::enable_view<
    fhamonic::melon::intrusive_view<I, Incr, Deref, Cond>> = true;

#endif  // MELON_DETAIL_INTRUSIVE_VIEW_HPP