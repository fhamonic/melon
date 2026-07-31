#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <ranges>
#include <type_traits>

namespace melon {

template <typename Iterator, typename Sentinel>
class consumable_iterator {
public:
    // iterator_concept, not iterator_category: post-increment returns void, so
    // `*it++` is ill-formed and no Cpp17 category is honestly meetable.
    using iterator_concept = std::input_iterator_tag;
    using value_type = std::iter_value_t<Iterator>;
    using reference = std::iter_reference_t<Iterator>;
    using pointer = void;
    using difference_type = std::iter_difference_t<Iterator>;

private:
    Iterator * _it;

public:
    explicit consumable_iterator(Iterator & it) : _it(&it) {}

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

    // Conditional, not unconditional: these forward into an arbitrary wrapped
    // iterator, whose increment and comparison may throw.
    constexpr void operator++(int) noexcept(noexcept(++(*_it))) { ++(*_it); }
    constexpr consumable_iterator & operator++() noexcept(noexcept(++(*_it))) {
        ++(*_it);
        return *this;
    }

    [[nodiscard]] constexpr friend bool operator==(
        const consumable_iterator & it1,
        const consumable_iterator & it2) noexcept(noexcept((*it1._it) ==
                                                           (*it2._it)))
        requires std::equality_comparable<Iterator>
    {
        return (*it1._it) == (*it2._it);
    }

    [[nodiscard]] constexpr friend bool operator==(
        const consumable_iterator & iterator,
        const Sentinel & sentinel) noexcept(noexcept((*iterator._it) ==
                                                     sentinel)) {
        return (*iterator._it) == sentinel;
    }
};

// A cursor that walks a range once. It holds exactly what one traversal needs
// -- the range and an iterator when it has to own the range, an iterator and a
// sentinel when the range is borrowed -- and deliberately nothing that would
// let it start over. Algorithms keeping one cursor per vertex or per stack
// frame re-seed through operator=(Rng &&) with a range they can ask the graph
// for again, e.g. `_remaining_out_arcs[u] = out_arcs(_graph, u)`; remembering
// begin() instead would cost an iterator per vertex and buy nothing.
//
// See consumable_view below for the variant that can restart itself.
template <std::ranges::range R>
class consumable_input_view {
protected:
    // mutable so that empty() can be const. Obtaining end() is a read, but
    // several standard views (filter_view above all) only expose end() on a
    // non-const object, and an algorithm's finished() has to be answerable
    // from a const one. Nothing here ever mutates _range through it.
    //
    // An optional, so that the cursor is default-constructible even when R is
    // not -- a filter_view over a capturing lambda, which is what every
    // filtered subgraph's incidence range is. Algorithms keep these cursors in
    // per-vertex maps and static_map default-constructs its slots, so without
    // the optional a cursor over any filtered subgraph cannot be stored in one
    // at all. A default-constructed cursor is *disengaged*: it only supports
    // destruction and assignment, which is the contract those maps need --
    // every slot is re-seeded through operator=(Rng &&) before use.
    mutable std::optional<R> _range;
    std::ranges::iterator_t<R> _it;
    // How far _it has walked. It exists because _it may refer *back* into
    // _range: a std::ranges::filter_view iterator -- which is what every
    // subgraph's incidence range yields -- holds a pointer to its parent view.
    // A defaulted copy or move would hand the new object an iterator aimed at
    // the *old* _range, a use-after-free as soon as the original dies. Copying
    // is not the only way in: these cursors live inside a std::vector and a
    // static_map, so a plain reallocation relocates them too. Every special
    // member therefore re-derives _it from _consumed instead of copying it.
    //
    // Only this specialisation pays for the counter; the borrowed one below
    // keeps an iterator and a sentinel that are independent of any range
    // object.
    std::size_t _consumed = 0;

    // Puts _it back where _consumed says it should be, against the _range this
    // object now owns. A no-op on a disengaged cursor, so that copying or
    // moving one yields another disengaged cursor instead of dereferencing an
    // empty optional.
    constexpr void _reseek() {
        if(!_range.has_value()) return;
        _it = std::ranges::next(
            std::ranges::begin(*_range),
            static_cast<std::ranges::range_difference_t<R>>(_consumed));
    }

public:
    // Constrained away from the class itself: unconstrained, this
    // single-argument template binds a non-const lvalue of the class type
    // better than the copy constructor, so direct-initialising a copy tries to
    // build a range out of one.
    template <typename Rng>
        requires(!std::same_as<std::remove_cvref_t<Rng>, consumable_input_view>)
    explicit consumable_input_view(Rng && r)
        : _range(std::views::all(std::forward<Rng>(r)))
        , _it(std::ranges::begin(*_range)) {}

