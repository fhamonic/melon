#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

#include "melon/detail/movable_box.hpp"

namespace melon {

namespace detail {

// Syntactic entry point: `m[k]` on an lvalue map with an rvalue key -- the
// exact expression the mapped_* aliases below evaluate, so the probe and the
// aliases cannot disagree on either operand's value category. The public
// `mapping` concept adds const-readability on top; the aliases must stay
// constrained on this looser form to remain usable for write-only maps.
template <typename Map, typename Key>
concept subscriptable_with =
    requires(Map & m, Key && k) { m[std::forward<Key>(k)]; };

}  // namespace detail

template <typename Map, typename Key>
    requires detail::subscriptable_with<Map, Key>
using mapped_reference_t = decltype(std::declval<Map &>()[std::declval<Key>()]);

template <typename Map, typename Key>
    requires detail::subscriptable_with<Map, Key>
using mapped_const_reference_t =
    decltype(std::declval<const Map &>()[std::declval<Key>()]);

template <typename Map, typename Key>
    requires detail::subscriptable_with<Map, Key>
using mapped_value_t = std::decay_t<mapped_const_reference_t<Map, Key>>;

// A mapping is *readable through a const access*: mapped_value_t goes through
// mapped_const_reference_t, so its substitution fails for maps whose
// subscript cannot be offered const -- std::map's inserting operator[] being
// the canonical case. Such maps are used through maps::mapping_all, whose
// views subscript via at() on a const base.
template <typename Map, typename Key>
concept mapping = detail::subscriptable_with<Map, Key> &&
                  !std::same_as<mapped_value_t<Map, Key>, void>;

template <typename Map, typename Key, typename Value>
concept mapping_of =
    mapping<Map, Key> && std::same_as<mapped_value_t<Map, Key>, Value>;

// The disequality is not redundant with the requires-expression below. A
// subscript returning a prvalue of the value type stores nothing -- `m[k] = v`
// assigns into a temporary -- yet it satisfies that expression, assigning to a
// class prvalue being well-formed. Only scalar value types are rejected
// without it, so dropping it admits computed maps of class type whose every
// write is silently discarded. The write probe constrains no return type (a
// const-assignable proxy in the C++23 vector<bool> style returns
// `const proxy &`, as static_filter_map's does), forwards the key with the
// value category subscriptable_with committed to, and writes from an rvalue
// so move-only value types model the concept.
template <typename Map, typename Key>
concept output_mapping =
    mapping<Map, Key> &&
    !std::same_as<mapped_reference_t<Map, Key>, mapped_value_t<Map, Key>> &&
    requires(Map & map, Key && key, mapped_value_t<Map, Key> & value) {
        map[std::forward<Key>(key)] = std::move(value);
    };

template <typename Map, typename Key, typename Value>
concept output_mapping_of =
    output_mapping<Map, Key> && std::same_as<mapped_value_t<Map, Key>, Value>;

// `data()` only has to give read access to the contiguous block: a const map
// hands out `const V *`, a mutable one `V *`. Requiring exactly `V *` here
// would push containers into declaring a const-incorrect `data() const` just
// to model the concept.
template <typename Map, typename Key>
concept contiguous_mapping =
    mapping<Map, Key> && std::integral<Key> && requires(Map & m) {
        {
            m.data()
        } -> std::convertible_to<
              std::add_pointer_t<std::add_const_t<mapped_value_t<Map, Key>>>>;
    };

template <typename Map, typename Key, typename Value>
concept contiguous_mapping_of = contiguous_mapping<Map, Key> &&
                                std::same_as<mapped_value_t<Map, Key>, Value>;

struct mapping_view_base {};

template <typename T>
inline constexpr bool enable_mapping_view =
    std::derived_from<T, mapping_view_base>;

template <typename Map, typename Key>
concept mapping_view =
    mapping<Map, Key> && std::movable<Map> && enable_mapping_view<Map>;

namespace detail {

template <typename Map, typename Key>
constexpr decltype(auto) mapping_subscript(Map & m, Key && k) {
    if constexpr(requires { m[std::forward<Key>(k)]; })
        return m[std::forward<Key>(k)];
    else if constexpr(requires { m(std::forward<Key>(k)); })
        return m(std::forward<Key>(k));
    else if constexpr(requires { m.at(std::forward<Key>(k)); })
        return m.at(std::forward<Key>(k));
    else
        static_assert(false,
                      "melon: this type cannot be used as a mapping with "
                      "this key; it must provide operator[](key), "
                      "operator()(key) or at(key).");
}

// One concept per branch of the dispatch above, in the same order: a fourth
// spelling added there and not here makes can_mapping_subscript reject a map
// the dispatch handles, silently removing its subscript from the overload set.
template <typename Map, typename Key>
concept callable_with =
    requires(Map & m, Key && k) { m(std::forward<Key>(k)); };

template <typename Map, typename Key>
concept at_callable_with =
    requires(Map & m, Key && k) { m.at(std::forward<Key>(k)); };

// mapping_subscript ends in a static_assert and is reached through members
// returning decltype(auto), so computing such a member's type instantiates the
// body and the failure escapes the immediate context. Every member that exposes
// it is constrained on this: drop the constraint and asking `mapping<M, K>`
// about a map the dispatch rejects fails to compile instead of yielding false,
// which no requires-clause can recover from. A mutable lambda is what reaches
// this, its operator() being non-const.
template <typename Map, typename Key>
concept can_mapping_subscript =
    subscriptable_with<Map, Key> || callable_with<Map, Key> ||
    at_callable_with<Map, Key>;

// Enumerates the *non-const* shapes rather than negating the const ones, so
// that the default for anything not listed -- a ref-qualified operator(), an
// unusual signature -- is false. A wrong `false` here costs nothing (the
// callable falls through to the ordinary concept machinery); a wrong `true`
// would reject a legitimate const-callable map outright.
template <typename T>
inline constexpr bool is_non_const_call_operator = false;
template <typename C, typename R, typename... Args>
inline constexpr bool is_non_const_call_operator<R (C::*)(Args...)> = true;
template <typename C, typename R, typename... Args>
inline constexpr bool is_non_const_call_operator<R (C::*)(Args...) noexcept> =
    true;

// Taking the address of operator() is ill-formed for a *generic* lambda, whose
// call operator is a template, so those answer false and are left to the
// concepts to judge.
template <typename F>
concept has_non_const_call_operator = requires {
    { &F::operator() };
    requires is_non_const_call_operator<decltype(&F::operator())>;
};

}  // namespace detail

template <typename Map>
    requires std::is_object_v<Map>
class mapping_ref_view;

namespace detail {

template <typename Map>
concept can_mapping_ref_view =
    requires { mapping_ref_view{std::declval<Map>()}; };

}  // namespace detail

// Reference view: shallow const, like std::ranges::ref_view -- constness of the
// access is carried by Map itself (mapping_ref_view<const M> reads const,
// mapping_ref_view<M> reads mutable, through a const view object).
template <typename Map>
    requires std::is_object_v<Map>
class mapping_ref_view : public mapping_view_base {
private:
    Map * _map;

