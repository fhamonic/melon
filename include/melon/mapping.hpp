#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

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

// `data()` only has to give read access to the contiguous block: a const map
// hands out `const V *`, a mutable one `V *`. Requiring exactly `V *` here
// would push containers into declaring a const-incorrect `data() const` just
// to model the concept.
template <typename _Map, typename _Key>
concept contiguous_mapping =
    input_mapping<_Map, _Key> && std::integral<_Key> && requires(_Map & __m) {
        {
            __m.data()
        } -> std::convertible_to<
              std::add_pointer_t<std::add_const_t<mapped_value_t<_Map, _Key>>>>;
    };

template <typename _Map, typename _Key, typename _Value>
concept contiguous_mapping_of =
    contiguous_mapping<_Map, _Key> &&
    std::same_as<mapped_value_t<_Map, _Key>, _Value>;

struct mapping_view_base {};

template <typename _Tp>
inline constexpr bool enable_mapping_view =
    std::derived_from<_Tp, mapping_view_base>;

template <typename _Map, typename _Key>
concept mapping_view =
    mapping<_Map, _Key> && std::movable<_Map> && enable_mapping_view<_Map>;

namespace __detail {

// Single point of dispatch between the three storage protocols a mapping
// can expose. Constness and value category of `__m` are the caller's: each
// requires-clause tests exactly the expression the branch evaluates.
template <typename _Map, typename _Key>
constexpr decltype(auto) __mapping_subscript(_Map & __m, _Key && __k) {
    if constexpr(requires { __m[std::forward<_Key>(__k)]; })
        return __m[std::forward<_Key>(__k)];
    else if constexpr(requires { __m(std::forward<_Key>(__k)); })
        return __m(std::forward<_Key>(__k));
    else if constexpr(requires { __m.at(std::forward<_Key>(__k)); })
        return __m.at(std::forward<_Key>(__k));
    else
        static_assert(false,
                      "melon: this type cannot be used as a mapping with "
                      "this key; it must provide operator[](key), "
                      "operator()(key) or at(key).");
}

// std::ranges-style movable-box: makes a non-assignable _Map (typically a
// capturing lambda) assignable through destroy + reconstruct, so that the
// views owning one stay std::movable. The reconstruct path is only enabled
// when the move cannot throw, so a failed assignment can never leave the
// box destroyed.
template <typename _Tp>
    requires std::move_constructible<_Tp> && std::is_object_v<_Tp>
class __movable_box {
private:
    [[no_unique_address]] _Tp _value;

public:
    constexpr __movable_box()
        requires std::default_initializable<_Tp>
    = default;
    constexpr __movable_box(_Tp && __t) noexcept(
        std::is_nothrow_move_constructible_v<_Tp>)
        : _value(std::move(__t)) {}
    constexpr __movable_box(const _Tp & __t)
        requires std::copy_constructible<_Tp>
        : _value(__t) {}

    constexpr __movable_box(const __movable_box &) = default;
    constexpr __movable_box(__movable_box &&) = default;

    constexpr __movable_box & operator=(__movable_box && __o) noexcept(
        std::movable<_Tp> ? std::is_nothrow_move_assignable_v<_Tp> : true)
        requires std::movable<_Tp> || std::is_nothrow_move_constructible_v<_Tp>
    {
        if constexpr(std::movable<_Tp>) {
            _value = std::move(__o._value);
        } else {
            if(this != std::addressof(__o)) {
                std::destroy_at(std::addressof(_value));
                std::construct_at(std::addressof(_value),
                                  std::move(__o._value));
            }
        }
        return *this;
    }
    constexpr __movable_box & operator=(const __movable_box & __o) noexcept(
        std::copyable<_Tp> ? std::is_nothrow_copy_assignable_v<_Tp> : false)
        requires std::copyable<_Tp> ||
                 (std::copy_constructible<_Tp> &&
                  std::is_nothrow_move_constructible_v<_Tp>)
    {
        if constexpr(std::copyable<_Tp>) {
            _value = __o._value;
        } else {
            if(this != std::addressof(__o)) {
                _Tp __tmp(__o._value);
                std::destroy_at(std::addressof(_value));
                std::construct_at(std::addressof(_value), std::move(__tmp));
            }
        }
        return *this;
    }

