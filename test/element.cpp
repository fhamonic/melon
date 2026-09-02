#undef NDEBUG
#include <gtest/gtest.h>

#include <tuple>
#include <utility>
#include <variant>

#include "melon/mapping.hpp"
#include "melon/maps/element.hpp"

using namespace melon;

static_assert(mapping_view<maps::element<0>, std::pair<int, int>>);

GTEST_TEST(element, projects_tuple_elements) {
    const auto entry = std::make_pair(1, std::make_tuple(2, 3.5));

    using first = maps::element<0>;
    using second_first = maps::element<1, 0>;
    using second_second = maps::element<1, 1>;

    ASSERT_EQ(first{}[entry], 1);
    ASSERT_EQ(second_first{}[entry], 2);
    ASSERT_EQ(second_second{}[entry], 3.5);

    static_assert(first{}[std::make_pair(4, 5)] == 4);
    static_assert(mapping_of<maps::element<1>, std::pair<int, double>, double>);
}

////////////////////////////////////////////////////////////////////////////////
// maps::element's noexcept follows std::get, so variant access propagates
////////////////////////////////////////////////////////////////////////////////

// regression: std::get is noexcept for tuple/pair/array but throws
// bad_variant_access for std::variant, so an unconditional noexcept on the
// accessor chain turns that throw into std::terminate.
namespace {
using int_pair = std::pair<int, int>;
using nested_pair = std::pair<std::pair<int, int>, int>;
using int_variant = std::variant<int, double>;
using variant_pair = std::pair<std::variant<int, double>, int>;
}  // namespace

static_assert(noexcept(
    std::declval<const maps::element<1> &>()[std::declval<int_pair &>()]));
static_assert(
    noexcept(std::declval<
             const maps::element<0, 1> &>()[std::declval<nested_pair &>()]));
static_assert(!noexcept(
    std::declval<const maps::element<0> &>()[std::declval<int_variant &>()]));
static_assert(
    !noexcept(std::declval<
              const maps::element<0, 0> &>()[std::declval<variant_pair &>()]));

GTEST_TEST(element, variant_access_propagates_instead_of_terminating) {
    int_variant v{3.5};  // holds the double alternative
    maps::element<0> first;
    ASSERT_THROW((void)first[v], std::bad_variant_access);
}
