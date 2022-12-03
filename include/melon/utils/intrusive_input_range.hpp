#ifndef MELON_UTILS_INTRUSIVE_INPUT_RANGE_HPP
#define MELON_UTILS_INTRUSIVE_INPUT_RANGE_HPP

#include <iterator>
#include <type_traits>
#include <utility>

namespace fhamonic {
namespace melon {

template <typename I, typename Incr, typename Deref, typename Cond>
class intrusive_input_range {
private:
    I _begin;
    Deref _deref;
    Incr _incr;
    Cond _cond;

public:
    using reference = std::invoke_result_t<Deref, I>;
    using value_type = std::decay_t<reference>;
    using const_reference = const value_type;
    using pointer = void;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    intrusive_input_range(I begin, Deref && deref, Incr && incr, Cond && cond)
        : _begin(begin)
        , _deref(std::forward<Deref>(deref))
        , _incr(std::forward<Incr>(incr))
        , _cond(std::forward<Cond>(cond)) {}

    struct sentinel {};

    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using reference = std::invoke_result_t<Deref, I>;
        using value_type = std::decay_t<reference>;
        using pointer = void;
        using difference_type = std::ptrdiff_t;

    private:
        I _index;
        std::reference_wrapper<const Deref> _deref;
        std::reference_wrapper<const Incr> _incr;
        std::reference_wrapper<const Cond> _cond;

    public:
        iterator(I index, const Deref & deref, const Incr & incr,
                 const Cond & cond)
            : _index(index), _deref(deref), _incr(incr), _cond(cond) {}

        iterator() = default;
        iterator(const iterator &) = default;
        iterator(iterator &&) = default;

        iterator & operator=(const iterator &) = default;
        iterator & operator=(iterator &&) = default;

        friend bool operator==(const iterator & it, sentinel) noexcept {
            return !it._cond(it._index);
        }

        reference operator*() const noexcept { return _deref(_index); }
        void operator++(int) noexcept { _index = _incr(_index); }
        iterator & operator++() noexcept {
            _index = _incr(_index);
            return *this;
        }
    };

    iterator begin() const {
        return iterator(_begin, _deref, _incr, _cond);
    }
    sentinel end() const { return sentinel(); }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILS_INTRUSIVE_INPUT_RANGE_HPP