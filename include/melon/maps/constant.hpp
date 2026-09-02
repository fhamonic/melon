#pragma once

#include <type_traits>

#include "melon/mapping.hpp"

namespace melon {
namespace maps {

// The constant is an NTTP, not a stored member, so a constant is an empty
// type -- what lets views::subgraph's true_map specializations cost nothing.
// Only structural types qualify as NTTPs; a runtime constant is
// maps::function over a capturing lambda.
template <auto V>
struct constant : public mapping_view_base {
    [[nodiscard]] constexpr auto operator[](const auto &) const
        noexcept(std::is_nothrow_copy_constructible_v<decltype(V)>) {
        return V;
    }
};

using true_map = constant<true>;
using false_map = constant<false>;

}  // namespace maps
}  // namespace melon
