#undef NDEBUG
#include <gtest/gtest.h>

#include <ranges>

#include "melon/experimental/bentley_ottmann.hpp"
using namespace fhamonic::melon;

testing::AssertionResult & operator<<(testing::AssertionResult & result,
                                      const rational<long int> & r) {
    return result << "(" << r.num << '/' << r.denom << ")";
}

#include "ranges_test_helper.hpp"

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

    std::vector<std::pair<intersection, std::vector<std::size_t>>>
        naive_intersections_vec;
    for(const auto & [i, intersecting_segments] : intersections_map) {
        naive_intersections_vec.emplace_back(std::make_pair(
            i, std::vector<std::size_t>(intersecting_segments.begin(),
                                        intersecting_segments.end())));
    }
    std::ranges::sort(
        naive_intersections_vec, [](const auto & e1, const auto & e2) {
            if(std::get<0>(e1.first) == std::get<0>(e2.first))
                return std::get<1>(e1.first) < std::get<1>(e2.first);
            return std::get<0>(e1.first) < std::get<0>(e2.first);
        });

    return naive_intersections_vec;
}

#include "type_name.hpp"

GTEST_TEST(bentley_ottmann, test) {
    using segment =
        std::pair<std::pair<int64_t, int64_t>, std::pair<int64_t, int64_t>>;
    using intersection = decltype(cartesian::segment_intersection(
        std::declval<segment>(), std::declval<segment>()))::value_type;

    std::vector<segment> segments = {{{0, 0}, {2, 1}},  {{1, 1}, {2, -1}},
                                     {{1, 0}, {2, 1}},  {{1, 3}, {3, -1}},
                                     {{-1, 0}, {4, 0}}, {{0, -2}, {3, 0}}};
    auto segments_ids = std::views::iota(0ul, segments.size());

    bentley_ottmann alg(segments_ids, segments);

    static_assert(std::ranges::input_range<decltype(alg)>);
    static_assert(std::ranges::viewable_range<decltype(alg)>);

    std::vector<std::pair<intersection, std::vector<std::size_t>>>
        intersections_vec;
    for(const auto & [i, intersecting_segments] : alg) {
        fmt::print("({}/{}, {}/{}) : {}\n", std::get<0>(i).num,
                   std::get<0>(i).denom, std::get<1>(i).num,
                   std::get<1>(i).denom,
                   fmt::join(intersecting_segments, " , "));
        intersections_vec.emplace_back(std::make_pair(
            i, std::vector<std::size_t>(intersecting_segments.begin(),
                                        intersecting_segments.end())));
    }
    std::vector<std::pair<intersection, std::vector<std::size_t>>>
        naive_intersections_vec = naive_intersections(segments);

    ASSERT_TRUE(EQ_RANGES(std::views::keys(intersections_vec),
                          std::views::keys(naive_intersections_vec)));

    const std::size_t num_intersections = intersections_vec.size();
    for(std::size_t i = 0; i < num_intersections; ++i) {
        ASSERT_TRUE(EQ_MULTISETS(intersections_vec[i].second,
                                 naive_intersections_vec[i].second));
    }
}

GTEST_TEST(bentley_ottmann, fuzzy_test) {
    using segment =
        std::pair<std::pair<int64_t, int64_t>, std::pair<int64_t, int64_t>>;
    using intersection = decltype(cartesian::segment_intersection(
        std::declval<segment>(), std::declval<segment>()))::value_type;

    std::vector<segment> segments;

    std::vector<std::pair<intersection, std::vector<std::size_t>>>
        intersections_vec;
    for(const auto & [i, intersecting_segments] :
        bentley_ottmann(std::views::iota(0ul, segments.size()), segments)) {
        intersections_vec.emplace_back(std::make_pair(
            i, std::vector<std::size_t>(intersecting_segments.begin(),
                                        intersecting_segments.end())));
    }
    std::vector<std::pair<intersection, std::vector<std::size_t>>>
        naive_intersections_vec = naive_intersections(segments);

    ASSERT_TRUE(EQ_RANGES(std::views::keys(intersections_vec),
                          std::views::keys(naive_intersections_vec)));

    const std::size_t num_intersections = intersections_vec.size();
    for(std::size_t i = 0; i < num_intersections; ++i) {
        ASSERT_TRUE(EQ_MULTISETS(intersections_vec[i].second,
                                 naive_intersections_vec[i].second));
    }
}