#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/detail/no_unique_address.hpp"
#include "melon/detail/not_self.hpp"

#include "melon/container/d_ary_heap.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/geometry.hpp"

namespace melon {

template <typename Traits>
concept bentley_ottmann_traits = requires {
    { Traits::report_endpoints } -> std::convertible_to<bool>;
};

template <typename Segment>
struct bentley_ottmann_default_traits {
    using coordinate_system = cartesian;
    using segment_type = Segment;
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

// Precondition no concept can check: the coordinate arithmetic must be exact.
// The sweep orders segments by where they cross the sweep line and reuses that
// order across events, so a rounded comparison makes segment_cmp inconsistent
// -- that is a strict-weak-ordering violation in the underlying tree, which is
// undefined behaviour, not merely a misreported point. Where the geometry
// divides, integral coordinates are promoted to numeric::rational; a
// floating-point coordinate type satisfies the concepts but not this
// precondition.
// Degeneracies are supported: an event reports every segment id passing
// through the point, so three concurrent segments and collinear overlaps come
// out right.
// O((n + k) log n) for n segments and k reported points.
//
// The id range is *stored* so that reset() can re-seed the event queue;
// forward_range because seeding walks it once per run, not once per object.
template <bentley_ottmann_traits Traits, std::ranges::view SegmentIdRange,
          mapping_view<std::ranges::range_value_t<SegmentIdRange>> SegmentMap =
              maps::identity_map>
    requires std::ranges::forward_range<SegmentIdRange>
class bentley_ottmann
    : public algorithm_view_interface<
          bentley_ottmann<Traits, SegmentIdRange, SegmentMap>> {
private:
    using segment_id_type = std::ranges::range_value_t<SegmentIdRange>;
    using coordinate_system = typename Traits::coordinate_system;
    using segment_type = typename Traits::segment_type;
    // Two decltypes, not a comma inside one: the comma operator discards the
    // first endpoint's type, so common_type is never consulted across the two
    // and a segment with differing endpoint types silently takes the second's.
    using endpoint_type =
        std::common_type_t<decltype(std::get<0>(std::declval<segment_type>())),
                           decltype(std::get<1>(std::declval<segment_type>()))>;
    using line_type = typename Traits::line_type;
    using intersection_type = typename Traits::intersection_type;
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
        // None of these is `const`: const members make the two defaulted
        // assignments below *deleted*, and `= default` on an operation the
        // compiler has already deleted reads as though the type were
        // assignable when it is not.
        // The crossing is `mutable` because it is a memo, recomputed on the
        // first comparison at a new event point, and the entries are const
        // both to segment_cmp and as tree keys.
        mutable sweepline_intersection_type sweepline_intersection;
        line_type line;
        segment_type segment;
        segment_id_type segment_id;

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
        typename Traits::template segments_tree<segment_entry, segment_cmp>;
    enum event_type { starting, ending, coincident };
    using events = std::vector<std::pair<segment_id_type, event_type>>;
    using events_tree = typename Traits::template events_tree<events>;

    // The two sweep points the tree comparators order against. segment_cmp
    // holds a std::cref to them and std::set carries its comparator with it on
    // move, so a comparator bound to a plain *member* would keep comparing
    // against the moved-from object -- a use-after-free once the source dies.
    // Behind a unique_ptr their address is move-invariant, which is what makes
    // the defaulted moves below sound. Declared before the trees: their
    // mem-initializers dereference it.
    struct event_points {
        intersection_type current;
        intersection_type tmp;
    };

private:
    SegmentIdRange _segment_ids_range;
    MELON_NO_UNIQUE_ADDRESS SegmentMap _segment_map;
    MELON_NO_UNIQUE_ADDRESS event_cmp _event_cmp;
    std::unique_ptr<event_points> _event_points;
    segments_tree _segments_tree;
    segments_tree _tmp_tree;
    events_tree _events_tree;

    std::vector<segment_id_type> _intersections;

public:
    // ---- Construction -------------------------------------------------------

    template <typename SIR, mapping_for<SegmentMap> SM = maps::identity_map>
        requires detail::not_self<SIR, bentley_ottmann> &&
                     std::ranges::forward_range<SIR> &&
                     std::constructible_from<SegmentIdRange,
                                             std::views::all_t<SIR>>
    bentley_ottmann(SIR && segments_ids_range, SM && segment_map = {})
        : _segment_ids_range(
              std::views::all(std::forward<SIR>(segments_ids_range)))
        , _segment_map(maps::mapping_all(std::forward<SM>(segment_map)))
        , _event_points(std::make_unique<event_points>())
        , _segments_tree(segment_cmp(std::cref(_event_points->current)))
        , _tmp_tree(segment_cmp(std::cref(_event_points->tmp))) {
        seed();
    }

    template <typename... Args>
        requires std::constructible_from<bentley_ottmann, Args...>
    constexpr bentley_ottmann(Traits, Args &&... args)
        : bentley_ottmann(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    constexpr bentley_ottmann(const bentley_ottmann &) = delete;
    constexpr bentley_ottmann(bentley_ottmann &&) = default;

    constexpr bentley_ottmann & operator=(const bentley_ottmann &) = delete;
    constexpr bentley_ottmann & operator=(bentley_ottmann &&) = default;

    // ---- Setup --------------------------------------------------------------

    constexpr bentley_ottmann & reset() {
        _events_tree.clear();
        _segments_tree.clear();
        _tmp_tree.clear();
        _intersections.clear();
        seed();
        return *this;
    }

private:
    void seed() {
        for(auto && s : _segment_ids_range) {
            // Read through the wrapped member, not the constructor parameter:
            // the latter need not be subscriptable (a callable, typically).
            const auto & [p1, p2] = _segment_map[s];
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
    void push_segment_endpoint(const endpoint_type & i,
                               const segment_id_type & s, const event_type et) {
        auto && [it, inserted] = _events_tree.try_emplace(intersection_type(i));
        it->second.emplace_back(s, et);
    }
    void push_intersection(const intersection_type & i) {
        _events_tree.try_emplace(i);
    }
    void detect_intersection(const segment_entry & e1,
                             const segment_entry & e2) {
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

        if(_event_cmp(i, _event_points->current)) return;

        push_intersection(i);
    }
    // The tree's value_type, not pair<intersection_type, events>: a std::map's
    // value_type has a const key, so the latter binds every call to a
    // temporary copying the point *and* the events vector.
    void handle_event(const typename events_tree::value_type & e) {
        const auto & [i, evts] = e;
        _event_points->tmp = i;

        _intersections.resize(0);
        auto after_last_removed_it = _segments_tree.lower_bound(i);
        while(after_last_removed_it != _segments_tree.end() &&
              after_last_removed_it->sweepline_y_intersection(i) ==
                  std::get<1>(i)) {
            // Without report_endpoints, a segment *ending* at the point is
            // suppressed like the starting ones handled below -- otherwise
            // the same endpoint-only geometry reports one event or zero
            // depending on which side of the point the segments lie.
            if constexpr(Traits::report_endpoints) {
                _intersections.emplace_back(after_last_removed_it->segment_id);
            } else {
                const auto sid = after_last_removed_it->segment_id;
                if(std::ranges::none_of(evts, [&](const auto & se) {
                       return std::get<0>(se) == sid &&
                              std::get<1>(se) == event_type::ending;
                   }))
                    _intersections.emplace_back(sid);
            }

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

        _event_points->current = i;

        for(const auto & [s, et] : evts) {
            if(et == event_type::ending) {
                _tmp_tree.erase(
                    _tmp_tree.find(segment_entry(s, _segment_map[s], i)));
                continue;
            }
            if constexpr(Traits::report_endpoints) {
                _intersections.emplace_back(s);
            }
            if(et != event_type::starting) continue;
            _tmp_tree.emplace(
                segment_entry(s, _segment_map[s], _event_points->current));
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
    // ---- Execution ----------------------------------------------------------

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_events_tree.empty())) {
        return _events_tree.empty();
    }

    [[nodiscard]] constexpr auto current() const
        noexcept(noexcept(std::make_pair(_events_tree.begin()->first,
                                         std::views::all(_intersections)))) {
        assert(!finished());
        return std::make_pair(_events_tree.begin()->first,
                              std::views::all(_intersections));
    }

    constexpr void advance() {
        assert(!finished());
        do {
            _events_tree.erase(_events_tree.begin());
            if(finished()) return;
            handle_event(*_events_tree.begin());
        } while(_intersections.size() < 2);
    }
};

template <typename SegmentIdRange>
bentley_ottmann(SegmentIdRange &&)
    -> bentley_ottmann<bentley_ottmann_default_traits<
                           std::ranges::range_value_t<SegmentIdRange>>,
                       std::views::all_t<SegmentIdRange>, maps::identity_map>;

template <typename SegmentIdRange, typename SegmentMap>
bentley_ottmann(SegmentIdRange &&, SegmentMap &&)
    -> bentley_ottmann<
        bentley_ottmann_default_traits<
            mapped_value_t<maps::mapping_all_t<SegmentMap>,
                           std::ranges::range_value_t<SegmentIdRange>>>,
        std::views::all_t<SegmentIdRange>, maps::mapping_all_t<SegmentMap>>;

template <typename SegmentIdRange, typename Traits>
bentley_ottmann(Traits, SegmentIdRange &&)
    -> bentley_ottmann<Traits, std::views::all_t<SegmentIdRange>,
                       maps::identity_map>;

template <typename SegmentIdRange, typename SegmentMap, typename Traits>
bentley_ottmann(Traits, SegmentIdRange &&, SegmentMap &&)
    -> bentley_ottmann<Traits, std::views::all_t<SegmentIdRange>,
                       maps::mapping_all_t<SegmentMap>>;

}  // namespace melon
