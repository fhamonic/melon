#ifndef MELON_DETAIL_CONCAT_VIEW_HPP
#define MELON_DETAIL_CONCAT_VIEW_HPP

#include <concepts>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

namespace fhamonic {
namespace melon {
namespace detail {
namespace views {

#if defined(__cpp_lib_ranges_concat)

inline constexpr auto concat = std::views::concat;

#else

template <typename V1, typename V2>
concept concat_view_compatible =
    std::ranges::view<V1> && std::ranges::view<V2> &&
    std::ranges::input_range<V1> && std::ranges::input_range<V2> &&
    std::common_reference_with<std::ranges::range_reference_t<V1>,
                               std::ranges::range_reference_t<V2> > &&
    std::common_reference_with<std::ranges::range_reference_t<V1>,
                               std::ranges::range_value_t<V2> &> &&
    std::common_reference_with<std::ranges::range_reference_t<V2>,
                               std::ranges::range_value_t<V1> &>;

template <std::ranges::view V1, std::ranges::view V2>
    requires concat_view_compatible<V1, V2>
class concat_view : public std::ranges::view_interface<concat_view<V1, V2> > {
private:
    V1 _first;
    V2 _second;

    template <bool Const>
    using base_t = std::conditional_t<Const, const concat_view<V1, V2>,
                                      concat_view<V1, V2> >;

    template <bool Const>
    class iterator {
        using Parent = base_t<Const>;
        using FirstBase = std::conditional_t<Const, const V1, V1>;
        using SecondBase = std::conditional_t<Const, const V2, V2>;

        std::ranges::iterator_t<FirstBase> _first_it{};
        std::ranges::sentinel_t<FirstBase> _first_end{};
        std::ranges::iterator_t<SecondBase> _second_it{};
        std::ranges::sentinel_t<SecondBase> _second_end{};
        bool _in_first = true;

        void satisfy() {
            if(_in_first && _first_it == _first_end) _in_first = false;
        }

    public:
        using iterator_concept = std::input_iterator_tag;
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type =
            std::common_type_t<std::ranges::range_value_t<FirstBase>,
                               std::ranges::range_value_t<SecondBase> >;
        using reference = std::common_reference_t<
            std::ranges::range_reference_t<FirstBase>,
            std::ranges::range_reference_t<SecondBase> >;

        iterator() = default;

        iterator(Parent & parent)
            : _first_it(std::ranges::begin(parent._first))
            , _first_end(std::ranges::end(parent._first))
            , _second_it(std::ranges::begin(parent._second))
            , _second_end(std::ranges::end(parent._second)) {
            satisfy();
        }

        reference operator*() const {
            return _in_first ? *_first_it : *_second_it;
        }

        iterator & operator++() {
            if(_in_first) {
                ++_first_it;
                satisfy();
            } else {
                ++_second_it;
            }
            return *this;
        }

        void operator++(int) { ++(*this); }

        friend bool operator==(const iterator & it,
                               std::default_sentinel_t) noexcept {
            if(it._in_first) return false;
            return it._second_it == it._second_end;
        }
    };

public:
    concat_view()
        requires std::default_initializable<V1> &&
                     std::default_initializable<V2>
    = default;

    constexpr concat_view(V1 first, V2 second)
        : _first(std::move(first)), _second(std::move(second)) {}

    constexpr auto begin() { return iterator<false>(*this); }

    constexpr auto begin() const
        requires std::ranges::range<const V1> && std::ranges::range<const V2>
    {
        return iterator<true>(*this);
    }

    constexpr auto end() const noexcept { return std::default_sentinel; }
    constexpr auto end() noexcept { return std::default_sentinel; }
};

template <std::ranges::viewable_range R1, std::ranges::viewable_range R2>
concat_view(std::views::all_t<R1>, std::views::all_t<R2>)
    -> concat_view<std::views::all_t<R1>, std::views::all_t<R2> >;

struct concat_fn {
    template <std::ranges::viewable_range R1, std::ranges::viewable_range R2>
        requires concat_view_compatible<std::views::all_t<R1>,
                                        std::views::all_t<R2> >
    constexpr auto operator()(R1 && r1, R2 && r2) const {
        return concat_view(std::views::all(std::forward<R1>(r1)),
                           std::views::all(std::forward<R2>(r2)));
    }
};

inline constexpr concat_fn concat{};

#endif

}  // namespace views
}  // namespace detail
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DETAIL_CONCAT_VIEW_HPP
