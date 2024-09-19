#ifndef MELON_ALGORITHM_BENTLEY_OTTMANN_HPP
#define MELON_ALGORITHM_BENTLEY_OTTMANN_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <limits>
#include <optional>
#include <queue>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/d_ary_heap.hpp"

namespace fhamonic {
namespace melon {

template <typename T>
class rational {
    T num;
    T denom;

    constexpr rational(T n, T d = T{1}) : num(n), denom(d) { normalize(); }

    void normalize() {
        T g = std::gcd(num, denom);
        num /= g;
        denom /= g;
        if constexpr(std::numeric_limits<T>::is_signed()) {
            if(denom < 0) {
                num = -num;
                denom = -denom;
            }
        }
    }

    rational operator+(const rational & other) const {
        return rational(num * other.denom + other.num * denom,
                        denom * other.denom);
    }

    rational operator-(const rational & other) const {
        return rational(num * other.denom - other.num * denom,
                        denom * other.denom);
    }

    rational operator*(const rational & other) const {
        return rational(num * other.num, denom * other.denom);
    }

    rational operator/(const rational & other) const {
        return rational(num * other.denom, denom * other.num);
    }

    bool operator==(const rational & other) const {
        return num == other.num && denom == other.denom;
    }

    std::strong_ordering operator<=>(const rational & other) {
        return (num * other.denom) <=> (other.num * denom);
    }
};

template <
    std::ranges::range _SegmentIdRange,
    input_mapping<std::ranges::range_value_t<_SegmentIdRange>> _SegmentMap>
class bentley_ottman {
private:
    using segment_id = std::ranges::range_value_t<_SegmentIdRange>;
    // using segment = mapped_value_t<_SegmentMap, segment_id>;
    using point = std::pair<int16_t, int16_t> using segment =
        std::pair<point, point>;
    using line = std::tuple<int16_t, int16_t, int32_t>;  // a*x + b*y = c
    using sweepline = rational<int64_t>;
    using intersection = std::pair<rational<int64_t>, rational<int64_t>>;

    static line segment_to_line(const segment & s) {
        const int16_t a =
            std::get<1>(std::get<1>(s)) - std::get<1>(std::get<0>(s));
        const int16_t b =
            std::get<0>(std::get<0>(s)) - std::get<0>(std::get<1>(s));
        const int32_t c =
            a * std::get<0>(std::get<0>(s)) + b * std::get<1>(std::get<0>(s));
        return line(a, b, c);
    }

    static std::optinal<intersection> get_intersection(const line & A,
                                                       const line & B) {
        const int32_t determinant =
            static_cast<int32_t>(std::get<0>(A) * std::get<1>(B)) -
            static_cast<int32_t>(std::get<1>(A) * std::get<0>(B));

        if(determinant == 0) return std::null_opt;

        return intersection(
            rational<int64_t>(
                static_cast<int64_t>(std::get<2>(A) * std::get<1>(B)) -
                    static_cast<int64_t>(std::get<2>(B) * std::get<1>(A)),
                determinant),
            rational<int64_t>(
                static_cast<int64_t>(std::get<0>(A) * std::get<2>(B)) -
                static_cast<int64_t>((std::get<0>(B) * std::get<2>(A)),
                                     determinant)));
    }

    struct event_cmp {
        [[nodiscard]] constexpr bool operator()(
            const coords_t & p1, const coords_t & p2) const noexcept {
            if(std::get<0>(p1) == std::get<0>(p2))
                return std::get<1>(p1) < std::get<1>(p2);
            return std::get<0>(p1) < std::get<0>(p2);
        }
    };
    using event_heap =
        std::priority_queue<intersection, std::vector<point>, event_cmp>;

    using sweepline = int;
    struct segment_cmp {
        std::reference_wrapper<sweepline> sweepline_x;

        [[nodiscard]] constexpr rational<int128_t> sweepline_intersection_y(
            const std::pair<coords_t, coords_t> & p) {
            const coords_t & A = std::get<0>(p);
            const coords_t & B = std::get<1>(p);
            const coord_t dx = std::get<0>(B) - std::get<0>(A);
            const coord_t dy = std::get<1>(B) - std::get<1>(A);
            return std::get<1>(A) +
                   (sweepline_x.get() - std::get<0>(A)) * dy / dx;
        }
        [[nodiscard]] constexpr bool operator()(
            const std::pair<segment_id, line> & e,
            const std::pair<segment_id, line> & e) {
            return sweepline_intersection_y(p1) < sweepline_intersection_y(p2);
        }
    };
    using segment_tree = std::set<std::pair<segment_id, line>, segment_cmp>;

    struct event_info {
        std::vector<segment_id> starting_segments;
        std::vector<segment_id> intersecting_segments;
        std::vector<segment_id> ending_segments;
    };
    using event_info_map = std::unordered_map<point, event_info>;

private:
    _SegmentIdRange _segments_ids_range;
    _SegmentMap _segment_map;

    event_heap _event_heap;
    sweepline _sweepline;
    segment_tree _segment_tree;

    event_info_map _event_info_map;

public:
    template <typename _SIR, typename _SM>
    bentley_ottman(_SIR && segments_ids_range, _SM && segment_map) noexcept
        : _segments_ids_range(
              std::ranges::views::all(std::forward<_SIR>(segments_ids_range)))
        , _segment_map(views::mapping_all(std::forward<_SM>(segment_map))) {}

    [[nodiscard]] constexpr bentley_ottman(const bentley_ottman &) = default;
    [[nodiscard]] constexpr bentley_ottman(bentley_ottman &&) = default;

    constexpr bentley_ottman & operator=(const bentley_ottman &) = default;
    constexpr bentley_ottman & operator=(bentley_ottman &&) = default;

    constexpr bentley_ottman & reset() noexcept {
        _event_heap.clear();
        _segment_tree.clear();
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _event_heap.empty();
    }

    [[nodiscard]] constexpr traversal_entry current() const noexcept {
        assert(!finished());
        return _event_heap.top();
    }

    constexpr void advance() noexcept {
        assert(!finished());
        do {
            const auto [p, intersecting_arcs] = _even_heap.top();
            _even_heap.pop();
        } while(!finished() && _even_heap.top().second.size() < 2);
    }

    constexpr void init() noexcept {
        if(_even_heap.top().second.size() < 2) advance();
    }

    constexpr void run() noexcept {
        // while(!finished()) advance();
    }
    [[nodiscard]] constexpr auto begin() noexcept {
        init();
        return algorithm_iterator(*this);
    }
    [[nodiscard]] constexpr auto end() noexcept {
        return algorithm_end_sentinel();
    }
};

}  // namespace melon
}  // namespace fhamonic

// template <typename _SegmentIdRange, typename _SegmentMap>
// bentley_ottman(_SegmentIdRange &&, _SegmentMap &&)
//     -> bentley_ottman<std::ranges::views::all_t<_SegmentIdRange>,
//                       views::mapping_all_t<_SegmentMap>>;

#endif  // MELON_ALGORITHM_BENTLEY_OTTMANN_HPP
