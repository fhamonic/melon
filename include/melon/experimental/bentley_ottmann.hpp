#ifndef MELON_ALGORITHM_BENTLEY_OTTMANN_HPP
#define MELON_ALGORITHM_BENTLEY_OTTMANN_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/d_ary_heap.hpp"

namespace fhamonic {
namespace melon {

template <
    std::ranges::range _SegmentIdRange,
    input_mapping<std::ranges::range_value_t<_SegmentIdRange>> _SegmentMap>
class bentley_ottman {
private:
    using segment_id = std::ranges::range_value_t<_SegmentIdRange>;
    using segment = mapped_value_t<_SegmentMap, segment_id>;
    // using point = mapped_value_t<EM, segment_id_t>;

    // struct event_cmp {
    //     [[nodiscard]] constexpr bool operator()(
    //         const coords_t & p1, const coords_t & p2) const noexcept {
    //         if(std::get<0>(e1.second) == std::get<0>(p2.second))
    //             return std::get<1>(p1.second) < std::get<1>(p2.second);
    //         return std::get<0>(p1.second) < std::get<0>(p2.second);
    //     }
    // };
    // using event_heap =
    //     d_ary_heap<2, coords_t, std::vector<arc_t<G>>, event_cmp>;

    // struct segment_cmp {
    //     std::reference_wrapper<sweepline> sweepline_x;

    //     [[nodiscard]] constexpr coord_t sweepline_intersection_y(
    //         const std::pair<coords_t, coords_t> & p) {
    //         const coords_t & A = std::get<0>(p);
    //         const coords_t & B = std::get<1>(p);
    //         const coord_t dx = std::get<0>(B) - std::get<0>(A);
    //         const coord_t dy = std::get<1>(B) - std::get<1>(A);
    //         return std::get<1>(A) +
    //                (sweepline_x.get() - std::get<0>(A)) * dy / dx;
    //     }
    //     [[nodiscard]] constexpr bool operator()(
    //         const std::pair<coords_t, coords_t> & p1,
    //         const std::pair<coords_t, coords_t> & p2) {
    //         return sweepline_intersection_y(p1) < sweepline_intersection_y(p2);
    //     }
    // };
    // using segment_tree = std::set<std::pair<coords_t, coords_t>, segment_cmp>;

private:
    _SegmentIdRange _segments_ids_range;
    _SegmentMap _segment_map;

    // int _sweepline;
    // event_heap _event_heap;
    // segment_tree _segment_tree;

public:
    template <typename _SIR, typename _SM>
    bentley_ottman(_SIR && segments_ids_range, _SM && value_map) noexcept
        : _segments_ids_range(
              std::ranges::views::all(std::forward<_SIR>(segments_ids_range)))
        , _value_map(views::mapping_all(std::forward<_SM>(value_map))) {}

    [[nodiscard]] constexpr bentley_ottman(const bentley_ottman &) =
        default;
    [[nodiscard]] constexpr bentley_ottman(bentley_ottman &&) = default;

    constexpr bentley_ottman & operator=(const bentley_ottman &) = default;
    constexpr bentley_ottman & operator=(bentley_ottman &&) = default;

    [[nodiscard]] constexpr std::optional<coords_t> get_intersection(
        const coords_t & A, const coords_t & B, const coords_t & C,
        const coords_t & D) {
        const coord_t a1 = std::get<1>(B) - std::get<1>(A);
        const coord_t a2 = std::get<1>(D) - std::get<1>(C);
        const coord_t b1 = std::get<0>(A) - std::get<0>(B);
        const coord_t b2 = std::get<0>(C) - std::get<0>(D);
        const coord_t c1 = a1 * std::get<0>(A) + b1 * std::get<1>(A);
        const coord_t c2 = a2 * std::get<0>(C) + b2 * std::get<1>(C);

        const coord_t determinant = a1 * b2 - a2 * b1;
        if(determinant == 0) return std::nullopt;

        const coord_t x = (b2 * c1 - b1 * c2) / determinant;
        if(x < std::max(std::get<0>(A), std::get<0>(B)) ||
           x > std::min(std::get<0>(C), std::get<0>(D)))
            return std::nullopt;

        const coord_t y = (a1 * c2 - a2 * c1) / determinant;
        return coords_t{x, y};
    }

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
        while(!finished()) advance();
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

template <typename _SegmentIdRange, typename _SegmentMap>
bentley_ottman(_SegmentIdRange &&, _SegmentMap &&)
    -> bentley_ottman<std::ranges::views::all_t<_SegmentIdRange>,
                      views::mapping_all_t<_SegmentMap>>;

#endif  // MELON_ALGORITHM_BENTLEY_OTTMANN_HPP