    // Rejects rvalues the way std::ranges::ref_view does, and not redundantly
    // with the constructor's `convertible_to<T, Map &>`: when Map is
    // const-qualified an rvalue *is* convertible to `Map &`, so without the
    // deleted overload the view would store the address of a temporary.
    static void bindable_test(Map &);
    static void bindable_test(Map &&) = delete;

public:
    template <typename T>
        requires(!std::same_as<std::remove_cvref_t<T>, mapping_ref_view>) &&
                std::convertible_to<T, Map &> &&
                requires { bindable_test(std::declval<T>()); }
    constexpr mapping_ref_view(T && t) noexcept(
        noexcept(static_cast<Map &>(std::declval<T>())))
        : _map(std::addressof(static_cast<Map &>(std::forward<T>(t)))) {}

    [[nodiscard]] constexpr Map & base() const noexcept { return *_map; }

    // Map, not `const Map`: the access goes through `Map &` however const the
    // view object is, so asking about `const Map` would drop the subscript for
    // every view over a map that is only non-const readable.
    template <typename Key>
        requires detail::can_mapping_subscript<Map, Key>
    [[nodiscard]] constexpr decltype(auto) operator[](Key && k) const {
        return detail::mapping_subscript(*_map, std::forward<Key>(k));
    }

    [[nodiscard]] constexpr auto data() const
        requires std::is_pointer_v<decltype(std::declval<Map &>().data())>
    {
        return _map->data();
    }
};

template <typename Map>
mapping_ref_view(Map &) -> mapping_ref_view<Map>;

// Owning view: deep const, like std::ranges::owning_view. Constructible
// from rvalues only; storage goes through movable_box so a view owning a
// capturing lambda remains std::movable.
template <typename Map>
    requires std::move_constructible<Map> && std::is_object_v<Map>
class mapping_owning_view : public mapping_view_base {
private:
    [[no_unique_address]] detail::movable_box<Map> _map;

public:
    constexpr mapping_owning_view()
        requires std::default_initializable<Map>
    = default;
    constexpr mapping_owning_view(Map && m) noexcept(
        std::is_nothrow_move_constructible_v<Map>)
        : _map(std::move(m)) {}

    constexpr mapping_owning_view(const mapping_owning_view &) = default;
    constexpr mapping_owning_view(mapping_owning_view &&) = default;
    constexpr mapping_owning_view & operator=(const mapping_owning_view &) =
        default;
    constexpr mapping_owning_view & operator=(mapping_owning_view &&) = default;