    // A cursor already `consumed` elements into a freshly obtained copy of the
    // range: the relocation shape. An algorithm moving a directly-held cursor
    // cannot use the move constructor -- its reseek walks the *old* range,
    // whose predicates may read a graph member that was moved away an instant
    // earlier -- so it builds the successor from the new graph's range and the
    // old cursor's consumed() instead.
    template <typename Rng>
        requires(!std::same_as<std::remove_cvref_t<Rng>, consumable_input_view>)
    explicit consumable_input_view(Rng && r, std::size_t consumed)
        : _range(std::views::all(std::forward<Rng>(r))), _consumed(consumed) {
        _reseek();
    }

    [[nodiscard]] constexpr std::size_t consumed() const noexcept {
        return _consumed;
    }

    consumable_input_view() = default;

    // User-provided, not defaulted: see _consumed. Each one takes the range
    // first and then re-derives the cursor against it.
    // Constrained, so that a cursor over a move-only range (owning_view) is
    // *not copy-constructible* rather than declared-but-ill-formed. A
    // user-provided special member of a class template is only instantiated
    // when called, so without the requires-clause std::copy_constructible
    // would answer true and the failure would move to the call site.
    constexpr consumable_input_view(const consumable_input_view & o)
        requires std::copy_constructible<R>
        : _range(o._range), _consumed(o._consumed) {
        _reseek();
    }
    constexpr consumable_input_view(consumable_input_view && o)
        : _range(std::move(o._range)), _consumed(o._consumed) {
        _reseek();
    }

    // See the copy constructor.
    constexpr consumable_input_view & operator=(const consumable_input_view & o)
        requires std::copy_constructible<R> && std::is_copy_assignable_v<R>
    {
        if(this == std::addressof(o)) return *this;
        _range = o._range;
        _consumed = o._consumed;
        _reseek();
        return *this;
    }
    constexpr consumable_input_view & operator=(consumable_input_view && o) {
        if(this == std::addressof(o)) return *this;
        _range = std::move(o._range);
        _consumed = o._consumed;
        _reseek();
        return *this;
    }

    // A forwarding reference, not `R &`: the re-seeding idiom this class
    // exists for, `_remaining_out_arcs[u] = out_arcs(_graph, u)`, hands over a
    // prvalue, which does not bind to `R &`.
    template <typename Rng>
        requires(!std::same_as<std::remove_cvref_t<Rng>, consumable_input_view>)
    constexpr consumable_input_view & operator=(Rng && r) {
        // emplace, not assignment: it only needs R move-constructible, which
        // every view is, where optional's converting assignment would also
        // demand assignability.
        _range.emplace(std::views::all(std::forward<Rng>(r)));
        _it = std::ranges::begin(*_range);
        _consumed = 0;
        return *this;
    }

    // Re-aims the cursor at a freshly obtained copy of the same logical range,
    // keeping how far it has walked, where operator=(Rng &&) above restarts
    // from the beginning. This is what an algorithm's copy/move uses to point
    // relocated frames back at its *own* graph: _consumed covers this object
    // relocating, rebase() covers the *graph* relocating, after which the
    // cached range must be asked for again. Precondition: `r` enumerates the
    // same elements in the same order as the range this cursor was walking --
    // the multi-pass guarantee every melon incidence range documents. Violate
    // it and the cursor silently resumes at the wrong element.
    template <typename Rng>
        requires(!std::same_as<std::remove_cvref_t<Rng>, consumable_input_view>)
    constexpr void rebase(Rng && r) {
        _range.emplace(std::views::all(std::forward<Rng>(r)));
        _reseek();
    }

    [[nodiscard]] bool empty() const {
        assert(_range.has_value());
        return _it == std::ranges::end(*_range);
    }
    void advance() {
        ++_it;
        ++_consumed;
    }
    [[nodiscard]] decltype(auto) current() { return *_it; }
    [[nodiscard]] decltype(auto) current() const { return *_it; }

    [[nodiscard]] constexpr auto begin() {
        return consumable_iterator<std::ranges::iterator_t<R>,
                                   std::ranges::sentinel_t<R>>(_it);
    }
    [[nodiscard]] constexpr auto end() const {
        assert(_range.has_value());
        return std::ranges::end(*_range);
    }
};

// A borrowed range outlives its own range object, so the cursor can drop it and
// keep the two iterators instead. This is the specialisation the per-vertex and
// per-stack-frame cursors above land on: out_arcs of a static_digraph is a
// std::span, so a cursor is two pointers.
template <std::ranges::borrowed_range R>
class consumable_input_view<R> : public std::ranges::view_base {
protected:
    std::ranges::iterator_t<R> _it;
    [[no_unique_address]] std::ranges::sentinel_t<R> _sentinel;

public:
    // See the primary template, including the `explicit`: whether a range
    // converts to a cursor must not depend on whether it happens to be
    // borrowed.
    template <typename Rng>
        requires(!std::same_as<std::remove_cvref_t<Rng>, consumable_input_view>)
    explicit consumable_input_view(Rng && r)
        : _it(std::ranges::begin(r)), _sentinel(std::ranges::end(r)) {}

