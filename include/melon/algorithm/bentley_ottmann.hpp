#ifndef MELON_ALGORITHM_BENTLEY_OTTMANN_HPP
#define MELON_ALGORITHM_BENTLEY_OTTMANN_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <functional>
#include <map>
#include <ranges>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/geometry.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
template <typename _Traits>
concept bentley_ottmann_traits = requires() {
    { _Traits::report_endpoints } -> std::convertible_to<bool>;
};
// clang-format on

template <typename _Segment>
struct default_bentley_ottmann_traits {
    using coordinate_system = cartesian;
    using segment_type = _Segment;
    using line_type = decltype(coordinate_system::segment_to_line(
        std::declval<segment_type>()));
    using intersection_type = decltype(coordinate_system::segments_intersection(
        std::declval<segment_type>(),
        std::declval<segment_type>()))::value_type;

    template <typename T, typename CMP>
    using segments_tree = std::set<T, CMP>;

    template <typename T>
    using events_tree =
        std::map<intersection_type, T,
                 typename coordinate_system::point_xy_comparator>;

    static constexpr bool report_endpoints = true;
};

template <bentley_ottmann_traits _Traits, typename _SegmentId,
          input_mapping<_SegmentId> _SegmentMap = views::identity_map>
class bentley_ottmann : public algorithm_view_interface<
                            bentley_ottmann<_Traits, _SegmentId, _SegmentMap>> {
private:
    using segment_id_type = _SegmentId;
    using coordinate_system = typename _Traits::coordinate_system;
    using segment_type = typename _Traits::segment_type;
    using endpoint_type =
        std::common_type_t<decltype(std::get<0>(std::declval<segment_type>()),
                                    std::get<1>(std::declval<segment_type>()))>;
    using line_type = typename _Traits::line_type;
    using intersection_type = typename _Traits::intersection_type;
    static constexpr auto compute_sweepline_intersection(
        const intersection_type & event_point, const line_type & line) {
        return std::make_tuple(
            std::get<0>(event_point),
            (std::get<2>(line) - std::get<0>(line) * std::get<0>(event_point)) /
                std::get<1>(line));
    }
    using sweepline_intersection_type =
        std::decay_t<decltype(compute_sweepline_intersection(
            std::declval<intersection_type>(), std::declval<line_type>()))>;
    using sweepline_intersection_y_type = std::decay_t<decltype(std::get<1>(
        std::declval<sweepline_intersection_type>()))>;

    using event_cmp = coordinate_system::point_xy_comparator;

    struct segment_entry {
        mutable sweepline_intersection_type sweepline_intersection;
        const line_type line;
        const segment_type segment;
        const segment_id_type segment_id;

        segment_entry(const segment_id_type & si, const segment_type & s,
                      const intersection_type & p)
            : sweepline_intersection(p)
            , line(coordinate_system::segment_to_line(s))
            , segment(s)
            , segment_id(si) {}

        segment_entry(const segment_entry &) = default;
        segment_entry(segment_entry &&) = default;

        segment_entry & operator=(const segment_entry &) = default;
        segment_entry & operator=(segment_entry &&) = default;

        [[nodiscard]] constexpr const sweepline_intersection_y_type
        sweepline_y_intersection(const intersection_type & event_point) const {
            if(std::get<1>(line) == 0) return std::get<1>(event_point);
            if(std::get<0>(sweepline_intersection) == std::get<0>(event_point))
                return std::get<1>(sweepline_intersection);

            sweepline_intersection =
                compute_sweepline_intersection(event_point, line);
            return std::get<1>(sweepline_intersection);
        }
    };
    struct segment_cmp {
        using is_transparent = void;
        std::reference_wrapper<const intersection_type> event_point;

        [[nodiscard]] constexpr bool operator()(
            const segment_entry & e1, const segment_entry & e2) const {
            const auto & y1 = e1.sweepline_y_intersection(event_point);
            const auto & y2 = e2.sweepline_y_intersection(event_point);
            if(y1 == y2) {
                const auto m1 = coordinate_system::line_slope(e1.line);
                const auto m2 = coordinate_system::line_slope(e2.line);
                if(m1 == m2) return e1.segment_id < e2.segment_id;
                return (y1 > std::get<1>(event_point.get())) != (m1 < m2);
            }
            return y1 < y2;
        }
        [[nodiscard]] constexpr bool operator()(const intersection_type & p,
                                                const segment_entry & e) const {
            return std::get<1>(p) < e.sweepline_y_intersection(p);
        }
        [[nodiscard]] constexpr bool operator()(
            const segment_entry & e, const intersection_type & p) const {
            return e.sweepline_y_intersection(p) < std::get<1>(p);
        }
    };

    using segments_tree =
        typename _Traits::segments_tree<segment_entry, segment_cmp>;
    enum event_type { starting, ending, coincident };
    using events = std::vector<std::pair<segment_id_type, event_type>>;
    using events_tree = typename _Traits::events_tree<events>;

private:
    [[no_unique_address]] _SegmentMap _segment_map;
    [[no_unique_address]] event_cmp _event_cmp;
    segments_tree _segments_tree;
    segments_tree _tmp_tree;
    events_tree _events_tree;

    intersection_type _current_event_point;
    intersection_type _tmp_event_point;

    std::vector<segment_id_type> _intersections;

public:
    template <std::ranges::range _SegmentIdRange,
              typename _SM = views::identity_map>
    bentley_ottmann(_SegmentIdRange && segments_ids_range,
                    _SM && segment_map = {}) noexcept
        : _segment_map(views::mapping_all(std::forward<_SM>(segment_map)))
        , _segments_tree(segment_cmp(std::cref(_current_event_point)))
        , _tmp_tree(segment_cmp(std::cref(_tmp_event_point))) {
        for(auto && s : segments_ids_range) {
            const auto & [p1, p2] = segment_map[s];
            if(_event_cmp(p1, p2)) {
                push_segment_endpoint(p1, s, event_type::starting);
                push_segment_endpoint(p2, s, event_type::ending);
                continue;
            }
            if(_event_cmp(p2, p1)) {
                push_segment_endpoint(p2, s, event_type::starting);
                push_segment_endpoint(p1, s, event_type::ending);
                continue;
            }
            push_segment_endpoint(p1, s, event_type::coincident);
        }
        init();
    }

    template <typename... _Args>
    [[nodiscard]] constexpr bentley_ottmann(_Traits, _Args &&... args)
        : bentley_ottmann(std::forward<_Args>(args)...) {}

    [[nodiscard]] constexpr bentley_ottmann(const bentley_ottmann &) = default;
    [[nodiscard]] constexpr bentley_ottmann(bentley_ottmann &&) = default;

    constexpr bentley_ottmann & operator=(const bentley_ottmann &) = default;
    constexpr bentley_ottmann & operator=(bentley_ottmann &&) = default;

    constexpr bentley_ottmann & reset() noexcept {
        _events_tree.clear();
        _segments_tree.clear();
        return *this;
    }

private:
    void push_segment_endpoint(const endpoint_type & i,
                               const segment_id_type & s, const event_type et) {
        // auto && [it, inserted] = _events_tree.try_emplace(i);
        auto && [it, inserted] = _events_tree.try_emplace(intersection_type(i));
        it->second.emplace_back(s, et);
    }
    void push_intersection(const intersection_type & i) {
        _events_tree.try_emplace(i);
    }
    void detect_intersection(const segment_entry & e1,
                             const segment_entry & e2) noexcept {
        const auto & [a, b] = e1.segment;
        const auto & [c, d] = e2.segment;

        const auto dx_ab = std::get<0>(b) - std::get<0>(a);
        const auto dy_ab = std::get<1>(b) - std::get<1>(a);
        const auto dx_ac = std::get<0>(c) - std::get<0>(a);
        const auto dy_ac = std::get<1>(c) - std::get<1>(a);
        const auto dx_ad = std::get<0>(d) - std::get<0>(a);
        const auto dy_ad = std::get<1>(d) - std::get<1>(a);

        const auto dx_bc = std::get<0>(c) - std::get<0>(b);
        const auto dy_bc = std::get<1>(c) - std::get<1>(b);
        const auto dx_dc = std::get<0>(c) - std::get<0>(d);
        const auto dy_dc = std::get<1>(c) - std::get<1>(d);

        if((dx_ab * dy_ac - dx_ac * dy_ab < 0) !=
               (dx_ab * dy_ad - dx_ad * dy_ab > 0) ||
           (dx_dc * dy_ac - dx_ac * dy_dc < 0) !=
               (dx_dc * dy_bc - dx_bc * dy_dc > 0))
            return;

        const auto & i_opt =
            coordinate_system::lines_intersection(e1.line, e2.line);
        if(!i_opt.has_value()) [[unlikely]]
            return;
        const auto & i = i_opt.value();

        if(_event_cmp(i, _current_event_point)) return;

        push_intersection(i);
    }
    void handle_event(const std::pair<intersection_type, events> & e) noexcept {
        const auto & [i, evts] = e;
        _tmp_event_point = i;

        _intersections.resize(0);
        auto after_last_removed_it = _segments_tree.lower_bound(i);
        while(after_last_removed_it != _segments_tree.end() &&
              after_last_removed_it->sweepline_y_intersection(i) ==
                  std::get<1>(i)) {
            _intersections.emplace_back(after_last_removed_it->segment_id);

            if constexpr(requires {
                             _segments_tree.extract_and_get_next(
                                 after_last_removed_it);
                         }) {
                auto && [node, next] =
                    _segments_tree.extract_and_get_next(after_last_removed_it);
                _tmp_tree.insert(std::move(node));
                after_last_removed_it = next;
            } else {
                const auto next_it = std::next(after_last_removed_it);
                _tmp_tree.insert(_tmp_tree.begin(),
                                 _segments_tree.extract(after_last_removed_it));
                after_last_removed_it = std::move(next_it);
            }
        }

        _current_event_point = i;

        for(const auto & [s, et] : evts) {
            if(et == event_type::ending) {
                _tmp_tree.erase(
                    _tmp_tree.find(segment_entry(s, _segment_map[s], i)));
                continue;
            }
            if constexpr(_Traits::report_endpoints) {
                _intersections.emplace_back(s);
            }
            if(et != event_type::starting) continue;
            _tmp_tree.emplace(
                segment_entry(s, _segment_map[s], _current_event_point));
        }

        if(_tmp_tree.empty()) {
            if(after_last_removed_it != _segments_tree.end() &&
               after_last_removed_it != _segments_tree.begin()) {
                detect_intersection(*std::prev(after_last_removed_it),
                                    *after_last_removed_it);
            }
            return;
        }
        auto last_added_it = _segments_tree.insert(
            after_last_removed_it,
            _tmp_tree.extract(std::prev(_tmp_tree.end())));
        if(auto next_it = std::next(last_added_it);
           next_it != _segments_tree.end())
            detect_intersection(*last_added_it, *next_it);

        while(!_tmp_tree.empty()) {
            last_added_it = _segments_tree.insert(
                last_added_it, _tmp_tree.extract(std::prev(_tmp_tree.end())));
        }

        if(last_added_it != _segments_tree.begin())
            detect_intersection(*std::prev(last_added_it), *last_added_it);
    }
    void init() {
        if(_events_tree.empty()) return;
        handle_event(*_events_tree.begin());
        if(_intersections.size() < 2) advance();
    }

public:
    [[nodiscard]] constexpr bool finished() const noexcept {
        return _events_tree.empty();
    }

    [[nodiscard]] constexpr auto current() const noexcept {
        assert(!finished());
        return std::make_pair(_events_tree.begin()->first,
                              std::views::all(_intersections));
    }

    constexpr void advance() noexcept {
        assert(!finished());
        do {
            _events_tree.erase(_events_tree.begin());
            if(finished()) return;
            handle_event(*_events_tree.begin());
        } while(_intersections.size() < 2);
    }
};

