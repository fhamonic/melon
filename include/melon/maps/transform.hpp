#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

#include "melon/detail/movable_box.hpp"
#include "melon/mapping.hpp"

namespace melon {

// In melon itself, like the other view classes; only the maps::transform
// adaptor lives in the nested namespace.
//
// Deep const, like mapping_owning_view: the const subscript reads the base
// through `const Map` and calls the projection through `const Func`. The
// projection applies to the mapped *value* -- `f(m[k])` -- never to the key.
template <typename Map, typename Func>
    requires std::movable<Map> && enable_mapping_view<Map> &&
             std::move_constructible<Func> && std::is_object_v<Func>
class transform_map_view : public mapping_view_base {
private:
    [[no_unique_address]] Map _map;
    [[no_unique_address]] detail::movable_box<Func> _func;

public:
    constexpr transform_map_view()
        requires std::default_initializable<Map> &&
                     std::default_initializable<Func>
    = default;
    constexpr transform_map_view(Map && map, Func && func) noexcept(
        std::is_nothrow_move_constructible_v<Map> &&
        std::is_nothrow_move_constructible_v<Func>)
        : _map(std::move(map)), _func(std::move(func)) {}

    constexpr transform_map_view(const transform_map_view &) = default;
    constexpr transform_map_view(transform_map_view &&) = default;
    constexpr transform_map_view & operator=(const transform_map_view &) =
        default;
    constexpr transform_map_view & operator=(transform_map_view &&) = default;

    [[nodiscard]] constexpr Map & base() & noexcept { return _map; }
    [[nodiscard]] constexpr const Map & base() const & noexcept { return _map; }
    [[nodiscard]] constexpr Map && base() && noexcept {
        return std::move(_map);
    }

    // Each guard asks about the qualification its own overload subscripts and
    // calls through, for the same reason as in mapping_owning_view: admitting
    // the const overload for a base or projection that is only non-const
    // usable would hard-error while computing its return type.
    template <typename Key>
        requires requires(Map & m, Func & f, Key && k) {
            f(m[std::forward<Key>(k)]);
        }
    [[nodiscard]] constexpr decltype(auto) operator[](Key && k) noexcept(
        noexcept((*_func)(_map[std::forward<Key>(k)]))) {
        return (*_func)(_map[std::forward<Key>(k)]);
    }
    template <typename Key>
        requires requires(const Map & m, const Func & f, Key && k) {
            f(m[std::forward<Key>(k)]);
        }
    [[nodiscard]] constexpr decltype(auto) operator[](Key && k) const
        noexcept(noexcept((*_func)(_map[std::forward<Key>(k)]))) {
        return (*_func)(_map[std::forward<Key>(k)]);
    }
};

namespace maps {

// The base goes through mapping_all -- an lvalue is referenced, an rvalue
// owned -- so a transform over a container can dangle no more than the
// container's own ref view would.
template <typename Map, typename Func>
    requires std::constructible_from<std::decay_t<Func>, Func> &&
             requires(Map && m) { mapping_all(std::forward<Map>(m)); }
[[nodiscard]] constexpr auto transform(Map && m, Func && f) {
    static_assert(
        !detail::has_non_const_call_operator<std::decay_t<Func>>,
        "melon: this callable cannot be used as a maps::transform projection "
        "-- its operator() is not const (a `mutable` lambda), and melon reads "
        "maps through a const access -- the const-readability rule in "
        "docs/graphs/mappings.md. Do not let the projection own the state: "
        "capture the storage and hand out a reference into it.");
    return transform_map_view<mapping_all_t<Map>, std::decay_t<Func>>(
        mapping_all(std::forward<Map>(m)),
        std::decay_t<Func>(std::forward<Func>(f)));
}

}  // namespace maps
}  // namespace melon