    consumable_input_view() = default;
    consumable_input_view(const consumable_input_view &) = default;
    consumable_input_view(consumable_input_view &&) = default;

    constexpr consumable_input_view & operator=(const consumable_input_view &) =
        default;
    constexpr consumable_input_view & operator=(consumable_input_view &&) =
        default;

    // See the primary template.
    template <typename Rng>
        requires(!std::same_as<std::remove_cvref_t<Rng>, consumable_input_view>)
    constexpr consumable_input_view & operator=(Rng && r) {
        _it = std::ranges::begin(r);
        _sentinel = std::ranges::end(r);
        return *this;
    }

    [[nodiscard]] bool empty() const { return _it == _sentinel; }
    void advance() { ++_it; }
    [[nodiscard]] decltype(auto) current() { return *_it; }
    [[nodiscard]] decltype(auto) current() const { return *_it; }

    [[nodiscard]] constexpr auto begin() {
        return consumable_iterator<std::ranges::iterator_t<R>,
                                   std::ranges::sentinel_t<R>>(_it);
    }
    [[nodiscard]] constexpr auto end() const { return _sentinel; }
};

template <std::ranges::viewable_range R>
consumable_input_view(R &&) -> consumable_input_view<std::views::all_t<R>>;

template <typename R>
using consumable_input_view_t =
    std::decay_t<decltype(consumable_input_view(std::declval<R>()))>;

// A consumable_input_view that can also restart itself over the range it
// already holds. Use it only when re-obtaining the range is not an option --
// traversal_forest owns a caller-supplied source range that may be a move-only
// std::ranges::owning_view, so it cannot keep a spare copy to assign back.
// Everywhere else consumable_input_view is the cheaper and more general choice:
// assigning a freshly obtained range works for an input range too.
//
// Restricted to a forward_range, since restarting is exactly what an input
// range cannot promise.
template <std::ranges::forward_range R>
class consumable_view : public consumable_input_view<R> {
private:
    using base = consumable_input_view<R>;

    struct no_state {};
    // Only the borrowed specialisation of the base needs a remembered begin:
    // the primary one owns the range and can ask it for begin() again.
    [[no_unique_address]] std::conditional_t<std::ranges::borrowed_range<R>,
                                             std::ranges::iterator_t<R>,
                                             no_state> _begin;

    constexpr void remember_begin() {
        if constexpr(std::ranges::borrowed_range<R>) _begin = base::_it;
    }

public:
    // See consumable_input_view's primary template.
    template <typename Rng>
        requires(!std::same_as<std::remove_cvref_t<Rng>, consumable_view>)
    explicit constexpr consumable_view(Rng && r) : base(std::forward<Rng>(r)) {
        remember_begin();
    }

    // The relocation shape -- see consumable_input_view's (range, consumed)
    // constructor. Only meaningful for the non-borrowed base, which is the
    // only one relocation can invalidate.
    template <typename Rng>
        requires(!std::same_as<std::remove_cvref_t<Rng>, consumable_view>) &&
                (!std::ranges::borrowed_range<R>)
    explicit constexpr consumable_view(Rng && r, std::size_t consumed)
        : base(std::forward<Rng>(r), consumed) {
        remember_begin();
    }

    consumable_view() = default;
    consumable_view(const consumable_view &) = default;
    consumable_view(consumable_view &&) = default;

    constexpr consumable_view & operator=(const consumable_view &) = default;
    constexpr consumable_view & operator=(consumable_view &&) = default;

    template <typename Rng>
        requires(!std::same_as<std::remove_cvref_t<Rng>, consumable_view>)
    constexpr consumable_view & operator=(Rng && r) {
        base::operator=(std::forward<Rng>(r));
        remember_begin();
        return *this;
    }

    constexpr void rewind() {
        if constexpr(std::ranges::borrowed_range<R>) {
            base::_it = _begin;
        } else {
            base::_it = std::ranges::begin(*base::_range);
            base::_consumed = 0;
        }
    }
};

template <std::ranges::viewable_range R>
consumable_view(R &&) -> consumable_view<std::views::all_t<R>>;

template <typename R>
using consumable_view_t =
    std::decay_t<decltype(consumable_view(std::declval<R>()))>;

}  // namespace melon
