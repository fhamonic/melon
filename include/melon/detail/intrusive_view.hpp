#pragma once

#include <functional>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>

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
    // Templated on the argument types rather than taking `Deref &&` etc.
    // directly: those are class template parameters, so they form plain
    // rvalue references and a named (lvalue) functor could not be passed at
    // all. The deduction guide below decays them back to the stored types.
    template <typename _Deref, typename _Incr, typename _Cond>
        requires std::constructible_from<Deref, _Deref> &&
                     std::constructible_from<Incr, _Incr> &&
                     std::constructible_from<Cond, _Cond>
    [[nodiscard]] constexpr intrusive_view(I begin, _Deref && deref,
                                           _Incr && incr, _Cond && cond)
        : _begin(std::move(begin))
        , _deref(std::forward<_Deref>(deref))
        , _incr(std::forward<_Incr>(incr))
        , _cond(std::forward<_Cond>(cond)) {}

    [[nodiscard]] constexpr intrusive_view() = default;
    [[nodiscard]] constexpr intrusive_view(const intrusive_view &) = default;
    [[nodiscard]] constexpr intrusive_view(intrusive_view &&) = default;

    // intrusive_view would not be a viewable_range without operator=
    // https://www.fluentcpp.com/2020/10/02/how-to-implement-operator-when-a-data-member-is-a-lambda/
    constexpr intrusive_view & operator=(const intrusive_view & that) noexcept(
        std::is_nothrow_copy_assignable_v<I> &&
        std::is_nothrow_copy_constructible_v<Deref> &&
        std::is_nothrow_copy_constructible_v<Incr> &&
        std::is_nothrow_copy_constructible_v<Cond>) {
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

        constexpr iterator & operator=(const iterator & that) noexcept(
            std::is_nothrow_copy_assignable_v<I> &&
            std::is_nothrow_copy_constructible_v<Deref> &&
            std::is_nothrow_copy_constructible_v<Incr> &&
            std::is_nothrow_copy_constructible_v<Cond>) {
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

        // These three invoke user-supplied functors, so their noexcept has to
        // be conditional on the invocation: an unconditional one turned a
        // throwing functor into std::terminate. They also dereference the
        // optionals unchecked -- using an iterator built by the default
        // constructor is undefined for every iterator, so the value() check
        // bought nothing and made an honest noexcept impossible.
        [[nodiscard]] constexpr friend bool
        operator==(const iterator & it, sentinel) noexcept(
            std::is_nothrow_invocable_v<const Cond &, const I &>) {
            return !(*it._cond)(it._index);
        }

        [[nodiscard]] constexpr reference operator*() const
            noexcept(std::is_nothrow_invocable_v<const Deref &, const I &>) {
            return (*_deref)(_index);
        }
        constexpr void operator++(int) noexcept(
            std::is_nothrow_invocable_v<const Incr &, const I &> &&
            std::is_nothrow_assignable_v<
                I &, std::invoke_result_t<const Incr &, const I &>>) {
            _index = (*_incr)(_index);
        }
        constexpr iterator & operator++() noexcept(
            std::is_nothrow_invocable_v<const Incr &, const I &> &&
            std::is_nothrow_assignable_v<
                I &, std::invoke_result_t<const Incr &, const I &>>) {
            _index = (*_incr)(_index);
            return *this;
        }
    };

    [[nodiscard]] constexpr iterator begin() const {
        return iterator(_begin, *_deref, *_incr, *_cond);
    }
    [[nodiscard]] constexpr sentinel end() const { return sentinel(); }
};

// Note the reordering: the constructor takes (begin, deref, incr, cond) while
// the class is parameterised <I, Incr, Deref, Cond>.
template <typename I, typename _Deref, typename _Incr, typename _Cond>
intrusive_view(I, _Deref &&, _Incr &&, _Cond &&)
    -> intrusive_view<I, std::decay_t<_Incr>, std::decay_t<_Deref>,
                      std::decay_t<_Cond>>;

}  // namespace melon

template <typename I, typename Incr, typename Deref, typename Cond>
inline constexpr bool std::ranges::enable_borrowed_range<
    melon::intrusive_view<I, Incr, Deref, Cond>> = true;

template <typename I, typename Incr, typename Deref, typename Cond>
inline constexpr bool
    std::ranges::enable_view<melon::intrusive_view<I, Incr, Deref, Cond>> =
        true;
