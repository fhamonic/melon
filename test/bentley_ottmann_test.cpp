#undef NDEBUG
#include <gtest/gtest.h>

#include <random>
#include <ranges>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <mp++/integer.hpp>

#include "type_name.hpp"

#include "melon/algorithm/bentley_ottmann.hpp"
#include "melon/utility/bounded_value.hpp"

using namespace fhamonic::melon;

template <typename N, typename D>
testing::AssertionResult & operator<<(testing::AssertionResult & result,
                                      const rational<N, D> & r) {
    return result << "(" << r.num() << '/' << r.den() << ")";
}

#include "ranges_test_helper.hpp"

template <typename C, int BOX_MIN, int BOX_MAX>
auto generate_random_box_segments(std::size_t num_segments) {
    using point = std::tuple<C, C>;
    using segment = std::tuple<point, point>;
    std::vector<segment> segments;
    segments.reserve(num_segments);

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<int> dist(BOX_MIN, BOX_MAX);

    while(segments.size() < num_segments) {
        const auto & s = segments.emplace_back(point(dist(rng), dist(rng)),
                                               point(dist(rng), dist(rng)));
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

template <typename C, int BOX_MIN, int BOX_MAX, int VEC_LENGTH>
auto generate_random_vector_segments(std::size_t num_segments) {
    using point = std::tuple<C, C>;
    using segment = std::tuple<point, point>;
    std::vector<segment> segments;
    segments.reserve(num_segments);

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<int> box_dist(BOX_MIN + VEC_LENGTH,
                                                BOX_MAX - VEC_LENGTH);
    std::uniform_int_distribution<int> vec_dist(-VEC_LENGTH, VEC_LENGTH);

    while(segments.size() < num_segments) {
        auto a = box_dist(rng);
        auto b = box_dist(rng);
        const auto & s = segments.emplace_back(
            point(a, b), point(a + vec_dist(rng), b + vec_dist(rng)));
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

// GTEST_TEST(bentley_ottmann, run_example) {
//     using coord_t = integer<int64_t>;
//     using segment =
//         std::tuple<std::tuple<coord_t, coord_t>, std::tuple<coord_t,
//         coord_t>>;

//     // std::vector<segment> segments = {
//         // {{0, 0}, {2, 0}}, {{1, 0}, {2, 1}}, {{1, 0}, {2, -1}}, {{0, -1},
//         // {2, 2}}};
//     std::vector<segment> segments = {{{0, 0}, {1, 0}},  {{0, -1}, {2, 1}},
//                                      {{0, 1}, {3, 0}},  {{2, -1}, {2, 4}}};
//     auto segments_ids = std::views::iota(0ul, segments.size());

//     bentley_ottmann alg(segments_ids, segments);

//     alg.run();
// }

GTEST_TEST(bentley_ottmann, run_integer_example) {
    using coord = integer<int64_t>;
    // using coord = integer<bounded_value<int32_t, -16, 16>>;
    using point = std::tuple<coord, coord>;
    using segment = std::tuple<point, point>;
    using intersection = decltype(cartesian::segments_intersection(
        std::declval<segment>(), std::declval<segment>()))::value_type;

    std::cout << type_name<intersection>() << std::endl;

    // point p(1, 1);
    // intersection i(p);
    // intersection i(std::get<0>(p), std::get<1>(p));

    std::vector<segment> segments = {{{0, 0}, {1, 0}},
                                     {{0, -1}, {2, 1}},
                                     {{0, 1}, {3, 0}},
                                     {{2, -1}, {2, 4}}};

    auto segments_ids = std::views::iota(0ul, segments.size());

    for(auto && [i, intersecting_segments] :
        bentley_ottmann(segments_ids, segments)) {
        fmt::print("({}/{}, {}/{}) : {}\n", int(std::get<0>(i).num()),
                   int(std::get<0>(i).den()), int(std::get<1>(i).num()),
                   int(std::get<1>(i).den()),
                   fmt::join(intersecting_segments, " , "));
    }
}

GTEST_TEST(bentley_ottmann, fuzzy_dense_test) {
    using coord = integer<int64_t>;
    using point = std::tuple<coord, coord>;
    using segment = std::tuple<point, point>;
    using intersection = decltype(cartesian::segments_intersection(
        std::declval<segment>(), std::declval<segment>()))::value_type;

    const std::size_t num_tests = 100;

    for(std::size_t test_i = 0; test_i < num_tests; ++test_i) {
        const std::size_t & num_segments = 100;
        auto segments =
            generate_random_box_segments<coord, -256, 255>(num_segments);

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

            // fmt::print("({}/{}, {}/{}) : {}\n", std::get<0>(i).num(),
            //            std::get<0>(i).den(), std::get<1>(i).num(),
            //            std::get<1>(i).den(),
            //            fmt::join(intersecting_segments, " , "));
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
GTEST_TEST(bentley_ottmann, fuzzy_sparse_test) {
    using coord = integer<int64_t>;
    using point = std::tuple<coord, coord>;
    using segment = std::tuple<point, point>;
    using intersection = decltype(cartesian::segments_intersection(
        std::declval<segment>(), std::declval<segment>()))::value_type;

    const std::size_t num_tests = 100;

    for(std::size_t test_i = 0; test_i < num_tests; ++test_i) {
        const std::size_t & num_segments = 100;
        auto segments =
            generate_random_vector_segments<coord, -96, 95, 32>(num_segments);

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

GTEST_TEST(bentley_ottmann, fuzzy_dense_bounded_value_test) {
    using coord = integer<bounded_value<int8_t>>;
    using point = std::tuple<coord, coord>;
    using segment = std::tuple<point, point>;
    using intersection = decltype(cartesian::segments_intersection(
        std::declval<segment>(), std::declval<segment>()))::value_type;

    const std::size_t num_tests = 100;

    for(std::size_t test_i = 0; test_i < num_tests; ++test_i) {
        const std::size_t & num_segments = 100;
        auto segments =
            generate_random_box_segments<rational<bounded_value<int8_t>>, -128,
                                         127>(num_segments);

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

GTEST_TEST(bentley_ottmann, fuzzy_sparse_bounded_value_test) {
    using coord = integer<bounded_value<int8_t>>;
    using point = std::tuple<coord, coord>;
    using segment = std::tuple<point, point>;
    using intersection = decltype(cartesian::segments_intersection(
        std::declval<segment>(), std::declval<segment>()))::value_type;

    // std::cout << type_name<intersection>() << std::endl;

    const std::size_t num_tests = 100;

    for(std::size_t test_i = 0; test_i < num_tests; ++test_i) {
        const std::size_t & num_segments = 100;
        auto segments =
            generate_random_vector_segments<coord, -96, 95, 32>(num_segments);

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

// GTEST_TEST(bentley_ottmann, fuzzy_test_mppp) {
//     using coord = rational<mppp::integer<1>>;
//     using point = std::tuple<coord, coord>;
//     using segment = std::tuple<point, point>;
//     using intersection = decltype(cartesian::segments_intersection(
//         std::declval<segment>(), std::declval<segment>()))::value_type;

//     const std::size_t num_tests = 100;

//     for(std::size_t test_i = 0; test_i < num_tests; ++test_i) {
//         const std::size_t & num_segments = 100;
//         auto segments =
//             generate_random_box_segments<coord, -128, 127>(num_segments);

//         std::vector<std::pair<intersection, std::vector<std::size_t>>>
//             intersections_vec;
//         intersections_vec.reserve(
//             static_cast<std::size_t>(std::pow(num_segments, 1.5)));
//         for(const auto & [i, intersecting_segments] :
//             bentley_ottmann(std::views::iota(0ul, num_segments), segments)) {
//             intersections_vec.emplace_back(std::make_pair(
//                 i, std::vector<std::size_t>(intersecting_segments.begin(),
//                                             intersecting_segments.end())));
//         }
//         const std::size_t num_intersections = intersections_vec.size();
//         auto naive_intersections_vec = naive_intersections(segments);

//         ASSERT_TRUE(EQ_MULTISETS(std::views::keys(intersections_vec),
//                                  std::views::keys(naive_intersections_vec)));

//         for(std::size_t i = 0; i < num_intersections; ++i) {
//             ASSERT_TRUE(EQ_MULTISETS(intersections_vec[i].second,
//                                      naive_intersections_vec[i].second));
//         }
//     }
// }