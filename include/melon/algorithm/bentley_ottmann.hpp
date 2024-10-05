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
#include <range/v3/view/map.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include "melon/container/d_ary_heap.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/geometry.hpp"



namespace std {
template <typename T>
struct hash<fhamonic::melon::rational<T>> {
    std::size_t operator()(const fhamonic::melon::rational<T> & r) const {
        r.normalize();
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
template <std::ranges::range _SegmentIdRange,
          input_mapping<std::ranges::range_value_t<_SegmentIdRange>>
              _SegmentMap = views::identity_map>
class bentley_ottmann {
private:
    using segment_id_type = std::ranges::range_value_t<_SegmentIdRange>;
    using segment = mapped_value_t<_SegmentMap, segment_id_type>;
    using line_type =
        decltype(cartesian::segment_to_line(std::declval<segment>()));
    using intersection = decltype(cartesian::segments_intersection(
        std::declval<segment>(), std::declval<segment>()))::value_type;

    using event_cmp = cartesian::point_xy_comparator;
    using event_heap = d_ary_heap<2, intersection, event_cmp>;

    struct segment_entry {
        const segment_id_type segment_id;
        const line_type line;
        mutable intersection sweepline_intersection;

        segment_entry(const segment_id_type & s, const line_type & l,
                      const intersection & p)
            : segment_id(s), line(l), sweepline_intersection(p) {}

        segment_entry(const segment_entry &) = default;
        segment_entry(segment_entry &&) = default;

        segment_entry & operator=(const segment_entry &) = default;
        segment_entry & operator=(segment_entry &&) = default;

        constexpr void compute_sweepline_intersection(
            const intersection & event_point) const {
            std::get<0>(sweepline_intersection) = std::get<0>(event_point);
            std::get<1>(sweepline_intersection) =
                rational(std::get<2>(line) * std::get<0>(event_point).denom -
                             std::get<0>(line) * std::get<0>(event_point).num,
                         std::get<1>(line) * std::get<0>(event_point).denom);
        }

        [[nodiscard]] constexpr const auto & sweepline_y_intersection(
            const intersection & event_point) const {
            if(std::get<1>(line) == 0) return std::get<1>(event_point);
            if(std::get<0>(sweepline_intersection) == std::get<0>(event_point))
                return std::get<1>(sweepline_intersection);

            compute_sweepline_intersection(event_point);
            return std::get<1>(sweepline_intersection);
        }
    };
    struct segment_cmp {
        using is_transparent = void;
        std::reference_wrapper<const intersection> event_point;

        [[nodiscard]] constexpr bool operator()(
            const segment_entry & e1, const segment_entry & e2) const {
            const auto & y1 = e1.sweepline_y_intersection(event_point);
            const auto & y2 = e2.sweepline_y_intersection(event_point);
            if(y1 == y2) {
                const auto m1 = cartesian::line_slope(e1.line);
                const auto m2 = cartesian::line_slope(e2.line);
                if(m1 == m2) return e1.segment_id < e2.segment_id;
                return (y1 <= std::get<1>(event_point.get())) == (m1 < m2);
            }
            return y1 < y2;
        }
        [[nodiscard]] constexpr bool operator()(const intersection & p,
                                                const segment_entry & e) const {
            return std::get<1>(p) < e.sweepline_y_intersection(p);
        }
        [[nodiscard]] constexpr bool operator()(const segment_entry & e,
                                                const intersection & p) const {
            return e.sweepline_y_intersection(p) < std::get<1>(p);
        }
    };

    using segment_tree = std::set<segment_entry, segment_cmp>;

    enum event_type { starting, ending };
    using events = std::vector<std::pair<segment_id_type, event_type>>;
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

    std::vector<segment_id_type> _intersections;

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
            push_segment_start(p1, s);
            push_segment_end(p2, s);
        }
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
    void push_segment_start(const intersection & i, const segment_id_type & s) {
        auto && [it, inserted] = _events_map.try_emplace(i);
        if(inserted) {
            _event_heap.push(i);
            // fmt::print("pushed start ({}/{}, {}/{})\n", std::get<0>(i).num,
            //            std::get<0>(i).denom, std::get<1>(i).num,
            //            std::get<1>(i).denom);
        }
        it->second.emplace_back(s, event_type::starting);
    }
    void push_segment_end(const intersection & i, const segment_id_type & s) {
        auto && [it, inserted] = _events_map.try_emplace(i);
        if(inserted) {
            _event_heap.push(i);
            // fmt::print("pushed end ({}/{}, {}/{})\n", std::get<0>(i).num,
            //            std::get<0>(i).denom, std::get<1>(i).num,
            //            std::get<1>(i).denom);
        }
        it->second.emplace_back(s, event_type::ending);
    }
    void push_intersection(const intersection & i) {
        auto && [it, inserted] = _events_map.try_emplace(i);
        if(inserted) {
            _event_heap.push(i);
            // fmt::print("pushed intersection ({}/{}, {}/{})\n",
            //            std::get<0>(i).num, std::get<0>(i).denom,
            //            std::get<1>(i).num, std::get<1>(i).denom);
        }
    }
    void detect_intersection(const segment_entry & e1,
                             const segment_entry & e2) noexcept {
        const auto & [a, b] = _segment_map[e1.segment_id];
        const auto & [c, d] = _segment_map[e2.segment_id];
        // xs overlaps because segments are in tree
        // so test if ys overlaps
        const auto & [y1_min, y1_max] =
            std::minmax(std::get<1>(a), std::get<1>(b));
        const auto & [y2_min, y2_max] =
            std::minmax(std::get<1>(c), std::get<1>(d));

        if(y1_min > y2_max || y2_min > y1_max) return;
        const auto & i_opt = cartesian::lines_intersection(e1.line, e2.line);
        if(!i_opt.has_value()) return;
        const auto & i = i_opt.value();

        if(std::max(y1_min, y2_min) > std::get<1>(i) ||
           std::min(y1_max, y2_max) < std::get<1>(i))
            return;

        const auto & x1_max = std::max(std::get<0>(a), std::get<0>(b));
        const auto & x2_max = std::max(std::get<0>(c), std::get<0>(d));

        if(_event_cmp(i, _current_event_point) ||
           std::get<0>(i) > std::min(x1_max, x2_max))
            return;

        push_intersection(i);
    }

    bool well_formed_tree() const {
        auto it = _segment_tree.begin();
        if(it == _segment_tree.end()) return true;
        auto next = std::next(it);
        while(next != _segment_tree.end()) {
            if(!_segment_tree.key_comp()(*it, *next)) return false;
            it = next;
            next = std::next(next);
        }
        return true;
    }

    void handle_event(const std::pair<intersection, events> & e) noexcept {
        const auto & [i, evts] = e;
        segment_tree tmp_tree(segment_cmp{i});

        auto after_last_removed_it = _segment_tree.end();
        for(const auto & [s, et] : evts) {
            if(et != event_type::ending) continue;
            after_last_removed_it =
                _segment_tree.erase(_segment_tree.find(segment_entry(
                    s, cartesian::segment_to_line(_segment_map[s]), i)));
        }
        _intersections.resize(0);
        {
            auto tree_it = _segment_tree.lower_bound(i);
            while(tree_it != _segment_tree.end() &&
                  tree_it->sweepline_y_intersection(i) == std::get<1>(i)) {
                auto next_it = std::next(tree_it);
                _intersections.emplace_back(tree_it->segment_id);
                tmp_tree.insert(tmp_tree.begin(),
                                _segment_tree.extract(tree_it));
                tree_it = std::move(next_it);
            }
            for(const auto & [s, et] : evts) {
                _intersections.emplace_back(s);
            }
        }
        _current_event_point = i;

        for(const auto & [s, et] : evts) {
            if(et != event_type::starting) continue;
            tmp_tree.emplace(
                segment_entry(s, cartesian::segment_to_line(_segment_map[s]),
                              _current_event_point));
        }
        if(tmp_tree.empty()) {
            if(after_last_removed_it != _segment_tree.end() &&
               after_last_removed_it != _segment_tree.begin()) {
                detect_intersection(*std::prev(after_last_removed_it),
                                    *after_last_removed_it);
            }
            return;
        }
        auto smallest_added_it = _segment_tree.end();
        auto greatest_added_it = _segment_tree.end();
        {
            const auto && [it, inserted, node] = _segment_tree.insert(
                tmp_tree.extract(std::prev(tmp_tree.end())));
            smallest_added_it = greatest_added_it = it;
        }
        while(!tmp_tree.empty()) {
            smallest_added_it = _segment_tree.insert(
                smallest_added_it, tmp_tree.extract(std::prev(tmp_tree.end())));
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
        if(_intersections.size() < 2) advance();
    }

public:
    [[nodiscard]] constexpr bool finished() const noexcept {
        return _event_heap.empty();
    }

    [[nodiscard]] constexpr auto current() const noexcept {
        assert(!finished());
        // return std::make_pair(_event_heap.top(),
        //                       ranges::views::keys(_current_event_it->second));
        return std::make_pair(_event_heap.top(),
                              std::views::all(_intersections));
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
        } while(_intersections.size() < 2);
    }
    void run() noexcept {
        while(!_event_heap.empty()) {
            const intersection & i = _event_heap.top();
            _current_event_it = _events_map.find(i);

            fmt::print("({}/{}, {}/{})\n", std::get<0>(i).num,
                       std::get<0>(i).denom, std::get<1>(i).num,
                       std::get<1>(i).denom);

            handle_event(*_current_event_it);

            // fmt::print("segment_tree ({}) : {}\n", well_formed_tree(),
            //            fmt::join(std::views::transform(_segment_tree,
            //                                            [](const auto & e) {
            //                                                return
            //                                                e.segment_id;
            //                                            }),
            //                      ","));

            // for(const auto & s : _intersections) {
            //     const auto & [p1, p2] = _segment_map[s];
            //     fmt::print("\t{} : [({}, {}), ({}, {})]\n", s, p1.first,
            //                p1.second, p2.first, p2.second);
            // }

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
