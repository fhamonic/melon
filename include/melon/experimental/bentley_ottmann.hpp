#ifndef MELON_ALGORITHM_BENTLEY_OTTMANN_HPP
#define MELON_ALGORITHM_BENTLEY_OTTMANN_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <ranges>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include <range/v3/view/concat.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include "melon/container/d_ary_heap.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace fhamonic {
namespace melon {

template <typename Num, typename Denom = Num>
struct rational {
    Num num;
    Denom denom;

    constexpr rational(Num n = Num{0}, Denom d = Denom{1}) : num(n), denom(d) {
        normalize();
    }

    void normalize() {
        auto g = std::gcd(num, denom);
        num /= g;
        denom /= g;
        if constexpr(std::numeric_limits<Num>::is_signed) {
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

    template <typename ONum, typename ODenom>
    std::strong_ordering operator<=>(
        const rational<ONum, ODenom> & other) const {
        return (num * other.denom) <=> (other.num * denom);
    }

    template <typename O>
    std::strong_ordering operator<=>(const O & other) const {
        return num <=> (other * denom);
    }
};
}  // namespace melon
}  // namespace fhamonic

namespace std {
template <typename T>
struct hash<fhamonic::melon::rational<T>> {
    std::size_t operator()(const fhamonic::melon::rational<T> & r) const {
        std::size_t h1 = std::hash<T>()(r.num);
        std::size_t h2 = std::hash<T>()(r.denom);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};
template <typename T>
struct hash<
    std::pair<fhamonic::melon::rational<T>, fhamonic::melon::rational<T>>> {
    std::size_t operator()(
        const std::pair<fhamonic::melon::rational<T>,
                        fhamonic::melon::rational<T>> & p) const {
        std::size_t h1 = std::hash<fhamonic::melon::rational<T>>()(p.first);
        std::size_t h2 = std::hash<fhamonic::melon::rational<T>>()(p.second);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};
}  // namespace std

namespace fhamonic {
namespace melon {

namespace cartesian {
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

static constexpr auto segment_to_line(const cartesian_segment auto & s) {
    const auto a = std::get<1>(std::get<1>(s)) - std::get<1>(std::get<0>(s));
    const auto b = std::get<0>(std::get<0>(s)) - std::get<0>(std::get<1>(s));
    const auto c =
        a * std::get<0>(std::get<0>(s)) + b * std::get<1>(std::get<0>(s));
    return std::tuple(a, b, c);
}
static constexpr auto line_intersection(const cartesian_line auto & A,
                                        const cartesian_line auto & B) {
    const auto determinant =
        std::get<0>(A) * std::get<1>(B) - std::get<1>(A) * std::get<0>(B);
    return determinant == 0 ? std::nullopt
                            : std::make_optional(std::make_pair(
                                  rational(std::get<2>(A) * std::get<1>(B) -
                                               std::get<2>(B) * std::get<1>(A),
                                           determinant),
                                  rational(std::get<0>(A) * std::get<2>(B) -
                                               std::get<0>(B) * std::get<2>(A),
                                           determinant)));
}
static constexpr auto segment_intersection(const cartesian_segment auto & A,
                                           const cartesian_segment auto & B) {
    const auto intersection_opt =
        line_intersection(segment_to_line(A), segment_to_line(B));

    if(!intersection_opt.has_value()) return intersection_opt;
    const auto & intersection = intersection_opt.value();

    const auto & [Ax_min, Ax_max] =
        std::minmax(std::get<0>(std::get<0>(A)), std::get<0>(std::get<1>(A)));
    const auto & [Bx_min, Bx_max] =
        std::minmax(std::get<0>(std::get<0>(B)), std::get<0>(std::get<1>(B)));
    const auto & [Ay_min, Ay_max] =
        std::minmax(std::get<1>(std::get<0>(A)), std::get<1>(std::get<1>(A)));
    const auto & [By_min, By_max] =
        std::minmax(std::get<1>(std::get<0>(B)), std::get<1>(std::get<1>(B)));

    return (std::max(Ax_min, Bx_min) > std::get<0>(intersection) ||
            std::min(Ax_max, Bx_max) < std::get<0>(intersection) ||
            std::max(Ay_min, By_min) > std::get<1>(intersection) ||
            std::min(Ay_max, By_max) < std::get<1>(intersection))
               ? std::nullopt
               : intersection_opt;
}
}  // namespace cartesian

template <std::ranges::range _SegmentIdRange,
          input_mapping<std::ranges::range_value_t<_SegmentIdRange>>
              _SegmentMap = views::identity_map>
class bentley_ottmann {
private:
    using segment_id = std::ranges::range_value_t<_SegmentIdRange>;
    // using segment = mapped_value_t<_SegmentMap, segment_id>;
    using point = std::pair<int8_t, int8_t>;
    using segment = std::pair<point, point>;
    using line = std::tuple<int8_t, int8_t, int16_t>;  // a*x + b*y = c
    using sweepline = rational<int32_t>;
    using intersection = std::pair<rational<int32_t>, rational<int32_t>>;

