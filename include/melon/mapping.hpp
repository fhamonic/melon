#ifndef MELON_mapping_HPP
#define MELON_mapping_HPP

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

#include "melon/detail/specialization_of.hpp"

namespace fhamonic {
namespace melon {

template <typename _Map, typename _Key>
concept mapping = requires(_Map m, _Key k) { m[k]; };

template <typename _Map, typename _Key>
    requires mapping<_Map, _Key>
using mapped_reference_t = decltype(std::declval<_Map>()[std::declval<_Key>()]);

template <typename _Map, typename _Key>
    requires mapping<_Map, _Key>
using mapped_const_reference_t =
    decltype(std::declval<std::add_const_t<_Map>>()[std::declval<_Key>()]);

template <typename _Map, typename _Key>
    requires mapping<_Map, _Key>
using mapped_value_t = std::decay_t<mapped_const_reference_t<_Map, _Key>>;

template <typename _Map, typename _Key>
concept input_mapping =
    mapping<_Map, _Key> && !std::same_as<mapped_value_t<_Map, _Key>, void>;

template <typename _Map, typename _Key, typename _Value>
concept input_mapping_of =
    mapping<_Map, _Key> && std::same_as<mapped_value_t<_Map, _Key>, _Value>;

template <typename _Map, typename _Key>
concept output_mapping =
    input_mapping<_Map, _Key> &&
    requires(_Map map, _Key key, mapped_value_t<_Map, _Key> value) {
        {
            map[key] = value
        } -> std::same_as<
            std::add_lvalue_reference_t<mapped_reference_t<_Map, _Key>>>;
    };

template <typename _Map, typename _Key, typename _Value>
concept output_mapping_of = output_mapping<_Map, _Key> &&
                            std::same_as<mapped_value_t<_Map, _Key>, _Value>;

template <typename _Map, typename _Key>
concept contiguous_mapping =
    input_mapping<_Map, _Key> && std::integral<_Key> && requires(_Map & __m) {
        {
            __m.data()
        } -> std::same_as<std::add_pointer_t<mapped_value_t<_Map, _Key>>>;
    };

template <typename _Map, typename _Key, typename _Value>
concept contiguous_mapping_of =
    contiguous_mapping<_Map, _Key> &&
    std::same_as<mapped_value_t<_Map, _Key>, _Value>;

struct mapping_view_base {};

template <typename _Tp>
inline constexpr bool enable_mapping_view =
    std::derived_from<_Tp, mapping_view_base>;

template <typename _Map>
concept mapping_view = std::movable<_Map> && enable_mapping_view<_Map>;

template <typename _Map>
class mapping_ref_view : public mapping_view_base {
private:
    _Map * _map;

public:
    template <typename _Tp>
        requires(!__detail::__specialization_of<_Tp, mapping_ref_view>)
    [[nodiscard]] constexpr explicit mapping_ref_view(_Tp && __m)
        : _map(std::addressof(static_cast<_Map &>(std::forward<_Tp>(__m)))) {}

    constexpr _Map & base() const { return *_map; }

    template <typename _Key>
    constexpr auto operator[](_Key && __k) const {
        if constexpr(requires(std::add_const_t<_Map> & __m) { __m[__k]; })
            return _map->operator[](__k);
        else if constexpr(requires(std::add_const_t<_Map> & __m) { __m(__k); })
            return _map->operator()(__k);
        else if constexpr(requires(std::add_const_t<_Map> & __m) {
                              __m.at(__k);
                          })
            return _map->at(__k);
    }

    template <typename _Key>
    constexpr decltype(auto) operator[](_Key && __k) {
        if constexpr(requires(_Map & __m) { __m[__k]; })
            return _map->operator[](__k);
        else if constexpr(requires(_Map & __m) { __m(__k); })
            return _map->operator()(__k);
        else if constexpr(requires(_Map & __m) { __m.at(__k); })
            return _map->at(__k);
    }

