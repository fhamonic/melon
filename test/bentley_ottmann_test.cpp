#undef NDEBUG
#include <gtest/gtest.h>

#include "melon/experimental/bentley_ottmann.hpp"

#include "ranges_test_helper.hpp"

using namespace fhamonic::melon;

GTEST_TEST(bentley_ottmann, test) {
    std::vector<std::pair<std::pair<int8_t, int8_t>, std::pair<int8_t, int8_t>>>
        segments = {{{0, 0}, {2, 1}},  {{1, 1}, {2, -1}}, {{1, 0}, {2, 1}},
                    {{1, 3}, {3, -1}}, {{-1, 0}, {4, 0}}, {{0, -2}, {3, 0}}};

    bentley_ottmann alg(std::views::iota(0ul, segments.size()), segments);

    alg.run();
}

template <typename S>
auto naive_intersections(const std::vector<S> segments) {
    using intersection = decltype(cartesian::segment_intersection(
        std::declval<S>(), std::declval<S>()))::value_type;
    std::unordered_map<intersection, std::set<std::size_t>> intersections_map;

    const std::size_t n = segments.size();
    for(std::size_t i = 0; i < n; ++i) {
        const auto & s1 = segments[i];
        for(std::size_t j = i + 1; j < n; ++j) {
            const auto & s2 = segments[j];
            const auto & p = cartesian::segment_intersection(s1, s2);
            if(!p.has_value()) continue;
            intersections_map[p.value()].emplace(i);
            intersections_map[p.value()].emplace(j);
        }
    }

    return intersections_map;
}

#include "type_name.hpp"

GTEST_TEST(bentley_ottmann, algorithm_iterator) {
    std::vector<std::pair<std::pair<int8_t, int8_t>, std::pair<int8_t, int8_t>>>
        segments = {{{0, 0}, {2, 1}},  {{1, 1}, {2, -1}}, {{1, 0}, {2, 1}},
                    {{1, 3}, {3, -1}}, {{-1, 0}, {4, 0}}, {{0, -2}, {3, 0}}};
    auto segments_ids = std::views::iota(0ul, segments.size());

    bentley_ottmann alg(segments_ids, segments);

    static_assert(std::ranges::input_range<decltype(alg)>);
    static_assert(std::ranges::viewable_range<decltype(alg)>);

    for(const auto & [i, intersecting_segments] : alg) {
        fmt::print("({}/{}, {}/{}) : {}\n", std::get<0>(i).num,
                   std::get<0>(i).denom, std::get<1>(i).num,
                   std::get<1>(i).denom,
                   fmt::join(intersecting_segments, " , "));
    }
    std::cout << std::endl;
    for(const auto & [i, intersecting_segments] :
        naive_intersections(segments)) {
        fmt::print("({}/{}, {}/{}) : {}\n", std::get<0>(i).num,
                   std::get<0>(i).denom, std::get<1>(i).num,
                   std::get<1>(i).denom,
                   fmt::join(intersecting_segments, " , "));
    }
}
