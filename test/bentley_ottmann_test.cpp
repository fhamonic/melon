#undef NDEBUG
#include <gtest/gtest.h>

#include <random>
#include <ranges>

#include <mp++/integer.hpp>

#include "melon/algorithm/bentley_ottmann.hpp"

using namespace fhamonic::melon;

template <typename N, typename D>
testing::AssertionResult & operator<<(testing::AssertionResult & result,
                                      const rational<N, D> & r) {
    return result << "(" << r.num << '/' << r.den << ")";
}

#include "ranges_test_helper.hpp"

template <typename C, C BOX_MIN, C BOX_MAX>
auto generate_random_box_segments(std::size_t num_segments) {
    using segment = std::pair<std::pair<C, C>, std::pair<C, C>>;
    std::vector<segment> segments;
    segments.reserve(num_segments);

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<C> dist(BOX_MIN, BOX_MAX);

    while(segments.size() < num_segments) {
        const auto & s =
            segments.emplace_back(std::make_pair(dist(rng), dist(rng)),
                                  std::make_pair(dist(rng), dist(rng)));
        // coincident points
        // if(s.first.first == s.second.first &&
        //    s.first.second == s.second.second) {
        //     segments.pop_back();
        //     continue;
        // }
        // vertical segments
        // if(s.first.first == s.second.first) {
        //     segments.pop_back();
        //     continue;
        // }
        // colinear segments
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
    return segments;
}

template <typename C, C BOX_MIN, C BOX_MAX, C VEC_LENGTH>
auto generate_random_vector_segments(std::size_t num_segments) {
    using segment = std::pair<std::pair<C, C>, std::pair<C, C>>;
    std::vector<segment> segments;
    segments.reserve(num_segments);

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<C> box_dist(BOX_MIN + VEC_LENGTH,
                                              BOX_MAX - VEC_LENGTH);
    std::uniform_int_distribution<C> vec_dist(-VEC_LENGTH, VEC_LENGTH);

    while(segments.size() < num_segments) {
        auto a = box_dist(rng);
        auto b = box_dist(rng);
        const auto & s = segments.emplace_back(
            std::make_pair(a, b),
            std::make_pair(a + vec_dist(rng), b + vec_dist(rng)));
    }
    return segments;
}

template <typename S>
auto naive_intersections(const std::vector<S> segments) {
    using intersection = decltype(cartesian::segments_intersection(
        std::declval<S>(), std::declval<S>()))::value_type;
    std::map<intersection, std::set<std::size_t>,
             cartesian::point_xy_comparator>
        intersections_map;

    auto point_eq = [](const auto & p1, const auto & p2) {
        return std::get<0>(p1) == std::get<0>(p2) &&
               std::get<1>(p1) == std::get<1>(p2);
    };

    const std::size_t n = segments.size();
    for(std::size_t i = 0; i < n; ++i) {
        const auto & s1 = segments[i];
        const auto & [a, b] = s1;
        for(std::size_t j = i + 1; j < n; ++j) {
            const auto & s2 = segments[j];
            const auto & [c, d] = s2;
            if(point_eq(a, b) && point_eq(c, d)) {
                if(point_eq(a, c)) {
                    intersections_map[a].emplace(i);
                    intersections_map[a].emplace(j);
                }
                continue;
            }
            if(point_eq(a, b) && !point_eq(c, d)) {
                if(cartesian::point_on_segment(a, s2)) {
                    intersections_map[a].emplace(i);
                    intersections_map[a].emplace(j);
                }
                continue;
            }
            if(!point_eq(a, b) && point_eq(c, d)) {
                if(cartesian::point_on_segment(c, s1)) {
                    intersections_map[c].emplace(i);
                    intersections_map[c].emplace(j);
                }
                continue;
            }
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
        // fmt::print("({}/{}, {}/{}) : {}\n", std::get<0>(i).num,
        //            std::get<0>(i).den, std::get<1>(i).num,
        //            std::get<1>(i).den,
        //            fmt::join(intersecting_segments, " , "));
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

GTEST_TEST(bentley_ottmann, fuzzy_box_test) {
    const std::size_t num_tests = 1000;
    std::size_t num_intersection_sum = 0;

    for(std::size_t test_i = 0; test_i < num_tests; ++test_i) {
        const std::size_t & num_segments = 100;
        auto segments =
            generate_random_box_segments<int64_t, -256, 255>(num_segments);

        using segment = decltype(segments)::value_type;
        using intersection = decltype(cartesian::segments_intersection(
            std::declval<segment>(), std::declval<segment>()))::value_type;

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
            //            std::get<0>(i).den, std::get<1>(i).num,
            //            std::get<1>(i).den,
            //            fmt::join(intersecting_segments, " , "));
        }
        const std::size_t num_intersections = intersections_vec.size();
        // num_intersection_sum += num_intersections;
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
GTEST_TEST(bentley_ottmann, fuzzy_vector_test) {
    const std::size_t num_tests = 1000;

    for(std::size_t test_i = 0; test_i < num_tests; ++test_i) {
        const std::size_t & num_segments = 100;
        auto segments = generate_random_vector_segments<int64_t, -96, 95, 32>(
            num_segments);

        using segment = decltype(segments)::value_type;
        using intersection = decltype(cartesian::segments_intersection(
            std::declval<segment>(), std::declval<segment>()))::value_type;

        std::vector<std::pair<intersection, std::vector<std::size_t>>>
            intersections_vec;
        intersections_vec.reserve(
            static_cast<std::size_t>(std::pow(num_segments, 1.5)));
        for(const auto & [i, intersecting_segments] :
            bentley_ottmann(std::views::iota(0ul, num_segments), segments)) {
            intersections_vec.emplace_back(std::make_pair(
                i, std::vector<std::size_t>(intersecting_segments.begin(),
                                            intersecting_segments.end())));
        }
        const std::size_t num_intersections = intersections_vec.size();
        auto naive_intersections_vec = naive_intersections(segments);

        ASSERT_TRUE(EQ_MULTISETS(std::views::keys(intersections_vec),
                                 std::views::keys(naive_intersections_vec)));

        for(std::size_t i = 0; i < num_intersections; ++i) {
            ASSERT_TRUE(EQ_MULTISETS(intersections_vec[i].second,
                                     naive_intersections_vec[i].second));
        }
    }
}

GTEST_TEST(bentley_ottmann, fuzzy_test_gmp) {
    using segment_coord = mppp::integer<1>;
    using segment = std::pair<std::pair<segment_coord, segment_coord>,
                              std::pair<segment_coord, segment_coord>>;
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
        using coord_type = int64_t;
        std::uniform_int_distribution<coord_type> dist(
            std::numeric_limits<coord_type>::min(),
            std::numeric_limits<coord_type>::max());

        while(segments.size() < num_segments) {
            const auto & s =
                segments.emplace_back(std::make_pair(segment_coord{dist(rng)},
                                                     segment_coord{dist(rng)}),
                                      std::make_pair(segment_coord{dist(rng)},
                                                     segment_coord{dist(rng)}));
        }

        std::vector<std::pair<intersection, std::vector<std::size_t>>>
            intersections_vec;
        intersections_vec.reserve(
            static_cast<std::size_t>(std::pow(num_segments, 1.5)));
        for(const auto & [i, intersecting_segments] :
            bentley_ottmann(std::views::iota(0ul, num_segments), segments)) {
            intersections_vec.emplace_back(std::make_pair(
                i, std::vector<std::size_t>(intersecting_segments.begin(),
                                            intersecting_segments.end())));
        }
        const std::size_t num_intersections = intersections_vec.size();
        auto naive_intersections_vec = naive_intersections(segments);

        ASSERT_TRUE(EQ_MULTISETS(std::views::keys(intersections_vec),
                                 std::views::keys(naive_intersections_vec)));

        for(std::size_t i = 0; i < num_intersections; ++i) {
            ASSERT_TRUE(EQ_MULTISETS(intersections_vec[i].second,
                                     naive_intersections_vec[i].second));
        }
    }
}