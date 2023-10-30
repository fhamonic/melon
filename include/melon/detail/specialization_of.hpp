#ifndef MELON_SPECIALIZATION_OF_HPP
#define MELON_SPECIALIZATION_OF_HPP

#include <concepts>
#include <ranges>
#include <type_traits>

namespace fhamonic {
namespace melon {

namespace __detail {
template <typename _Tp, template <typename...> typename _Primary>
struct __is_specialization_of : std::false_type {};

template <template <typename...> typename _Primary, typename... _Args>
struct __is_specialization_of<_Primary<_Args...>, _Primary> : std::true_type {};

template <typename _Tp, template <typename...> typename _Primary>
concept __specialization_of = __is_specialization_of<_Tp, _Primary>::value;

template <typename _Tp>
inline constexpr int _range_rank() {
    if constexpr(__specialization_of<_Tp, std::ranges::iota_view>)
        return 3;
    else if constexpr(std::ranges::contiguous_range<_Tp>)
        return 2;
    else if constexpr(std::ranges::viewable_range<_Tp>)
        return 1;
    else
        return 0;
};
}  // namespace __detail

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_SPECIALIZATION_OF_HPP