    static line segment_to_line(const segment & s) {
        const int8_t a =
            std::get<1>(std::get<1>(s)) - std::get<1>(std::get<0>(s));
        const int8_t b =
            std::get<0>(std::get<0>(s)) - std::get<0>(std::get<1>(s));
        const int16_t c =
            static_cast<int16_t>(a) *
                static_cast<int16_t>(std::get<0>(std::get<0>(s))) +
            static_cast<int16_t>(b) *
                static_cast<int16_t>(std::get<1>(std::get<0>(s)));
        return line(a, b, c);
    }

    static std::optional<intersection> line_intersection(const line & A,
                                                         const line & B) {
        const int16_t determinant =
            static_cast<int16_t>(std::get<0>(A) * std::get<1>(B)) -
            static_cast<int16_t>(std::get<1>(A) * std::get<0>(B));
        if(determinant == 0) return std::nullopt;
        return intersection(
            rational<int32_t>(static_cast<int32_t>(std::get<2>(A)) *
                                      static_cast<int32_t>(std::get<1>(B)) -
                                  static_cast<int32_t>(std::get<2>(B)) *
                                      static_cast<int32_t>(std::get<1>(A)),
                              determinant),
            rational<int32_t>(static_cast<int32_t>(std::get<0>(A)) *
                                      static_cast<int32_t>(std::get<2>(B)) -
                                  static_cast<int32_t>(std::get<0>(B)) *
                                      static_cast<int32_t>(std::get<2>(A)),
                              determinant));
    }

    struct event_cmp {
        [[nodiscard]] constexpr bool operator()(
            const auto & p1, const auto & p2) const noexcept {
            if(std::get<0>(p1) == std::get<0>(p2))
                return std::get<1>(p1) < std::get<1>(p2);
            return std::get<0>(p1) < std::get<0>(p2);
        }
    };
    using event_heap = d_ary_heap<2, intersection, event_cmp>;

    struct segment_cmp {
        std::reference_wrapper<const intersection> last_event_point;

        [[nodiscard]] constexpr rational<int64_t> sweepline_intersection_x(
            const line & l) const {
            const auto & sweepline_x = std::get<0>(last_event_point.get());
            return rational<int64_t>(
                static_cast<int64_t>(std::get<2>(l)) *
                        static_cast<int64_t>(sweepline_x.denom) -
                    static_cast<int64_t>(std::get<0>(l)) *
                        static_cast<int64_t>(sweepline_x.num),
                static_cast<int64_t>(std::get<1>(l)) *
                    static_cast<int64_t>(sweepline_x.denom));
        }
        [[nodiscard]] constexpr bool operator()(
            const std::pair<segment_id, line> & e1,
            const std::pair<segment_id, line> & e2) const {
            const line & l1 = e1.second;
            const line & l2 = e2.second;
            const auto y1 = sweepline_intersection_x(l1);
            const auto y2 = sweepline_intersection_x(l2);
            // fmt::print("y1 = {}/{} ; y2 = {}/{}\n", y1.num, y1.denom, y2.num,
            //            y2.denom);
            if(y1 == y2) {
                const auto m1 = rational(std::get<0>(l1), std::get<1>(l1));
                const auto m2 = rational(std::get<0>(l2), std::get<1>(l2));
                const auto & sweepline_y = std::get<1>(last_event_point.get());
                if(m1 == m2) return e1.first < e2.first;
                return (y1 <= sweepline_y) == (m1 > m2);
            }
            return y1 < y2;
        }
    };
    using segment_tree = std::set<std::pair<segment_id, line>, segment_cmp>;

    enum event_type { starting, ending, interior };
    using events = std::map<segment_id, event_type>;
    using events_map = std::unordered_map<intersection, events>;

private:
    _SegmentIdRange _segments_ids_range;
    _SegmentMap _segment_map;

    event_cmp _event_cmp;
    event_heap _event_heap;
    intersection _current_event_point;
    segment_cmp _segment_cmp;
    segment_tree _segment_tree;

