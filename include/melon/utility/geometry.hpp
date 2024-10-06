#ifndef GEOMETRIC_SYSTEMS_HPP
#define GEOMETRIC_SYSTEMS_HPP

#include <utility>

#include "melon/utility/rational.hpp"

namespace fhamonic {
namespace melon {
template <typename _Tp>
concept cartesian_point = requires(const _Tp & __t) {
    { std::get<0>(__t) };
    { std::get<1>(__t) };
};
template <typename _Tp>
concept cartesian_segment = requires(const _Tp & __t) {
    { std::get<0>(__t) } -> cartesian_point;
    { std::get<1>(__t) } -> cartesian_point;
};
template <typename _Tp>
concept cartesian_line = requires(const _Tp & __t) {
    { std::get<0>(__t) };
    { std::get<1>(__t) };
    { std::get<2>(__t) };
};

struct cartesian {
    struct point_xy_comparator {
        [[nodiscard]] constexpr bool operator()(
            const cartesian_point auto & p1,
            const cartesian_point auto & p2) const noexcept {
            if(std::get<0>(p1) == std::get<0>(p2))
                return std::get<1>(p1) < std::get<1>(p2);
            return std::get<0>(p1) < std::get<0>(p2);
        }
    };
    static constexpr auto segment_to_line(const cartesian_segment auto & s) {
        const auto a =
            std::get<1>(std::get<1>(s)) - std::get<1>(std::get<0>(s));
        const auto b =
            std::get<0>(std::get<0>(s)) - std::get<0>(std::get<1>(s));
        const auto c =
            a * std::get<0>(std::get<0>(s)) + b * std::get<1>(std::get<0>(s));
        return std::make_tuple(a, b, c);
    }
    static constexpr auto lines_intersection(const cartesian_line auto & A,
                                             const cartesian_line auto & B) {
        const auto determinant =
            std::get<0>(A) * std::get<1>(B) - std::get<1>(A) * std::get<0>(B);
        return determinant == 0
                   ? std::nullopt
                   : std::make_optional(std::make_pair(
                         rational(std::get<2>(A) * std::get<1>(B) -
                                      std::get<2>(B) * std::get<1>(A),
                                  determinant),
                         rational(std::get<0>(A) * std::get<2>(B) -
                                      std::get<0>(B) * std::get<2>(A),
                                  determinant)));
    }
    static constexpr auto line_slope(const cartesian_line auto & l) {
        return rational(std::get<1>(l) != 0 ? std::get<0>(l) : 1,
                        -std::get<1>(l));  // (1/0) represents infinity
    }
    static constexpr bool point_on_line(const cartesian_point auto & p,
                                        const cartesian_line auto & l) {
        return std::get<0>(l) * std::get<0>(p) +
                   std::get<1>(l) * std::get<1>(p) ==
               std::get<2>(l);
    }
    static constexpr auto point_on_segment(const cartesian_point auto & p,
                                           const cartesian_segment auto & s) {
        if(!point_on_line(p, segment_to_line(s))) return false;
        const point_xy_comparator cmp;
        const auto & [s_min, s_max] =
            std::minmax(std::get<0>(s), std::get<1>(s), cmp);
        return !cmp(p, s_min) && !cmp(s_max, p);
    }
    static constexpr auto segments_intersection(
        const cartesian_segment auto & A, const cartesian_segment auto & B) {
        const auto intersection_opt =
            lines_intersection(segment_to_line(A), segment_to_line(B));

        if(!intersection_opt.has_value()) return intersection_opt;
        const auto & intersection = intersection_opt.value();

        const auto & [Ax_min, Ax_max] = std::minmax(
            std::get<0>(std::get<0>(A)), std::get<0>(std::get<1>(A)));
        const auto & [Bx_min, Bx_max] = std::minmax(
            std::get<0>(std::get<0>(B)), std::get<0>(std::get<1>(B)));
        const auto & [Ay_min, Ay_max] = std::minmax(
            std::get<1>(std::get<0>(A)), std::get<1>(std::get<1>(A)));
        const auto & [By_min, By_max] = std::minmax(
            std::get<1>(std::get<0>(B)), std::get<1>(std::get<1>(B)));

        return (std::max(Ax_min, Bx_min) > std::get<0>(intersection) ||
                std::min(Ax_max, Bx_max) < std::get<0>(intersection) ||
                std::max(Ay_min, By_min) > std::get<1>(intersection) ||
                std::min(Ay_max, By_max) < std::get<1>(intersection))
                   ? std::nullopt
                   : intersection_opt;
    }
    static constexpr auto aligned_segments_overlap(
        const cartesian_segment auto & A, const cartesian_segment auto & B) {
        const point_xy_comparator cmp;
        const auto & [A_min, A_max] =
            std::minmax(std::get<0>(A), std::get<1>(A), cmp);
        const auto & [B_min, B_max] =
            std::minmax(std::get<0>(B), std::get<1>(B), cmp);

        return (cmp(A_max, B_min) || cmp(B_max, A_min))
                   ? std::nullopt
                   : std::make_optional(
                         std::make_pair(std::max(A_min, B_min, cmp),
                                        std::min(A_max, B_max, cmp)));
    }
    static constexpr auto colinear_segments_overlap(
        const cartesian_segment auto & A, const cartesian_segment auto & B) {
        const auto lA = segment_to_line(A);
        const auto lB = segment_to_line(B);

        return (std::get<2>(lA) * std::get<1>(lB) !=
                std::get<2>(lB) * std::get<1>(lA))
                   ? std::nullopt
                   : aligned_segments_overlap(A, B);
    }
    static constexpr auto segments_overlap(const cartesian_segment auto & A,
                                           const cartesian_segment auto & B) {
        const auto lA = segment_to_line(A);
        const auto lB = segment_to_line(B);

        return (std::get<0>(lA) * std::get<1>(lB) !=
                    std::get<0>(lB) * std::get<1>(lA) ||
                std::get<2>(lA) * std::get<1>(lB) !=
                    std::get<2>(lB) * std::get<1>(lA))
                   ? std::nullopt
                   : aligned_segments_overlap(A, B);
    }
};
}  // namespace melon
}  // namespace fhamonic

#endif  // GEOMETRIC_SYSTEMS_HPP