#undef NDEBUG
#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <utility>

#include "melon/mapping.hpp"
#include "melon/maps/constant.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// true_map and false_map are aliases of maps::constant, and aliases are
// transparent -- what keeps views::subgraph's same_as<..., true_map>
// specializations selected
////////////////////////////////////////////////////////////////////////////////

static_assert(std::same_as<maps::true_map, maps::constant<true>>);
static_assert(std::same_as<maps::false_map, maps::constant<false>>);

static_assert(mapping_view<maps::true_map, int>);
static_assert(mapping_view<maps::false_map, int>);
static_assert(mapping_view<maps::constant<1>, int>);

// The constant is an NTTP, so the map is an empty type -- the
// [[no_unique_address]] guarantee views::subgraph's unfiltered
// specializations rely on.
static_assert(std::is_empty_v<maps::true_map>);
static_assert(std::is_empty_v<maps::constant<1>>);

// The subscript returns a prvalue of the value type, so a maps::constant is
// readable but never writable -- exactly what output_mapping's disequality
// guard rejects.
static_assert(mapping_of<maps::true_map, int, bool>);
static_assert(mapping_of<maps::constant<1>, int, int>);
static_assert(!output_mapping<maps::true_map, int>);
static_assert(!output_mapping<maps::constant<1>, int>);

static_assert(noexcept(maps::constant<1>{}[0]));

GTEST_TEST(constant, answers_for_any_key) {
    static_assert(maps::true_map{}[0]);
    static_assert(!maps::false_map{}[0]);
    static_assert(maps::constant<7>{}[nullptr] == 7);

    // Any key type, which is what makes them usable as the default filter
    // maps of the algorithms and views.
    ASSERT_TRUE(maps::true_map{}[std::string("anything")]);
    ASSERT_FALSE(maps::false_map{}[std::make_pair(1, 2)]);
    ASSERT_EQ(maps::constant<'x'>{}[std::string("key")], 'x');
}
