#ifndef RANGES_TEST_HELPER_HPP
#define RANGES_TEST_HELPER_HPP

#undef NDEBUG
#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <ranges>

// template <typename T1, typename T2>
// testing::AssertionResult & operator<<(testing::AssertionResult & result,
//                                       const std::pair<T1, T2> & p);
template <typename T1, typename T2>
testing::AssertionResult & operator<<(testing::AssertionResult & result,
                                      const std::pair<T1, T2> & p) {
    return result << "(" << p.first << "," << p.second << ")";
}

template <typename T1, typename T2>
testing::AssertionResult & operator<<(testing::AssertionResult & result,
                                      const std::tuple<T1, T2> & p) {
    return result << "(" << std::get<0>(p) << "," << std::get<1>(p) << ")";
}

template <typename T>
testing::AssertionResult & operator<<(testing::AssertionResult & result,
                                      const std::vector<T> & v) {
    auto it = v.begin();
    if(it == v.end()) return result;
    result << *it;
    ++it;
    while(it != v.end()) {
        result << "," << *it;
        ++it;
    }
    return result;
}

template <std::ranges::range R>
testing::AssertionResult SIZE(R && r, auto expected_size) {
    const auto size = std::ranges::distance(r);
    if(size > 0) {
        return ::testing::AssertionFailure()
               << "size is " << size << ", but expected to be "
               << expected_size;
    }
    return ::testing::AssertionSuccess();
}
template <std::ranges::range R>
testing::AssertionResult EMPTY(R && r) {
    return SIZE(std::forward<R>(r), 0);
}

template <typename R1, typename R2>
testing::AssertionResult EQ_RANGES(R1 && r1, R2 && r2) {
    const auto r1_size = std::ranges::distance(r1);
    const auto r2_size = std::ranges::distance(r2);
    if(r1_size != r2_size) {
        return ::testing::AssertionFailure()
               << "ranges sizes differ: " << r1_size << " != " << r2_size;
    }
    std::size_t pos = 0;
    auto it1 = r1.begin();
    auto it2 = r2.begin();
    while(it1 != r1.end() && it2 != r2.end()) {
        if(*it1 != *it2) {
            return ::testing::AssertionFailure()
                   << "ranges values differ at pos " << pos << ": " << *it1
                   << " != " << *it2;
        }
        ++it1;
        ++it2;
        ++pos;
    }
    return ::testing::AssertionSuccess();
}

template <typename R, typename T>
    requires std::convertible_to<T, std::ranges::range_value_t<R>>
testing::AssertionResult EQ_RANGES(R && r1, std::initializer_list<T> l) {
    auto r2 = std::ranges::to<std::vector<std::ranges::range_value_t<R>>>(std::views::transform(l, [](const auto & e) {
        return static_cast<std::ranges::range_value_t<R>>(e);
    }));
    return EQ_RANGES(r1, r2);
}

template <typename R1, typename R2>
    requires std::same_as<std::ranges::range_value_t<R1>,
                          std::ranges::range_value_t<R2>>
testing::AssertionResult EQ_MULTISETS(R1 && r1, R2 && r2) {
    using vector_t = std::vector<std::ranges::range_value_t<R1>>;

    vector_t s1 = std::ranges::to<vector_t>(r1);
    std::ranges::sort(s1);

    vector_t s2 = std::ranges::to<vector_t>(r2);
    std::ranges::sort(s2);

    vector_t s1_minus_s2;
    std::ranges::set_difference(s1, s2, std::back_inserter(s1_minus_s2));
    vector_t s2_minus_s1;
    std::ranges::set_difference(s2, s1, std::back_inserter(s2_minus_s1));

    if(s1_minus_s2.empty() && s2_minus_s1.empty()) {
        return ::testing::AssertionSuccess();
    }
    auto result = ::testing::AssertionFailure();
    if(!s1_minus_s2.empty()) {
        result << "s1\\s2 = {" << s1_minus_s2 << "}";
    }
    if(!s2_minus_s1.empty()) {
        if(!s1_minus_s2.empty()) result << " and ";
        result << "s2\\s1 = {" << s2_minus_s1 << "}";
    }
    return result;
}

template <typename R, typename T>
    requires std::convertible_to<T, std::ranges::range_value_t<R>>
testing::AssertionResult EQ_MULTISETS(R && r1, std::initializer_list<T> l) {
    auto r2 = std::ranges::to<std::vector<std::ranges::range_value_t<R>>>(std::views::transform(l, [](const auto & e) {
        return static_cast<std::ranges::range_value_t<R>>(e);
    }));
    return EQ_MULTISETS(r1, r2);
}

#endif  // RANGES_TEST_HELPER_HPP