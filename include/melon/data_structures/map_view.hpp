#ifndef MELON_MAP_VIEW_HPP
#define MELON_MAP_VIEW_HPP

#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <vector>

namespace fhamonic {
namespace melon {

template <typename F>
class MapView {
private:
    F _f;

public:
    MapView(F f) : _f(f){};
    reference operator[](size_type i) noexcept { return _f(i); }
    const_reference operator[](size_type i) const noexcept { return _f(i); }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_MAP_VIEW_HPP