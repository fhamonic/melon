#undef NDEBUG
#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/mapping.hpp"
#include "melon/maps/transform.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// the base goes through mapping_all: an lvalue is referenced, an rvalue is
// owned, an existing view is passed through
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(transform, selects_the_right_base_adaptor) {
    std::vector<int> v{1, 2, 3};
    auto twice = [](int x) { return 2 * x; };

    auto from_lvalue = maps::transform(v, twice);
    static_assert(std::same_as<decltype(from_lvalue.base()),
                               mapping_ref_view<std::vector<int>> &>);
    ASSERT_EQ(std::addressof(from_lvalue.base().base()), std::addressof(v));
    ASSERT_EQ(from_lvalue[1u], 4);

    auto from_rvalue = maps::transform(std::vector<int>{4, 5}, twice);
    static_assert(std::same_as<decltype(from_rvalue.base()),
                               mapping_owning_view<std::vector<int>> &>);
    ASSERT_EQ(from_rvalue[1u], 10);

    // An existing view is passed through rather than wrapped again --
    // including another transform, so transforms compose.
    auto composed = maps::transform(maps::transform(v, twice), twice);
    static_assert(
        std::same_as<decltype(composed.base()), decltype(from_lvalue) &>);
    ASSERT_EQ(composed[2u], 12);
}

static_assert(mapping_view<decltype(maps::transform(std::vector<int>{},
                                                    [](int x) { return x; })),
                           unsigned>);

////////////////////////////////////////////////////////////////////////////////
// a prvalue-returning projection is readable only; a reference-returning one
// keeps the base's writability -- and the ref-view base stays shallow const
// while an owned base is deep const
////////////////////////////////////////////////////////////////////////////////

namespace {
// The const overload is what lets a deep-const (owned) base stay readable; a
// single `pair & -> int &` signature would erase the const subscript
// entirely.
struct second_ref {
    int & operator()(std::pair<int, int> & p) const { return p.second; }
    const int & operator()(const std::pair<int, int> & p) const {
        return p.second;
    }
};
}  // namespace

static_assert(!output_mapping<decltype(maps::transform(
                                  std::vector<int>{}, [](int x) { return x; })),
                              unsigned>);

GTEST_TEST(transform, reference_projection_writes_through_a_ref_base) {
    std::vector<std::pair<int, int>> v{{1, 10}, {2, 20}};
    const auto tm = maps::transform(v, second_ref{});

    // Shallow const, inherited from the mapping_ref_view base: the write
    // lands in the underlying vector even through a const view object.
    static_assert(output_mapping_of<decltype(tm), unsigned, int>);
    tm[1u] = 21;
    ASSERT_EQ(v[1].second, 21);
}

GTEST_TEST(transform, owned_base_is_deep_const) {
    auto tm = maps::transform(
        std::vector<std::pair<int, int>>{{1, 10}, {2, 20}}, second_ref{});

    tm[0u] = 11;  // lands in the owned copy
    ASSERT_EQ(tm[0u], 11);

    const auto & const_tm = tm;
    static_assert(std::same_as<decltype(const_tm[0u]), const int &>);
    ASSERT_EQ(const_tm[0u], 11);
}

////////////////////////////////////////////////////////////////////////////////
// the factory is SFINAE-friendly, and the guarded subscripts answer the
// concepts instead of hard-erroring -- same contract as mapping_owning_view
////////////////////////////////////////////////////////////////////////////////

namespace {
struct not_a_mapping {
    not_a_mapping(const not_a_mapping &) = delete;
    not_a_mapping(not_a_mapping &&) = delete;
};

template <typename M, typename F>
concept can_transform = requires(M && m, F && f) {
    maps::transform(std::forward<M>(m), std::forward<F>(f));
};

using proj_t = decltype([](int x) { return x; });
}  // namespace

static_assert(can_transform<std::vector<int> &, proj_t>);
static_assert(can_transform<std::vector<int>, proj_t>);
static_assert(!can_transform<not_a_mapping, proj_t>);

// A mutable *generic* lambda slips past the factory's static_assert (the
// detector abstains on templates), so the subscript guards are the real gate:
// the const overload vanishes and the concepts answer false instead of
// hard-erroring.
namespace {
using mutable_generic_transform_t = decltype(maps::transform(
    std::vector<int>{}, [](auto x) mutable { return x; }));
}  // namespace

static_assert(!mapping<mutable_generic_transform_t, unsigned>);

////////////////////////////////////////////////////////////////////////////////
// the view stays movable over a capturing lambda, and noexcept follows the
// base subscript and the projection
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(transform, stays_movable_over_a_capturing_lambda) {
    int offset = 5;
    auto tm = maps::transform(std::vector<int>{1, 2},
                              [offset](int x) { return x + offset; });
    static_assert(std::movable<decltype(tm)>);

    auto moved = std::move(tm);
    ASSERT_EQ(moved[1u], 7);
}

namespace {
using noexcept_proj_transform_t = decltype(maps::transform(
    maps::identity{}, [](int x) noexcept { return x + 1; }));
using throwing_proj_transform_t =
    decltype(maps::transform(maps::identity{}, [](int x) { return x + 1; }));
}  // namespace

static_assert(noexcept(std::declval<const noexcept_proj_transform_t &>()[3]));
static_assert(!noexcept(std::declval<const throwing_proj_transform_t &>()[3]));
