#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace melon {
namespace detail {

// std::ranges-style movable-box: makes a non-assignable Map (typically a
// capturing lambda) assignable through destroy + reconstruct, so that the
// views owning one stay std::movable. The reconstruct path is only enabled
// when the move cannot throw, so a failed assignment can never leave the
// box destroyed.
template <typename T>
    requires std::move_constructible<T> && std::is_object_v<T>
class movable_box {
private:
    [[no_unique_address]] T _value;

public:
    constexpr movable_box()
        requires std::default_initializable<T>
    = default;
    constexpr movable_box(T && t) noexcept(
        std::is_nothrow_move_constructible_v<T>)
        : _value(std::move(t)) {}
    constexpr movable_box(const T & t)
        requires std::copy_constructible<T>
        : _value(t) {}
    template <typename... Args>
        requires std::constructible_from<T, Args...>
    constexpr explicit movable_box(std::in_place_t, Args &&... args)
        : _value(std::forward<Args>(args)...) {}

    constexpr movable_box(const movable_box &) = default;
    constexpr movable_box(movable_box &&) = default;

    constexpr movable_box & operator=(movable_box && o) noexcept(
        std::movable<T> ? std::is_nothrow_move_assignable_v<T> : true)
        requires std::movable<T> || std::is_nothrow_move_constructible_v<T>
    {
        if constexpr(std::movable<T>) {
            _value = std::move(o._value);
        } else {
            if(this != std::addressof(o)) {
                std::destroy_at(std::addressof(_value));
                std::construct_at(std::addressof(_value), std::move(o._value));
            }
        }
        return *this;
    }
    constexpr movable_box & operator=(const movable_box & o) noexcept(
        std::copyable<T> ? std::is_nothrow_copy_assignable_v<T> : false)
        requires std::copyable<T> || (std::copy_constructible<T> &&
                                      std::is_nothrow_move_constructible_v<T>)
    {
        if constexpr(std::copyable<T>) {
            _value = o._value;
        } else {
            if(this != std::addressof(o)) {
                // The temporary is what makes the destroy safe: the copy is
                // the throwing step, so it must complete before _value dies.
                // Constructing straight from o._value would leave the box
                // destroyed if that copy threw.
                T tmp(o._value);
                std::destroy_at(std::addressof(_value));
                std::construct_at(std::addressof(_value), std::move(tmp));
            }
        }
        return *this;
    }

    constexpr T & operator*() & noexcept { return _value; }
    constexpr const T & operator*() const & noexcept { return _value; }
    constexpr T && operator*() && noexcept { return std::move(_value); }
};

}  // namespace detail
}  // namespace melon