    [[nodiscard]] constexpr Map & base() & noexcept { return *_map; }
    [[nodiscard]] constexpr const Map & base() const & noexcept {
        return *_map;
    }
    [[nodiscard]] constexpr Map && base() && noexcept {
        return *std::move(_map);
    }

    // Each guard asks about the qualification its own overload subscripts
    // through. Spelling both `Map` admits the const overload for a map that is
    // only non-const readable, and computing its return type then hard-errors
    // inside mapping_subscript -- what the guards exist to prevent.
    template <typename Key>
        requires detail::can_mapping_subscript<Map, Key>
    [[nodiscard]] constexpr decltype(auto) operator[](Key && k) {
        return detail::mapping_subscript(*_map, std::forward<Key>(k));
    }
    template <typename Key>
        requires detail::can_mapping_subscript<const Map, Key>
    [[nodiscard]] constexpr decltype(auto) operator[](Key && k) const {
        return detail::mapping_subscript(*_map, std::forward<Key>(k));
    }

    [[nodiscard]] constexpr auto data()
        requires std::is_pointer_v<decltype(std::declval<Map &>().data())>
    {
        return (*_map).data();
    }
    [[nodiscard]] constexpr auto data() const
        requires std::is_pointer_v<decltype(std::declval<const Map &>().data())>
    {
        return (*_map).data();
    }
};

namespace detail {

// Declared here rather than next to can_mapping_ref_view: the CTAD probe
// needs mapping_owning_view to be complete. The const exclusion mirrors
// can_graph_owning_view in views/graph_view.hpp: a const rvalue cannot be
// moved from, so without it the owning branch silently deep-copies into a
// mapping_owning_view<const Map>.
template <typename Map>
concept can_mapping_owning_view =
    (!std::is_const_v<std::remove_reference_t<Map>>) &&
    requires { mapping_owning_view{std::declval<Map>()}; };

}  // namespace detail

// Mapping views live in melon::maps, graph views in melon::views: two different
// abstractions that happen to share the word "view".
namespace maps {

struct mapping_all_fn {
private:
    template <typename Map>
    static constexpr bool pass_through =
        std::movable<std::decay_t<Map>> &&
        enable_mapping_view<std::decay_t<Map>> &&
        std::constructible_from<std::decay_t<Map>, Map>;

    template <typename Map>
    static consteval bool is_noexcept() {
        if constexpr(pass_through<Map>)
            return std::is_nothrow_constructible_v<std::decay_t<Map>, Map>;
        else if constexpr(detail::can_mapping_ref_view<Map>)
            return noexcept(mapping_ref_view{std::declval<Map>()});
        else
            return noexcept(mapping_owning_view{std::declval<Map>()});
    }

public:
    // Constrained so that `requires { mapping_all(m); }` and `mapping_all_t<M>`
    // are usable in a requires-clause: without this the noexcept-specifier
    // below is instantiated for every argument and hard-errors outside the
    // immediate context instead of just removing the candidate.
    template <typename Map>
        requires pass_through<Map> || detail::can_mapping_ref_view<Map> ||
                 detail::can_mapping_owning_view<Map>
    [[nodiscard]] constexpr auto operator()(Map && m) const
        noexcept(is_noexcept<Map>()) {
        if constexpr(pass_through<Map>)
            return std::decay_t<Map>(std::forward<Map>(m));
        else if constexpr(detail::can_mapping_ref_view<Map>)
            return mapping_ref_view{std::forward<Map>(m)};
        else
            return mapping_owning_view{std::forward<Map>(m)};
    }
};

inline constexpr mapping_all_fn mapping_all{};

template <typename Map>
using mapping_all_t = decltype(mapping_all(std::declval<Map>()));

// The diagnostic belongs here and not in the concepts, which are probed in
// requires-clauses and must answer false rather than fire. maps::function
// is probed nowhere, so it can afford to be loud; the same assert added to
// mapping_all would break `requires { maps::mapping_all(x); }`.
template <typename F>
[[nodiscard]] constexpr auto function(F && f) {
    static_assert(
        !detail::has_non_const_call_operator<std::decay_t<F>>,
        "melon: this callable cannot be used as a mapping -- its operator() is "
        "not const (a `mutable` lambda), and melon reads maps through a const "
        "access -- the const-readability rule in docs/graphs/mappings.md. Do "
        "not let the map own the state: capture "
        "the storage and hand out a reference into it, as in "
        "maps::function([&storage](key_t k) -> value_t & { "
        "return storage[k]; }).");
    return mapping_owning_view<std::decay_t<F>>(
        std::decay_t<F>(std::forward<F>(f)));
}

struct identity : public mapping_view_base {
    template <typename T>
    [[nodiscard]] constexpr auto operator[](T && e) const
        noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, T>) {
        return std::forward<T>(e);
    }
};

}  // namespace maps

template <typename M, typename Target>
concept mapping_for = std::constructible_from<Target, maps::mapping_all_t<M>>;

}  // namespace melon
