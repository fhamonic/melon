#ifndef MELON_VALUE_MAP_HPP
#define MELON_VALUE_MAP_HPP

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace fhamonic {
namespace melon {

template <typename M, typename K>
concept value_map = requires(M m, K k) { m[k]; };

template <typename M, typename K>
    requires value_map<M, K>
using mapped_reference_t = decltype(std::declval<M>()[std::declval<K>()]);

template <typename M, typename K>
    requires value_map<M, K>
using mapped_value_t = std::decay_t<mapped_reference_t<M, K>>;

template <typename M, typename K>
concept input_value_map = value_map<M, K> && !
std::same_as<mapped_value_t<M, K>, void>;

template <typename M, typename K, typename V>
concept input_value_map_of =
    value_map<M, K> && std::same_as<mapped_value_t<M, K>, V>;

template <typename M, typename K>
concept output_value_map = input_value_map<M, K> &&
                           requires(M map, K key, mapped_value_t<M, K> value) {
                               {
                                   map[key] = value
                                   } -> std::same_as<mapped_reference_t<M, K>>;
                           };

template <typename M, typename K, typename V>
concept output_value_map_of =
    output_value_map<M, K> && std::same_as<mapped_value_t<M, K>, V>;

template <typename M, typename K>
concept contiguous_value_map =
    output_value_map<M, K> && std::integral<K> &&
    requires(M & __m) {
        {
            __m.data()
            } -> std::same_as<std::add_pointer_t<mapped_value_t<M, K>>>;
    };

template <typename M, typename K, typename V>
concept contiguous_value_map_of =
    contiguous_value_map<M, K> && std::same_as<mapped_value_t<M, K>, V>;

// TODO refactor with the same rationale as views::all
template <typename _ValueMap>
class ref_value_map {
private:
    _ValueMap * _M_r;

public:
    constexpr ref_value_map(_ValueMap & __t) noexcept
        : _M_r(std::addressof(__t)) {}

    constexpr _ValueMap & base() const { return *_M_r; }

    template <typename _Key>
    constexpr auto operator[](_Key && __k) const
        requires input_value_map<std::add_const_t<_ValueMap>, _Key> ||
                 requires(const _ValueMap & __m, _Key & __k) { __m.at(__k); }
    {
        if constexpr(input_value_map<std::add_const_t<_ValueMap>, _Key>)
            return _M_r->operator[](__k);
        else
            return _M_r->at(__k);
    }

    template <typename _Key>
    constexpr auto operator[](_Key && __k)
        requires input_value_map<_ValueMap, _Key>
    {
        return _M_r->operator[](__k);
    }

    constexpr auto data() const
        requires std::is_pointer_v<decltype(std::declval<_ValueMap>().data())>
    {
        return _M_r->data();
    }
};
namespace views {
// namespace __cust_access {
template <typename _Tp>
concept __can_ref_value_map = requires { ref_value_map{std::declval<_Tp>()}; };

//   template<typename _Range>
// concept __can_owning_view = requires { owning_view{std::declval<_Range>()};
// };
// struct _All : __adaptor::_RangeAdaptorClosure
// {
//   template<typename _Range>
// static constexpr bool
// _S_noexcept()
// {
//   if constexpr (view<decay_t<_Range>>)
//     return is_nothrow_constructible_v<decay_t<_Range>, _Range>;
//   else if constexpr (__detail::__can_ref_view<_Range>)
//     return true;
//   else
//     return noexcept(owning_view{std::declval<_Range>()});
// }

//   template<viewable_range _Range>
// requires view<decay_t<_Range>>
//   || __detail::__can_ref_view<_Range>
//   || __detail::__can_owning_view<_Range>
// constexpr auto
// operator() [[nodiscard]] (_Range&& __r) const
// noexcept(_S_noexcept<_Range>())
// {
//   if constexpr (view<decay_t<_Range>>)
//     return std::forward<_Range>(__r);
//   else if constexpr (__detail::__can_ref_view<_Range>)
//     return ref_view{std::forward<_Range>(__r)};
//   else
//     return owning_view{std::forward<_Range>(__r)};
// }

//   static constexpr bool _S_has_simple_call_op = true;
// };
// }
// inline namespace __cust {
// inline constexpr _All all;
// }
// template<viewable_range _Range>
//   using all_t = decltype(all(std::declval<_Range>()));

template <typename F>
class map {
private:
    F _func;

public:
    [[nodiscard]] constexpr map(F && f) : _func(std::forward<F>(f)) {}
    [[nodiscard]] constexpr auto operator[](const auto & k) const noexcept {
        return _func(k);
    }
};
}  // namespace views

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_VALUE_MAP_HPP