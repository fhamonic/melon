#undef NDEBUG
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/mapping.hpp"

using namespace melon;

// ############################### concepts ###############################

static_assert(mapping<std::vector<int>, std::size_t>);
static_assert(!mapping<std::vector<int>, std::string>);
static_assert(std::same_as<mapped_value_t<std::vector<int>, std::size_t>, int>);
static_assert(
    std::same_as<mapped_reference_t<std::vector<int> &, std::size_t>, int &>);
static_assert(
    std::same_as<mapped_const_reference_t<std::vector<int>, std::size_t>,
                 const int &>);

static_assert(input_mapping<std::vector<int>, std::size_t>);
static_assert(input_mapping_of<std::vector<int>, std::size_t, int>);
static_assert(!input_mapping_of<std::vector<int>, std::size_t, double>);
static_assert(output_mapping<std::vector<int>, std::size_t>);
static_assert(output_mapping_of<std::vector<int>, std::size_t, int>);

// std::vector<bool> subscripts to a proxy, which still assigns through, so it
// models the output protocol; a const vector is read-only.
static_assert(input_mapping_of<std::vector<bool>, std::size_t, bool>);
static_assert(output_mapping_of<std::vector<bool>, std::size_t, bool>);
static_assert(!contiguous_mapping<std::vector<bool>, std::size_t>);
static_assert(input_mapping_of<const std::vector<int>, std::size_t, int>);
static_assert(!output_mapping<const std::vector<int>, std::size_t>);

static_assert(contiguous_mapping<std::vector<int>, std::size_t>);
static_assert(contiguous_mapping_of<std::vector<int>, std::size_t, int>);
// A std::map subscripts fine but has no data(), and its key is not integral.
static_assert(mapping<std::map<std::string, int>, std::string>);
static_assert(!contiguous_mapping<std::map<std::size_t, int>, std::size_t>);

// A raw mapping is not a view; the two adaptors and the canned maps are.
static_assert(!mapping_view<std::vector<int>, std::size_t>);
static_assert(mapping_view<mapping_ref_view<std::vector<int>>, std::size_t>);
static_assert(mapping_view<views::true_map, int>);
static_assert(mapping_view<views::false_map, int>);
static_assert(mapping_view<views::identity_map, int>);
static_assert(mapping_view<views::element_map<0>, std::pair<int, int>>);

// ############################## ref view ################################

// Shallow const, like std::ranges::ref_view: constness rides on _Map, so a
// const view over a mutable map still writes through.
static_assert(output_mapping_of<const mapping_ref_view<std::vector<int>>,
                                std::size_t, int>);
static_assert(
    !output_mapping<mapping_ref_view<const std::vector<int>>, std::size_t>);
static_assert(input_mapping_of<mapping_ref_view<const std::vector<int>>,
                               std::size_t, int>);
static_assert(std::copyable<mapping_ref_view<std::vector<int>>>);

GTEST_TEST(mapping_ref_view, reads_and_writes_through) {
    std::vector<int> v{1, 2, 3};
    const mapping_ref_view view(v);

    ASSERT_EQ(std::addressof(view.base()), std::addressof(v));
    ASSERT_EQ(view[1u], 2);

    view[1u] = 20;
    ASSERT_EQ(v[1], 20);
    ASSERT_EQ(view.data(), v.data());
}

// It must bind to lvalues only, so it can never dangle on a temporary.
static_assert(std::constructible_from<mapping_ref_view<std::vector<int>>,
                                      std::vector<int> &>);
static_assert(!std::constructible_from<mapping_ref_view<std::vector<int>>,
                                       std::vector<int>>);

// ############################# owning view ##############################

GTEST_TEST(mapping_owning_view, owns_its_map) {
    mapping_owning_view view{std::vector<int>{1, 2, 3}};

    ASSERT_EQ(view[2u], 3);
    view[2u] = 30;
    ASSERT_EQ(view.base()[2], 30);

    // Deep const, like std::ranges::owning_view.
    const auto & const_view = view;
    static_assert(std::same_as<decltype(const_view[2u]), const int &>);
    ASSERT_EQ(const_view[2u], 30);
}