template <typename _SegmentIdRange>
bentley_ottmann(_SegmentIdRange &&) -> bentley_ottmann<
    default_bentley_ottmann_traits<std::ranges::range_value_t<_SegmentIdRange>>,
    std::ranges::range_value_t<_SegmentIdRange>, views::identity_map>;

template <typename _SegmentIdRange, typename _SegmentMap>
bentley_ottmann(_SegmentIdRange &&, _SegmentMap &&) -> bentley_ottmann<
    default_bentley_ottmann_traits<mapped_value_t<
        _SegmentMap, std::ranges::range_value_t<_SegmentIdRange>>>,
    std::ranges::range_value_t<_SegmentIdRange>,
    views::mapping_all_t<_SegmentMap>>;

template <typename _SegmentIdRange, typename _Traits>
bentley_ottmann(_Traits, _SegmentIdRange &&)
    -> bentley_ottmann<_Traits, std::ranges::range_value_t<_SegmentIdRange>,
                       views::identity_map>;

template <typename _SegmentIdRange, typename _SegmentMap, typename _Traits>
bentley_ottmann(_Traits, _SegmentIdRange &&, _SegmentMap &&)
    -> bentley_ottmann<_Traits, std::ranges::range_value_t<_SegmentIdRange>,
                       views::mapping_all_t<_SegmentMap>>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_BENTLEY_OTTMANN_HPP
