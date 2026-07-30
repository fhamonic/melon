#undef NDEBUG
#include <gtest/gtest.h>

#include <random>
#include <ranges>

// #include <mp++/integer.hpp>

#include "type_name.hpp"

#include "melon/algorithm/bentley_ottmann.hpp"
#include "melon/numeric/bounded_value.hpp"

using namespace melon;
using namespace melon::numeric;

// declared before ranges_test_helper.hpp is included, so its assertion
// helpers can stream rational coordinates
template <typename N, typename D>
testing::AssertionResult & operator<<(testing::AssertionResult & result,
                                      const rational<N, D> & r) {
    return result << "(" << r.num() << '/' << r.den() << ")";
}

#include "ranges_test_helper.hpp"

////////////////////////////////////////////////////////////////////////////////
// fixtures: random segment generators, and a naive quadratic oracle the sweep
// is checked against
////////////////////////////////////////////////////////////////////////////////

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
        // std::cout << std::format("({}/{}, {}/{}) : {}\n", std::get<0>(i).num,
        //                          std::get<0>(i).den, std::get<1>(i).num,
        //                          std::get<1>(i).den, intersecting_segments);
    }
    std::ranges::sort(
        naive_intersections_vec, [](const auto & e1, const auto & e2) {
            if(std::get<0>(e1.first) == std::get<0>(e2.first))
                return std::get<1>(e1.first) < std::get<1>(e2.first);
            return std::get<0>(e1.first) < std::get<0>(e2.first);
        });

    return naive_intersections_vec;
}

////////////////////////////////////////////////////////////////////////////////
// bentley_ottmann sweeps a hand-built example, reporting each intersection
// point with the segments through it
////////////////////////////////////////////////////////////////////////////////

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
#if defined(__cpp_lib_format_ranges) && __cpp_lib_format_ranges >= 202207L
        std::cout << std::format(
            "({}/{}, {}/{}) : {}\n", int(std::get<0>(i).num()),
            int(std::get<0>(i).den()), int(std::get<1>(i).num()),
            int(std::get<1>(i).den()), intersecting_segments);
#else
        std::cout << "(" << int(std::get<0>(i).num()) << "/"
                  << int(std::get<0>(i).den()) << ", "
                  << int(std::get<1>(i).num()) << "/"
                  << int(std::get<1>(i).den()) << ") : {";
        bool first = true;
        for(auto id : intersecting_segments) {
            if(!first) std::cout << ", ";
            std::cout << id;
            first = false;
        }
        std::cout << "}\n";
#endif
    }
}

////////////////////////////////////////////////////////////////////////////////
// mid-run relocation: the trees' comparators reference the heap-anchored
// event points, whose address a move transfers intact -- so a sweep moved
// mid-run (construction and assignment, source destroyed either way) yields
// exactly what an undisturbed sweep yields. Pins the fix for the defaulted
// moves comparing against the moved-from object's members (ASan-confirmed
// use-after-free before the anchor).
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(bentley_ottmann, mid_run_move) {
    using coord = integer<int64_t>;
    using point = std::tuple<coord, coord>;
    using segment = std::tuple<point, point>;
    using intersection = decltype(cartesian::segments_intersection(
        std::declval<segment>(), std::declval<segment>()))::value_type;

    std::vector<segment> segments = {{{0, 0}, {8, 8}}, {{0, 8}, {8, 0}},
                                     {{0, 4}, {8, 4}}, {{2, 0}, {2, 8}},
                                     {{6, 0}, {6, 8}}, {{0, 2}, {8, 6}}};
    auto segments_ids = std::views::iota(0ul, segments.size());

    std::vector<std::pair<intersection, std::vector<std::size_t>>> expected;
    for(const auto & [i, intersecting_segments] :
        bentley_ottmann(segments_ids, segments)) {
        expected.emplace_back(
            i, std::vector<std::size_t>(intersecting_segments.begin(),
                                        intersecting_segments.end()));
    }
    ASSERT_GT(expected.size(), 2ul);

    // move construction, the moved-from algorithm destroyed mid-run
    {
        std::vector<std::pair<intersection, std::vector<std::size_t>>> found;
        auto moved = [&]() {
            auto src = std::make_unique<decltype(bentley_ottmann(
                segments_ids, segments))>(segments_ids, segments);
            const auto & [i, intersecting_segments] = src->current();
            found.emplace_back(
                i, std::vector<std::size_t>(intersecting_segments.begin(),
                                            intersecting_segments.end()));
            src->advance();
            return std::move(*src);
        }();
        for(; !moved.finished(); moved.advance()) {
            const auto & [i, intersecting_segments] = moved.current();
            found.emplace_back(
                i, std::vector<std::size_t>(intersecting_segments.begin(),
                                            intersecting_segments.end()));
        }
        ASSERT_EQ(found, expected);
    }

    // move assignment over an algorithm holding live trees of its own
    {
        bentley_ottmann alg(segments_ids, segments);
        bentley_ottmann other(std::views::iota(0ul, 2ul), segments);
        alg.advance();
        other = std::move(alg);
        std::vector<std::pair<intersection, std::vector<std::size_t>>> found = {
            expected.front()};
        for(; !other.finished(); other.advance()) {
            const auto & [i, intersecting_segments] = other.current();
            found.emplace_back(
                i, std::vector<std::size_t>(intersecting_segments.begin(),
                                            intersecting_segments.end()));
        }
        ASSERT_EQ(found, expected);
    }
}