GTEST_TEST(mapping_owning_view, survives_a_move) {
    mapping_owning_view view{std::vector<int>{1, 2, 3}};
    auto moved = std::move(view);
    ASSERT_EQ(moved[0u], 1);

    std::vector<int> recovered = std::move(moved).base();
    ASSERT_EQ(recovered.size(), 3u);
}

// ########################### the lambda case ############################

// A capturing lambda is move-constructible but not assignable. The owning view
// stores it in a movable-box so that it stays std::movable anyway, which is
// what lets algorithms hold a `views::map(...)` by value.
GTEST_TEST(mapping_owning_view, keeps_a_lambda_assignable) {
    int offset = 10;
    auto lambda = [offset](std::size_t i) { return offset + int(i); };
    using lambda_t = decltype(lambda);

    static_assert(!std::is_copy_assignable_v<lambda_t>);
    static_assert(std::movable<mapping_owning_view<lambda_t>>);

    auto view = views::map(lambda);
    static_assert(std::movable<decltype(view)>);
    static_assert(mapping_view<decltype(view), std::size_t>);
    ASSERT_EQ(view[5u], 15);

    auto other = views::map([](std::size_t i) { return int(i) * 2; });
    static_assert(!std::same_as<decltype(other), decltype(view)>);

    auto reassigned = views::map(lambda);
    reassigned = views::map(lambda);
    ASSERT_EQ(reassigned[1u], 11);
}

// The three storage protocols a mapping may expose all reach the same
// subscript: operator[], operator() and at().
GTEST_TEST(mapping_views, dispatch_the_three_subscript_protocols) {
    struct call_op {
        int operator()(std::size_t i) const { return int(i) + 1; }
    };
    struct at_only {
        std::vector<int> v{7, 8, 9};
        int at(std::size_t i) const { return v[i]; }
    };

    ASSERT_EQ(views::map(call_op{})[3u], 4);

    at_only m;
    ASSERT_EQ(mapping_ref_view(m)[1u], 8);
}

// ############################# mapping_all ##############################

GTEST_TEST(mapping_all, selects_the_right_adaptor) {
    std::vector<int> v{1, 2, 3};

    auto from_lvalue = views::mapping_all(v);
    static_assert(std::same_as<decltype(from_lvalue),
                               mapping_ref_view<std::vector<int>>>);
    ASSERT_EQ(std::addressof(from_lvalue.base()), std::addressof(v));

    auto from_rvalue = views::mapping_all(std::vector<int>{4, 5});
    static_assert(std::same_as<decltype(from_rvalue),
                               mapping_owning_view<std::vector<int>>>);
    ASSERT_EQ(from_rvalue[1u], 5);

    // An existing view is passed through rather than wrapped again.
    auto passed_through = views::mapping_all(views::true_map{});
    static_assert(std::same_as<decltype(passed_through), views::true_map>);

    static_assert(std::same_as<views::mapping_all_t<std::vector<int> &>,
                               mapping_ref_view<std::vector<int>>>);
    static_assert(std::same_as<views::mapping_all_t<std::vector<int>>,
                               mapping_owning_view<std::vector<int>>>);
}

// ############################# canned maps ##############################

GTEST_TEST(canned_maps, constant_and_identity) {
    static_assert(views::true_map{}[0]);
    static_assert(!views::false_map{}[0]);
    static_assert(views::identity_map{}[42] == 42);

    // They answer for any key type, which is what makes them usable as the
    // default filter/priority map of the algorithms.
    ASSERT_TRUE(views::true_map{}[std::string("anything")]);
    ASSERT_FALSE(views::false_map{}[std::make_pair(1, 2)]);
    ASSERT_EQ(views::identity_map{}[std::string("x")], "x");
}

GTEST_TEST(canned_maps, element_map_projects_tuple_elements) {
    const auto entry = std::make_pair(1, std::make_tuple(2, 3.5));

    using first = views::element_map<0>;
    using second_first = views::element_map<1, 0>;
    using second_second = views::element_map<1, 1>;

    ASSERT_EQ(first{}[entry], 1);
    ASSERT_EQ(second_first{}[entry], 2);
    ASSERT_EQ(second_second{}[entry], 3.5);

    static_assert(first{}[std::make_pair(4, 5)] == 4);
    static_assert(input_mapping_of<views::element_map<1>,
                                   std::pair<int, double>, double>);
}
