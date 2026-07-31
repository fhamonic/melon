#pragma once

#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

#include "melon/numeric/rational.hpp"

namespace melon {
template <typename T>
concept cartesian_point = requires(const T & t) {
    { std::get<0>(t) };
    { std::get<1>(t) };
};
template <typename T>
concept cartesian_segment = requires(const T & t) {
    { std::get<0>(t) } -> cartesian_point;
    { std::get<1>(t) } -> cartesian_point;
};
template <typename T>
concept cartesian_line = requires(const T & t) {
    { std::get<0>(t) };
    { std::get<1>(t) };
    { std::get<2>(t) };
};

// Exact for integral and rational coordinates, but nothing here widens: the
// line coefficients are degree 2 in the input coordinates and the intersection
// numerators degree 3, so an integral coordinate type must have room for that
// or the results are silently wrong. Coordinates of a few thousand already
// exceed int32.
struct cartesian {
    struct point_xy_comparator {
        using is_transparent = void;
        // Conditional: the coordinates' == and < are user operators.
        [[nodiscard]] constexpr bool operator()(const cartesian_point auto & p1,
                                                const cartesian_point auto & p2)
            const noexcept(noexcept(std::get<0>(p1) == std::get<0>(p2)) &&
                           noexcept(std::get<1>(p1) < std::get<1>(p2)) &&
                           noexcept(std::get<0>(p1) < std::get<0>(p2))) {
            if(std::get<0>(p1) == std::get<0>(p2))
                return std::get<1>(p1) < std::get<1>(p2);
            return std::get<0>(p1) < std::get<0>(p2);
        }
    };
    [[nodiscard]] static constexpr auto segment_to_line(
        const cartesian_segment auto & s) {
        const auto a =
            std::get<1>(std::get<1>(s)) - std::get<1>(std::get<0>(s));
        const auto b =
            std::get<0>(std::get<0>(s)) - std::get<0>(std::get<1>(s));
        const auto c =
            a * std::get<0>(std::get<0>(s)) + b * std::get<1>(std::get<0>(s));
        return std::make_tuple(a, b, c);
    }
    [[nodiscard]] static constexpr auto intersecting_lines_intersection(
        const cartesian_line auto & A, const cartesian_line auto & B,
        const auto & determinant) {
        const auto x_num =
            std::get<2>(A) * std::get<1>(B) - std::get<2>(B) * std::get<1>(A);
        const auto y_num =
            std::get<0>(A) * std::get<2>(B) - std::get<0>(B) * std::get<2>(A);
        // Integral coordinates: `/` truncates, turning (1, 1/3) into (1, 0),
        // so divide through an exact rational instead -- callers get a
        // rational-coordinate point back. Rational and floating-point
        // coordinates keep the plain division, exact resp. deliberately
        // inexact.
        if constexpr(std::integral<
                         std::remove_cvref_t<decltype(determinant)>>) {
            return std::make_tuple(numeric::make_rational(x_num, determinant),
                                   numeric::make_rational(y_num, determinant));
        } else {
            return std::make_tuple(x_num / determinant, y_num / determinant);
        }
    }

    [[nodiscard]] static constexpr auto intersecting_lines_intersection(
        const cartesian_line auto & A, const cartesian_line auto & B) {
        return intersecting_lines_intersection(
            A, B,
            std::get<0>(A) * std::get<1>(B) - std::get<1>(A) * std::get<0>(B));
    }

    [[nodiscard]] static constexpr auto lines_intersection(
        const cartesian_line auto & A, const cartesian_line auto & B) {
        const auto determinant =
            std::get<0>(A) * std::get<1>(B) - std::get<1>(A) * std::get<0>(B);
        return determinant == 0
                   ? std::nullopt
                   : std::make_optional(
                         intersecting_lines_intersection(A, B, determinant));
    }
    [[nodiscard]] static constexpr auto line_slope(
        const cartesian_line auto & l) {
        // Integral coordinates: plain division would be UB on a vertical line
        // and would truncate every other slope. make_rational is exact and
        // collapses the zero denominator to its 1/0 infinity sentinel, which
        // orders above every finite slope.
        if constexpr(std::integral<
                         std::remove_cvref_t<decltype(std::get<0>(l))>>) {
            return numeric::make_rational(std::get<0>(l), -std::get<1>(l));
        } else {
            return std::get<0>(l) / -std::get<1>(l);
        }
    }
    [[nodiscard]] static constexpr bool point_on_line(
        const cartesian_point auto & p, const cartesian_line auto & l) {
        return std::get<0>(l) * std::get<0>(p) +
                   std::get<1>(l) * std::get<1>(p) ==
               std::get<2>(l);
    }
    [[nodiscard]] static constexpr auto point_on_segment(
        const cartesian_point auto & p, const cartesian_segment auto & s) {
        if(!point_on_line(p, segment_to_line(s))) return false;
        const point_xy_comparator cmp;
        const auto & [s_min, s_max] =
            std::minmax(std::get<0>(s), std::get<1>(s), cmp);
        return !cmp(p, s_min) && !cmp(s_max, p);
    }
    [[nodiscard]] static constexpr auto segments_intersection(
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
    [[nodiscard]] static constexpr auto aligned_segments_overlap(
        const cartesian_segment auto & A, const cartesian_segment auto & B) {
        const point_xy_comparator cmp;
        const auto & [A_min, A_max] =
            std::minmax(std::get<0>(A), std::get<1>(A), cmp);
        const auto & [B_min, B_max] =
            std::minmax(std::get<0>(B), std::get<1>(B), cmp);

        return (cmp(A_max, B_min) || cmp(B_max, A_min))
                   ? std::nullopt
                   : std::make_optional(
                         std::make_tuple(std::max(A_min, B_min, cmp),
                                         std::min(A_max, B_max, cmp)));
    }
    [[nodiscard]] static constexpr auto colinear_segments_overlap(
        const cartesian_segment auto & A, const cartesian_segment auto & B) {
        const auto lA = segment_to_line(A);
        const auto lB = segment_to_line(B);

        return (std::get<2>(lA) * std::get<1>(lB) !=
                std::get<2>(lB) * std::get<1>(lA))
                   ? std::nullopt
                   : aligned_segments_overlap(A, B);
    }
    [[nodiscard]] static constexpr auto segments_overlap(
        const cartesian_segment auto & A, const cartesian_segment auto & B) {
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
