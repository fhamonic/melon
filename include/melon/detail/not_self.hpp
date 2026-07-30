#pragma once

#include <concepts>
#include <type_traits>

namespace melon {
namespace detail {

// Guard for single-argument constructor templates taking a forwarding
// reference. Unconstrained, such a template beats the copy constructor for a
// non-const lvalue of the class type -- it deduces an exact match where the
// copy constructor needs an added const -- so `X x2(x);` hijacks the copy and
// hard-errors inside the body. `explicit` hides the problem by rescuing the
// copy-initialisation spelling `auto x2 = x;`, which is why it is easy to miss.
//
// The cv-ref stripping is the load-bearing part: the constructor sees the
// *deduced* type, which for an lvalue is `X &` -- a reference is not a
// specialization of anything, so a guard written against the raw deduced type
// never fires where it is needed.
//
// Only the class's own specialization competes with its copy/move
// constructors, so this is deliberately narrower than
// `!specialization_of<T, X>`: converting from a *different* specialization
// (a subgraph of a subgraph, a reverse of a reverse) stays available.
template <typename T, typename Self>
concept not_self = !std::same_as<std::remove_cvref_t<T>, Self>;

}  // namespace detail
}  // namespace melon
