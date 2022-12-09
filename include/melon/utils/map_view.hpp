#ifndef MELON_UTILS_MAP_VIEW_HPP
#define MELON_UTILS_MAP_VIEW_HPP

#include <utility>

namespace fhamonic {
namespace melon {

template <typename F>
class map_view {
private:
    F _func;

public:
    [[nodiscard]] constexpr map_view(F && f) : _func(std::forward<F>(f)) {}
    [[nodiscard]] constexpr auto operator[](const auto & k) const noexcept {
        return _func(k);
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILS_MAP_VIEW_HPP