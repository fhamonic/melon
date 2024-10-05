#undef NDEBUG
#include <gtest/gtest.h>

#include <random>
#include <ranges>

#include "melon/algorithm/bentley_ottmann.hpp"

using namespace fhamonic::melon;

testing::AssertionResult & operator<<(testing::AssertionResult & result,
                                      const rational<long int> & r) {
    return result << "(" << r.num << '/' << r.denom << ")";
}

#include "ranges_test_helper.hpp"

template <typename S>
auto naive_intersections(const std::vector<S> segments) {
    using intersection = decltype(cartesian::segments_intersection(
        std::declval<S>(), std::declval<S>()))::value_type;
    std::unordered_map<intersection, std::set<std::size_t>> intersections_map;

    const std::size_t n = segments.size();
    for(std::size_t i = 0; i < n; ++i) {
        const auto & s1 = segments[i];
        for(std::size_t j = i + 1; j < n; ++j) {
            const auto & s2 = segments[j];
            const auto & p = cartesian::segments_intersection(s1, s2);
            if(!p.has_value()) {
                const auto & is = cartesian::segments_overlap(s1, s2);
                if(!is.has_value()) continue;
                const auto & [k, l] = is.value();
                intersections_map[k].emplace(i);
                intersections_map[k].emplace(j);
                intersections_map[l].emplace(i);
                intersections_map[l].emplace(j);
                continue;
            }
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

// GTEST_TEST(bentley_ottmann, run_example) {
//     using segment =
//         std::pair<std::pair<int64_t, int64_t>, std::pair<int64_t, int64_t>>;

//     // std::vector<segment> segments = {
//         // {{0, 0}, {2, 0}}, {{1, 0}, {2, 1}}, {{1, 0}, {2, -1}}, {{0, -1},
//         // {2, 2}}};
//     std::vector<segment> segments = {{{0, 0}, {1, 0}},  {{0, -1}, {2, 1}},
//                                      {{0, 1}, {3, 0}},  {{2, -1}, {2, 4}}};
//     auto segments_ids = std::views::iota(0ul, segments.size());

//     bentley_ottmann alg(segments_ids, segments);

//     alg.run();
// }

GTEST_TEST(bentley_ottmann, fuzzy_test_no_colinear) {
    using segment =
        std::pair<std::pair<int64_t, int64_t>, std::pair<int64_t, int64_t>>;
    using intersection = decltype(cartesian::segments_intersection(
        std::declval<segment>(), std::declval<segment>()))::value_type;

    const std::size_t num_tests = 1000;
    std::size_t num_intersection_sum = 0;

    for(std::size_t test_i = 0; test_i < num_tests; ++test_i) {
        const std::size_t & num_segments = 100;
        std::vector<segment> segments;
        segments.reserve(num_segments);

        std::random_device dev;
        std::mt19937 rng(dev());
        using coord_type = int8_t;
        std::uniform_int_distribution<int64_t> dist(
            std::numeric_limits<coord_type>::min(),
            std::numeric_limits<coord_type>::max());

        while(segments.size() < num_segments) {
            const auto & s =
                segments.emplace_back(std::make_pair(dist(rng), dist(rng)),
                                      std::make_pair(dist(rng), dist(rng)));
            if(s.first.first == s.second.first &&
               s.first.second == s.second.second) {
                segments.pop_back();
                continue;
            }
            // if(s.first.first == s.second.first) {
            //     segments.pop_back();
            //     continue;
            // }
            // if(std::any_of(segments.begin(), std::prev(segments.end()),
            //                [s](auto && s2) {
            //                    return cartesian::line_slope(
            //                               cartesian::segment_to_line(s)) ==
            //                           cartesian::line_slope(
            //                               cartesian::segment_to_line(s2));
            //                })) {
            //     segments.pop_back();
            //     continue;
            // }
        }

        // fmt::print("test {}\n", test_i);
        // for(auto && s : segments) {
        //     auto && [a, b] = s;
        //     auto && [ax, ay] = a;
        //     auto && [bx, by] = b;
        //     fmt::print("{{{{{},{}}},{{{},{}}}}},\n", ax, ay, bx, by);
        // }

        std::vector<std::pair<intersection, std::vector<std::size_t>>>
            intersections_vec;
        intersections_vec.reserve(
            static_cast<std::size_t>(std::pow(num_segments, 1.5)));
        for(const auto & [i, intersecting_segments] :
            bentley_ottmann(std::views::iota(0ul, num_segments), segments)) {
            intersections_vec.emplace_back(std::make_pair(
                i, std::vector<std::size_t>(intersecting_segments.begin(),
                                            intersecting_segments.end())));

            // fmt::print("({}/{}, {}/{}) : {}\n", std::get<0>(i).num,
            //            std::get<0>(i).denom, std::get<1>(i).num,
            //            std::get<1>(i).denom,
            //            fmt::join(intersecting_segments, " , "));
        }
        const std::size_t num_intersections = intersections_vec.size();
        num_intersection_sum += num_intersections;
        auto naive_intersections_vec = naive_intersections(segments);

        ASSERT_TRUE(EQ_MULTISETS(std::views::keys(intersections_vec),
                                 std::views::keys(naive_intersections_vec)));

        for(std::size_t i = 0; i < num_intersections; ++i) {
            ASSERT_TRUE(EQ_MULTISETS(intersections_vec[i].second,
                                     naive_intersections_vec[i].second));
        }
    }
    // fmt::println("avg number of intersections : {}",
    //              num_intersection_sum / num_tests);
}