    events_map _events_map;
    events_map::const_iterator _current_event_it;

public:
    template <typename _SIR, typename _SM = views::identity_map>
    bentley_ottmann(_SIR && segments_ids_range,
                    _SM && segment_map = {}) noexcept
        : _segments_ids_range(
              std::ranges::views::all(std::forward<_SIR>(segments_ids_range)))
        , _segment_map(views::mapping_all(std::forward<_SM>(segment_map)))
        , _segment_cmp(std::cref(_current_event_point))
        , _segment_tree(_segment_cmp) {
        for(auto && s : segments_ids_range) {
            auto [p1, p2] = segment_map[s];
            if(_event_cmp(p2, p1)) std::swap(p1, p2);
            push_event(p1, s, event_type::starting);
            push_event(p2, s, event_type::ending);
        }
        // init();
    }

    [[nodiscard]] constexpr bentley_ottmann(const bentley_ottmann &) = default;
    [[nodiscard]] constexpr bentley_ottmann(bentley_ottmann &&) = default;

    constexpr bentley_ottmann & operator=(const bentley_ottmann &) = default;
    constexpr bentley_ottmann & operator=(bentley_ottmann &&) = default;

    constexpr bentley_ottmann & reset() noexcept {
        _event_heap.clear();
        _segment_tree.clear();
        return *this;
    }

private:
    void push_event(const intersection & i, const segment_id & s,
                    const event_type & et) {
        auto && [it, inserted] = _events_map.try_emplace(i);
        if(inserted) _event_heap.push(i);
        it->second.try_emplace(s, et);
        // auto && [evt_it, evt_inserted] = it->second.try_emplace(s, et);
        // fmt::print("({}/{}, {}/{}) {} {} ({})\n", std::get<0>(i).num,
        //            std::get<0>(i).denom, std::get<1>(i).num,
        //            std::get<1>(i).denom, s,
        //            (et == event_type::starting
        //                 ? "starting"
        //                 : (et == event_type::ending ? "ending" :
        //                 "interior")),
        //            evt_inserted ? "inserted" : "not_inserted");
    }
    void detect_intersection(const std::pair<segment_id, line> & s1,
                             const std::pair<segment_id, line> & s2) noexcept {
        const auto & [a, b] = _segment_map[s1.first];
        const auto & [c, d] = _segment_map[s2.first];
        // xs overlaps because segments are in tree
        // so test if ys overlaps
        const auto & [y1_min, y1_max] =
            std::minmax(std::get<1>(a), std::get<1>(b));
        const auto & [y2_min, y2_max] =
            std::minmax(std::get<1>(c), std::get<1>(d));

        if(y1_min > y2_max || y2_min > y1_max) return;
        const auto & i_opt = line_intersection(s1.second, s2.second);
        if(!i_opt.has_value()) return;
        const auto & i = i_opt.value();

        if(std::max(y1_min, y2_min) > std::get<1>(i) ||
           std::min(y1_max, y2_max) < std::get<1>(i))
            return;

        const auto & x1_max = std::max(std::get<0>(a), std::get<0>(b));
        const auto & x2_max = std::max(std::get<0>(c), std::get<0>(d));

        if(std::get<0>(i) < std::get<0>(_current_event_point) ||
           std::get<0>(i) > std::min(x1_max, x2_max))
            return;

        push_event(i, s1.first, event_type::interior);
        push_event(i, s2.first, event_type::interior);
    }

