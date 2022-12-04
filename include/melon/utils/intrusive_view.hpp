#ifndef MELON_UTILS_INTRUSIVE_VIEW_HPP
#define MELON_UTILS_INTRUSIVE_VIEW_HPP

#include <functional>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>

namespace fhamonic {
namespace melon {

template <typename I, typename Incr, typename Deref, typename Cond>
class intrusive_view {
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
    intrusive_view(I begin, Deref && deref, Incr && incr, Cond && cond)
        : _begin(begin)
        , _deref(std::forward<Deref>(deref))
        , _incr(std::forward<Incr>(incr))
        , _cond(std::forward<Cond>(cond)) {}

    intrusive_view(const intrusive_view &) = default;
    intrusive_view(intrusive_view &&) = default;

    // intrusive_view would not be a viewable_range without operator=
    // https://www.fluentcpp.com/2020/10/02/how-to-implement-operator-when-a-data-member-is-a-lambda/
    intrusive_view & operator=(const intrusive_view & that) noexcept {
        _begin = that._begin;
        _deref.reset();
        if(that._deref) _deref.emplace(*that._deref);
        _incr.reset();
        if(that._incr) _incr.emplace(*that._incr);
        _cond.reset();
        if(that._cond) _cond.emplace(*that._cond);
        return *this;
    }
    intrusive_view & operator=(intrusive_view && that) {
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
        iterator(const I & index, const Deref & deref, const Incr & incr,
                 const Cond & cond)
            : _index(index), _deref(deref), _incr(incr), _cond(cond) {}

        iterator() = default;
        iterator(const iterator &) = default;
        iterator(iterator &&) = default;

        iterator & operator=(const iterator & that) noexcept {
            _index = that._index;
            _deref.reset();
            if(that._deref) _deref.emplace(*that._deref);
            _incr.reset();
            if(that._incr) _incr.emplace(*that._incr);
            _cond.reset();
            if(that._cond) _cond.emplace(*that._cond);
            return *this;
        }
        iterator & operator=(iterator && that) {
            _index = std::move(that._index);
            _deref.reset();
            if(that._deref) _deref.emplace(std::move(*that._deref));
            _incr.reset();
            if(that._incr) _incr.emplace(std::move(*that._incr));
            _cond.reset();
            if(that._cond) _cond.emplace(std::move(*that._cond));
            return *this;
        }

        friend bool operator==(const iterator & it, sentinel) noexcept {
            return !it._cond.value()(it._index);
        }

        reference operator*() const noexcept { return _deref.value()(_index); }
        void operator++(int) noexcept { _index = _incr.value()(_index); }
        iterator & operator++() noexcept {
            _index = _incr.value()(_index);
            return *this;
        }
    };

    iterator begin() const { return iterator(_begin, *_deref, *_incr, *_cond); }
    sentinel end() const { return sentinel(); }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILS_INTRUSIVE_VIEW_HPP