#ifndef MELON_DIJKSTRA_SEMIRINGS_HPP
#define MELON_DIJKSTRA_SEMIRINGS_HPP

#include <functional>

namespace fhamonic {
namespace melon {

template <typename T>
struct DijkstraShortestPathSemiring {
    static constexpr T zero = static_cast<T>(0);
    static constexpr std::plus<T> plus{};
    static constexpr std::less<T> less{};
};

template <typename T>
struct DijkstraMostProbablePathSemiring {
    static constexpr T zero = static_cast<T>(1);
    static constexpr std::multiplies<T> plus{};
    static constexpr std::greater<T> less{};
};

template <typename T>
struct DijkstraMaxFlowPathSemiring {
    static constexpr T zero = std::numeric_limits<T>::max();
    static constexpr auto plus = [](const T & a, const T & b){ return std::min(a, b); };
    static constexpr std::greater<T> less{};
};

template <typename T>
struct DijkstraSpanningTreeSemiring {
    static constexpr T zero = static_cast<T>(0);
    static constexpr auto plus = [](const T & a, const T & b){ return b; };
    static constexpr std::less<T> less{};
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DIJKSTRA_SEMIRINGS_HPP