////////////////////////////////////////////////////////////////////////////////
// reset() re-seeds the event queue from the stored id range and replays the
// sweep. Pins the fix for reset() clearing the trees with no way to refill
// them, which left the object permanently finished().
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(bentley_ottmann, reset_replays_the_sweep) {
    using coord = integer<int64_t>;
    using point = std::tuple<coord, coord>;
    using segment = std::tuple<point, point>;
    using intersection = decltype(cartesian::segments_intersection(
        std::declval<segment>(), std::declval<segment>()))::value_type;

    std::vector<segment> segments = {{{0, 0}, {8, 8}}, {{0, 8}, {8, 0}},
                                     {{0, 4}, {8, 4}}, {{2, 0}, {2, 8}},
                                     {{6, 0}, {6, 8}}, {{0, 2}, {8, 6}}};
    auto segments_ids = std::views::iota(0ul, segments.size());

    bentley_ottmann alg(segments_ids, segments);

    std::vector<std::pair<intersection, std::vector<std::size_t>>> first_run;
    for(; !alg.finished(); alg.advance()) {
        const auto & [i, intersecting_segments] = alg.current();
        first_run.emplace_back(
            i, std::vector<std::size_t>(intersecting_segments.begin(),
                                        intersecting_segments.end()));
    }
    ASSERT_GT(first_run.size(), 2ul);
    ASSERT_TRUE(alg.finished());

    alg.reset();
    ASSERT_FALSE(alg.finished());

    std::vector<std::pair<intersection, std::vector<std::size_t>>> second_run;
    for(; !alg.finished(); alg.advance()) {
        const auto & [i, intersecting_segments] = alg.current();
        second_run.emplace_back(
            i, std::vector<std::size_t>(intersecting_segments.begin(),
                                        intersecting_segments.end()));
    }
    ASSERT_EQ(second_run, first_run);
}

////////////////////////////////////////////////////////////////////////////////
// fuzzy: on random segment sets, the sweep finds the same intersection points
// and the same segments through each as the quadratic oracle
////////////////////////////////////////////////////////////////////////////////

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

        // std::cout << std::format("test {}\n", test_i);
        // for(auto && s : segments) {
        //     auto && [a, b] = s;
        //     auto && [ax, ay] = a;
        //     auto && [bx, by] = b;
        //     std::cout << std::format("{{{{{},{}}},{{{},{}}}}},\n", ax, ay,
        //     bx, by);
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

            // std::cout << std::format("({}/{}, {}/{}) : {}\n",
            // std::get<0>(i).num(),
            //            std::get<0>(i).den(), std::get<1>(i).num(),
            //            std::get<1>(i).den(),
            //            intersecting_segments);
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

////////////////////////////////////////////////////////////////////////////////
// fuzzy: the same agreement holds with bounded_value coordinates, whose
// arithmetic widens instead of overflowing
////////////////////////////////////////////////////////////////////////////////

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