    constexpr _Tp & operator*() & noexcept { return _value; }
    constexpr const _Tp & operator*() const & noexcept { return _value; }
    constexpr _Tp && operator*() && noexcept { return std::move(_value); }
};

}  // namespace __detail

template <typename _Map>
    requires std::is_object_v<_Map>
class mapping_ref_view;

namespace __detail {

template <typename _Map>
concept __can_mapping_ref_view =
    requires { mapping_ref_view{std::declval<_Map>()}; };

}  // namespace __detail

// Reference view: shallow const, like std::ranges::ref_view — constness of
// the access is carried by _Map itself (mapping_ref_view<const M> reads
// const, mapping_ref_view<M> reads mutable, through a const view object).
template <typename _Map>
    requires std::is_object_v<_Map>
class mapping_ref_view : public mapping_view_base {
private:
    _Map * _map;

    static void __bindable_test(_Map &);
    static void __bindable_test(_Map &&) = delete;

public:
    template <typename _Tp>
        requires(!std::same_as<std::remove_cvref_t<_Tp>, mapping_ref_view>) &&
                std::convertible_to<_Tp, _Map &> &&
                requires { __bindable_test(std::declval<_Tp>()); }
    constexpr mapping_ref_view(_Tp && __t) noexcept(
        noexcept(static_cast<_Map &>(std::declval<_Tp>())))
        : _map(std::addressof(static_cast<_Map &>(std::forward<_Tp>(__t)))) {}

    constexpr _Map & base() const noexcept { return *_map; }

    template <typename _Key>
    [[nodiscard]] constexpr decltype(auto) operator[](_Key && __k) const {
        return __detail::__mapping_subscript(*_map, std::forward<_Key>(__k));
    }

    [[nodiscard]] constexpr auto data() const
        requires std::is_pointer_v<decltype(std::declval<_Map &>().data())>
    {
        return _map->data();
    }
};

template <typename _Map>
mapping_ref_view(_Map &) -> mapping_ref_view<_Map>;

// Owning view: deep const, like std::ranges::owning_view. Constructible
// from rvalues only; storage goes through __movable_box so a view owning a
// capturing lambda remains std::movable.
template <typename _Map>
    requires std::move_constructible<_Map> && std::is_object_v<_Map>
class mapping_owning_view : public mapping_view_base {
private:
    [[no_unique_address]] __detail::__movable_box<_Map> _map;

public:
    constexpr mapping_owning_view()
        requires std::default_initializable<_Map>
    = default;
    constexpr mapping_owning_view(_Map && __m) noexcept(
        std::is_nothrow_move_constructible_v<_Map>)
        : _map(std::move(__m)) {}

    constexpr mapping_owning_view(const mapping_owning_view &) = default;
    constexpr mapping_owning_view(mapping_owning_view &&) = default;
    constexpr mapping_owning_view & operator=(const mapping_owning_view &) =
        default;
    constexpr mapping_owning_view & operator=(mapping_owning_view &&) = default;

    constexpr _Map & base() & noexcept { return *_map; }
    constexpr const _Map & base() const & noexcept { return *_map; }
    constexpr _Map && base() && noexcept { return *std::move(_map); }

    template <typename _Key>
    [[nodiscard]] constexpr decltype(auto) operator[](_Key && __k) {
        return __detail::__mapping_subscript(*_map, std::forward<_Key>(__k));
    }
    template <typename _Key>
    [[nodiscard]] constexpr decltype(auto) operator[](_Key && __k) const {
        return __detail::__mapping_subscript(*_map, std::forward<_Key>(__k));
    }

    [[nodiscard]] constexpr auto data()
        requires std::is_pointer_v<decltype(std::declval<_Map &>().data())>
    {
        return (*_map).data();
    }
    [[nodiscard]] constexpr auto data() const
        requires std::is_pointer_v<
            decltype(std::declval<const _Map &>().data())>
    {
        return (*_map).data();
    }
};