    constexpr auto data() const
        requires std::is_pointer_v<decltype(std::declval<_Map>().data())>
    {
        return _map->data();
    }
};

template <typename _Map>
mapping_ref_view(_Map &) -> mapping_ref_view<_Map>;

template <typename _Map>
// requires std::movable<_Map> // TODO fix this for lambdas
    requires std::move_constructible<_Map>
class mapping_owning_view : public mapping_view_base {
private:
    _Map _map;

public:
    constexpr mapping_owning_view(_Map && __m) noexcept(
        std::is_nothrow_move_constructible_v<_Map>)
        : _map(std::move(__m)) {}

    [[nodiscard]] mapping_owning_view()
        requires std::default_initializable<_Map>
    = default;
    [[nodiscard]] constexpr mapping_owning_view(const mapping_owning_view &) =
        default;
    [[nodiscard]] constexpr mapping_owning_view(mapping_owning_view &&) =
        default;

    constexpr mapping_owning_view & operator=(const mapping_owning_view &) =
        default;
    // constexpr mapping_owning_view & operator=(mapping_owning_view &&) =
    // default;

    constexpr _Map && base() && noexcept { return std::move(_map); }

    template <typename _Key>
    constexpr auto operator[](_Key && __k) const {
        if constexpr(requires(std::add_const_t<_Map> & __m) { __m[__k]; })
            return _map.operator[](__k);
        else if constexpr(requires(std::add_const_t<_Map> & __m) { __m(__k); })
            return _map.operator()(__k);
        else if constexpr(requires(std::add_const_t<_Map> & __m) {
                              __m.at(__k);
                          })
            return _map.at(__k);
    }

    template <typename _Key>
    constexpr decltype(auto) operator[](_Key && __k) {
        if constexpr(requires(_Map & __m) { __m[__k]; })
            return _map.operator[](__k);
        else if constexpr(requires(_Map & __m) { __m(__k); })
            return _map.operator()(__k);
        else if constexpr(requires(_Map & __m) { __m.at(__k); })
            return _map.at(__k);
    }

    constexpr auto data() const
        requires std::is_pointer_v<decltype(std::declval<_Map>().data())>
    {
        return _map.data();
    }
};

namespace views {
namespace __cust_access {
namespace __detail {
template <typename _Map>
concept __can_mapping_ref_view =
    requires { mapping_ref_view{std::declval<_Map>()}; };

template <typename _Map>
concept __can_mapping_owning_view =
    requires { mapping_owning_view{std::declval<_Map>()}; };
}  // namespace __detail

struct _MapAll {
    template <typename _Map>
    static constexpr bool _S_noexcept() {
        if constexpr(mapping_view<std::decay_t<_Map>>)
            return std::is_nothrow_constructible_v<std::decay_t<_Map>, _Map>;
        else if constexpr(__detail::__can_mapping_ref_view<_Map>)
            return true;
        else
            return noexcept(mapping_owning_view{std::declval<_Map>()});
    }

    template <typename _Map>
    constexpr auto operator() [[nodiscard]] (_Map && __m) const
        noexcept(_S_noexcept<_Map>()) {
        if constexpr(mapping_view<std::decay_t<_Map>>)
            return std::forward<_Map>(__m);
        else if constexpr(__detail::__can_mapping_ref_view<_Map>)
            return mapping_ref_view{std::forward<_Map>(__m)};
        else
            return mapping_owning_view{std::forward<_Map>(__m)};
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_MapAll mapping_all{};
}  // namespace __cust

template <typename _Map>
using mapping_all_t = decltype(mapping_all(std::declval<_Map>()));
}  // namespace views

namespace views {

template <typename F>
class map : public mapping_view_base {
private:
    F _func;

public:
    [[nodiscard]] constexpr map(F && f) : _func(std::forward<F>(f)) {}

    [[nodiscard]] constexpr map() = default;
    [[nodiscard]] constexpr map(const map &) = default;
    [[nodiscard]] constexpr map(map &&) = default;

    [[nodiscard]] constexpr auto operator[](const auto & k) const noexcept {
        return _func(k);
    }
};

struct true_map : public mapping_view_base {
    [[nodiscard]] constexpr bool operator[](const auto & k) const noexcept {
        return true;
    }
};

struct false_map : public mapping_view_base {
    [[nodiscard]] constexpr bool operator[](const auto & k) const noexcept {
        return false;
    }
};
}  // namespace views

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_mapping_HPP