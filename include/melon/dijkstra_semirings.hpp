#ifndef MELON_DIJKSTRA_SEMIRINGS_HPP
#define MELON_DIJKSTRA_SEMIRINGS_HPP

#include <functional>

namespace fhamonic {
namespace melon {

template <typename T>
struct DijkstraShortestPathSemiring {
    static const T zero = static_cast<T>(0);
    static const std::plus<T> plus;
    static const std::less<T> less;
};

template <typename T>
struct DijkstraMostProbablePathSemiring {
    static const T zero = static_cast<T>(1);
    static const std::multiplies<T> plus;
    static const std::greater<T> less;
};

template <typename T>
struct DijkstraMaxFlowPathSemiring {
    static const T zero = std::numeric_limits<T>::max();
    static const auto plus = [](const T & a, const T & b){ return std::min(a, b); };
    static const std::greater<T> less;
};

template <typename T>
struct DijkstraSpanningTreeSemiring {
    static const T zero = static_cast<T>(0);
    static const auto plus = [](const T & a, const T & b){ return b; };
    static const std::less<T> less;
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_DIJKSTRA_SEMIRINGS_HPP
