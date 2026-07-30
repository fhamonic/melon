#pragma once

#include <functional>
#include <iterator>
#include <ranges>

#include <type_traits>
#include <utility>
#include "melon/detail/movable_box.hpp"

namespace melon {

template <typename I, typename Incr, typename Deref, typename Cond>
class intrusive_view : public std::ranges::view_base {
public:
    using reference = std::invoke_result_t<Deref, const I &>;
    using value_type = std::decay_t<reference>;
    using const_reference = const value_type;
    using pointer = void;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

private:
    // movable_box, not std::optional: the problem these three solve is that a
    // capturing lambda is copy-constructible but not *assignable*, which is
    // exactly what melon::detail::movable_box exists for -- mapping_owning_view
    // already used it for the same reason. The optionals cost a discriminant
    // byte plus padding in every iterator and forced the two hand-written
    // assignment operators below (once here, once again in the iterator) that
    // are now `= default`.
    I _begin;
    [[no_unique_address]] detail::movable_box<Deref> _deref;
    [[no_unique_address]] detail::movable_box<Incr> _incr;
    [[no_unique_address]] detail::movable_box<Cond> _cond;

public:
    // Templated on the argument types rather than taking `Deref &&` etc.
    // directly: those are class template parameters, so they form plain
    // rvalue references and a named (lvalue) functor could not be passed at
    // all. The deduction guide below decays them back to the stored types.
    template <typename DerefArg, typename IncrArg, typename CondArg>
        requires std::constructible_from<Deref, DerefArg> &&
                     std::constructible_from<Incr, IncrArg> &&
                     std::constructible_from<Cond, CondArg>
    constexpr intrusive_view(I begin, DerefArg && deref, IncrArg && incr,
                             CondArg && cond)
        : _begin(std::move(begin))
        , _deref(std::in_place, std::forward<DerefArg>(deref))
        , _incr(std::in_place, std::forward<IncrArg>(incr))
        , _cond(std::in_place, std::forward<CondArg>(cond)) {}

    // Constrained, not unconditional: a box holding a capturing lambda has no
    // default constructor. Nothing requires an intrusive_view to be
    // default-initialisable -- std::input_iterator and std::ranges::view do
    // not -- so this is now honest rather than an empty-optional placeholder.
    constexpr intrusive_view()
        requires std::default_initializable<Deref> &&
                     std::default_initializable<Incr> &&
                     std::default_initializable<Cond>
    = default;
    constexpr intrusive_view(const intrusive_view &) = default;
    constexpr intrusive_view(intrusive_view &&) = default;

    // Defaulted now: movable_box is what makes a non-assignable functor
    // assignable, so the destroy-and-reconstruct dance these two used to spell
    // out by hand -- for three members, in two classes -- lives in one place.
    constexpr intrusive_view & operator=(const intrusive_view &) = default;
    constexpr intrusive_view & operator=(intrusive_view &&) = default;

    struct sentinel {};

    class iterator {
    public:
        // iterator_concept, not iterator_category: `*it++` is ill-formed here
        // (post-increment returns void), so the Cpp17 category the old
        // typedef promised was a lie. See algorithm_iterator.
        using iterator_concept = std::input_iterator_tag;
        using reference = std::invoke_result_t<Deref, const I &>;
        using value_type = std::decay_t<reference>;
        using pointer = void;
        using difference_type = std::ptrdiff_t;

    private:
        I _index;
        [[no_unique_address]] detail::movable_box<Deref> _deref;
        [[no_unique_address]] detail::movable_box<Incr> _incr;
        [[no_unique_address]] detail::movable_box<Cond> _cond;

    public:
        constexpr iterator(const I & index, const Deref & deref,
                           const Incr & incr, const Cond & cond)
            : _index(index), _deref(deref), _incr(incr), _cond(cond) {}

        // See intrusive_view's default constructor.
        constexpr iterator()
            requires std::default_initializable<Deref> &&
                         std::default_initializable<Incr> &&
                         std::default_initializable<Cond>
        = default;
        constexpr iterator(const iterator &) = default;
        constexpr iterator(iterator &&) = default;

        constexpr iterator & operator=(const iterator &) = default;
        constexpr iterator & operator=(iterator &&) = default;

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
template <typename I, typename Deref, typename Incr, typename Cond>
intrusive_view(I, Deref &&, Incr &&, Cond &&)
    -> intrusive_view<I, std::decay_t<Incr>, std::decay_t<Deref>,
                      std::decay_t<Cond>>;

}  // namespace melon

template <typename I, typename Incr, typename Deref, typename Cond>
inline constexpr bool std::ranges::enable_borrowed_range<
    melon::intrusive_view<I, Incr, Deref, Cond>> = true;

template <typename I, typename Incr, typename Deref, typename Cond>
inline constexpr bool
    std::ranges::enable_view<melon::intrusive_view<I, Incr, Deref, Cond>> =
        true;