namespace __detail {

// Declared here rather than next to __can_mapping_ref_view: the CTAD probe
// needs mapping_owning_view to be complete.
template <typename _Map>
concept __can_mapping_owning_view =
    requires { mapping_owning_view{std::declval<_Map>()}; };

}  // namespace __detail

namespace views {

struct _MappingAll {
private:
    template <typename _Map>
    static constexpr bool __pass_through =
        std::movable<std::decay_t<_Map>> &&
        enable_mapping_view<std::decay_t<_Map>> &&
        std::constructible_from<std::decay_t<_Map>, _Map>;

    template <typename _Map>
    static consteval bool _S_noexcept() {
        if constexpr(__pass_through<_Map>)
            return std::is_nothrow_constructible_v<std::decay_t<_Map>, _Map>;
        else if constexpr(__detail::__can_mapping_ref_view<_Map>)
            return noexcept(mapping_ref_view{std::declval<_Map>()});
        else
            return noexcept(mapping_owning_view{std::declval<_Map>()});
    }

public:
    // Constrained so that `requires { mapping_all(m); }` and `mapping_all_t<M>`
    // are usable in a requires-clause: without this the noexcept-specifier
    // below is instantiated for every argument and hard-errors outside the
    // immediate context instead of just removing the candidate.
    template <typename _Map>
        requires __pass_through<_Map> ||
                 __detail::__can_mapping_ref_view<_Map> ||
                 __detail::__can_mapping_owning_view<_Map>
    [[nodiscard]] constexpr auto operator()(_Map && __m) const
        noexcept(_S_noexcept<_Map>()) {
        if constexpr(__pass_through<_Map>)
            return std::decay_t<_Map>(std::forward<_Map>(__m));
        else if constexpr(__detail::__can_mapping_ref_view<_Map>)
            return mapping_ref_view{std::forward<_Map>(__m)};
        else
            return mapping_owning_view{std::forward<_Map>(__m)};
    }
};

inline constexpr _MappingAll mapping_all{};

template <typename _Map>
using mapping_all_t = decltype(mapping_all(std::declval<_Map>()));

template <typename F>
[[nodiscard]] constexpr auto map(F && f) {
    return mapping_owning_view<std::decay_t<F>>(
        std::decay_t<F>(std::forward<F>(f)));
}

struct true_map : public mapping_view_base {
    [[nodiscard]] constexpr bool operator[](const auto &) const noexcept {
        return true;
    }
};

struct false_map : public mapping_view_base {
    [[nodiscard]] constexpr bool operator[](const auto &) const noexcept {
        return false;
    }
};

struct identity_map : public mapping_view_base {
    template <typename T>
    [[nodiscard]] constexpr auto operator[](T && e) const
        noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, T>) {
        return std::forward<T>(e);
    }
};

template <std::size_t... I>
struct element_map : public mapping_view_base {
private:
    // std::get is noexcept for tuple/pair/array but *not* for std::variant,
    // where it throws std::bad_variant_access. An unconditional noexcept on
    // the chain below turned that throw into std::terminate.
    template <typename T, std::size_t First, std::size_t... Rest>
    static consteval bool __chain_is_nothrow() {
        if constexpr(sizeof...(Rest) == 0) {
            return noexcept(std::get<First>(std::declval<T>()));
        } else {
            return noexcept(std::get<First>(std::declval<T>())) &&
                   __chain_is_nothrow<
                       decltype(std::get<First>(std::declval<T>())), Rest...>();
        }
    }

    template <std::size_t First, std::size_t... Rest, typename T>
    static constexpr decltype(auto) get_chain(T && e) noexcept(
        __chain_is_nothrow<T, First, Rest...>()) {
        if constexpr(sizeof...(Rest) == 0) {
            return std::get<First>(std::forward<T>(e));
        } else {
            return get_chain<Rest...>(std::get<First>(std::forward<T>(e)));
        }
    }

public:
    template <typename T>
    [[nodiscard]] constexpr decltype(auto) operator[](T && e) const
        noexcept(__chain_is_nothrow<T, I...>()) {
        return get_chain<I...>(std::forward<T>(e));
    }
};

}  // namespace views

}  // namespace melon