    void handle_event(const std::pair<intersection, events> & e) noexcept {
        const auto & [i, evts] = e;
        auto not_starting_segments = std::views::filter(
            evts,
            [](const auto & p) { return p.second != event_type::starting; });
        auto after_last_removed_it = _segment_tree.end();
        if(not_starting_segments.begin() != not_starting_segments.end()) {
            for(const auto & [s, et] :
                std::views::drop(not_starting_segments, 1)) {
                _segment_tree.erase({s, segment_to_line(_segment_map[s])});
                // _segment_tree.erase(
                // _segment_tree.find({s, segment_to_line(_segment_map[s])}));
            }
            const auto & [s, et] = not_starting_segments.front();
            after_last_removed_it = _segment_tree.erase(
                _segment_tree.find({s, segment_to_line(_segment_map[s])}));
        }
        ////
        _current_event_point = i;

        auto not_ending_segments = std::views::filter(evts, [](const auto & p) {
            return p.second != event_type::ending;
        });
        if(std::ranges::distance(not_ending_segments) == 0) {
            if(after_last_removed_it != _segment_tree.end() &&
               after_last_removed_it != _segment_tree.begin()) {
                detect_intersection(*std::prev(after_last_removed_it),
                                    *after_last_removed_it);
            }
            return;
        }

        auto smallest_added_it = _segment_tree.end();
        auto greatest_added_it = _segment_tree.end();
        if(not_ending_segments.begin() != not_ending_segments.end()) {
            {
                const auto & [s, et] = not_ending_segments.front();
                const auto && [it, inserted] =
                    _segment_tree.emplace(s, segment_to_line(_segment_map[s]));
                smallest_added_it = greatest_added_it = it;
            }
            for(const auto & [s, et] :
                std::views::drop(not_ending_segments, 1)) {
                auto [it, inserted] =
                    _segment_tree.emplace(s, segment_to_line(_segment_map[s]));
                if(_segment_cmp(*it, *smallest_added_it))
                    smallest_added_it = it;
                if(_segment_cmp(*greatest_added_it, *it))
                    greatest_added_it = it;
            }
        }

        if(smallest_added_it != _segment_tree.begin())
            detect_intersection(*std::prev(smallest_added_it),
                                *smallest_added_it);
        if(auto next_it = std::next(greatest_added_it);
           next_it != _segment_tree.end())
            detect_intersection(*greatest_added_it, *next_it);
    }
    void init() {
        if(_event_heap.empty()) return;
        const intersection & i = _event_heap.top();
        _current_event_it = _events_map.find(i);
        handle_event(*_current_event_it);
        if(_current_event_it->second.size() < 2) advance();

        // const intersection & i = _event_heap.top();
        // const auto event_it = _events_map.find(i);
        // for(auto && [s, et] : event_it->second) {
        //     assert(et == event_type::starting);
        //     auto [it, inserted] =
        //         _segment_tree.emplace(s, segment_to_line(_segment_map[s]));
        //     if(it != _segment_tree.begin())
        //         detect_intersection(*std::prev(it), *it);
        //     if(auto next_it = std::next(it); next_it != _segment_tree.end())
        //         detect_intersection(*it, *next_it);
        // }
        // _event_heap.pop();
        // _events_map.erase(event_it);
    }

public:
    [[nodiscard]] constexpr bool finished() const noexcept {
        return _event_heap.empty();
    }

    [[nodiscard]] constexpr auto current() const noexcept {
        assert(!finished());
        return std::make_pair(_event_heap.top(),
                              std::views::keys(_current_event_it->second));
    }

    constexpr void advance() noexcept {
        assert(!finished());
        do {
            _event_heap.pop();
            _events_map.erase(_current_event_it);

            if(finished()) return;

            const intersection & i = _event_heap.top();
            _current_event_it = _events_map.find(i);
            handle_event(*_current_event_it);
        } while(_current_event_it->second.size() < 2);
    }

    constexpr void run() noexcept {
        while(!_event_heap.empty()) {
            const intersection & i = _event_heap.top();
            _current_event_it = _events_map.find(i);
            handle_event(*_current_event_it);

            fmt::print("({}/{}, {}/{})\n", std::get<0>(i).num,
                       std::get<0>(i).denom, std::get<1>(i).num,
                       std::get<1>(i).denom);
            for(auto && [s, et] : _current_event_it->second) {
                auto [p1, p2] = _segment_map[s];
                fmt::print(
                    "\t{} : [({}, {}), ({}, {})] ({})\n", s, p1.first,
                    p1.second, p2.first, p2.second,
                    (et == event_type::starting
                         ? "starting"
                         : (et == event_type::ending ? "ending" : "interior")));
            }
            fmt::print("segment_tree : {}\n",
                       fmt::join(std::views::keys(_segment_tree), ","));

            _event_heap.pop();
            _events_map.erase(_current_event_it);
        }
    }

    [[nodiscard]] constexpr auto begin() noexcept {
        init();
        return algorithm_iterator(*this);
    }
    [[nodiscard]] constexpr auto end() noexcept {
        return algorithm_end_sentinel();
    }
};

template <typename _SegmentIdRange>
bentley_ottmann(_SegmentIdRange &&)
    -> bentley_ottmann<std::ranges::views::all_t<_SegmentIdRange>,
                       views::identity_map>;

template <typename _SegmentIdRange, typename _SegmentMap>
bentley_ottmann(_SegmentIdRange &&, _SegmentMap &&)
    -> bentley_ottmann<std::ranges::views::all_t<_SegmentIdRange>,
                       views::mapping_all_t<_SegmentMap>>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_BENTLEY_OTTMANN_HPP
