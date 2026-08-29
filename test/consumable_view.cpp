#undef NDEBUG
#include <gtest/gtest.h>

#include <span>

#include <ranges>
#include <vector>

#include "melon/detail/consumable_view.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// detail::consumable_view adapts a range into an empty()/current()/advance()
// cursor
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(consumable_view, test_std_iota_view) {
    auto v = std::views::iota(0, 9);
    auto r = detail::consumable_view(v);
    std::vector<int> took;
    for(; !r.empty(); r.advance()) took.push_back(r.current());
    ASSERT_TRUE(EQ_RANGES(took, {0, 1, 2, 3, 4, 5, 6, 7, 8}));
}

GTEST_TEST(consumable_view, test_vector_manual_loop) {
    std::vector<bool> filter(9, false);
    std::vector<unsigned int> v = {7u, 5u, 4u, 3u, 2u, 6u, 8u, 1u};

    auto r = detail::consumable_view(v);
    std::vector<unsigned int> took;

    for(; !r.empty(); r.advance()) {
        auto i = r.current();
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
        break;
    }
    for(; !r.empty(); r.advance()) {
        auto i = r.current();
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
        break;
    }
    ASSERT_TRUE(EQ_RANGES(took, {7u, 5u}));

    took.resize(0);
    for(; !r.empty(); r.advance()) {
        auto i = r.current();
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
    }
    ASSERT_TRUE(EQ_RANGES(took, {4u, 3u, 2u, 6u, 8u, 1u}));
}

////////////////////////////////////////////////////////////////////////////////
// consumption persists across loops: every new begin() resumes at the cursor
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(consumable_view, test_vector_iterator_loop) {
    std::vector<bool> filter(9, false);
    std::vector<unsigned int> v = {7u, 5u, 4u, 3u, 2u, 6u, 8u, 1u};

    auto r = detail::consumable_view(v);
    std::vector<unsigned int> took;

    for(auto it = std::begin(r); it != std::end(r); ++it) {
        const auto i = *it;
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
        break;
    }
    for(auto it = std::begin(r); it != std::end(r); ++it) {
        const auto i = *it;
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
        break;
    }
    ASSERT_TRUE(EQ_RANGES(took, {7u, 5u}));

    took.resize(0);
    for(auto it = std::begin(r); it != std::end(r); ++it) {
        const auto i = *it;
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
    }
    ASSERT_TRUE(EQ_RANGES(took, {4u, 3u, 2u, 6u, 8u, 1u}));
}

GTEST_TEST(consumable_view, test_vector_for_loop) {
    std::vector<bool> filter(9, false);
    std::vector<unsigned int> v = {7u, 5u, 4u, 3u, 2u, 6u, 8u, 1u};

    auto r = detail::consumable_view(v);
    std::vector<unsigned int> took;

    for(auto i : r) {
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
        break;
    }
    for(auto i : r) {
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
        break;
    }
    ASSERT_TRUE(EQ_RANGES(took, {7u, 5u}));

    took.resize(0);
    for(auto i : r) {
        if(filter[i]) continue;
        filter[i] = true;
        took.push_back(i);
    }
    ASSERT_TRUE(EQ_RANGES(took, {4u, 3u, 2u, 6u, 8u, 1u}));
}

////////////////////////////////////////////////////////////////////////////////
// assigning a new range rewinds the cursor to its begin and returns *this
////////////////////////////////////////////////////////////////////////////////

// regression: an operator=(R &) that falls off the end of a non-void function
// is a hard error ("no return statement in constexpr function") the moment it
// is instantiated -- the overload is simply unusable, in both
// specializations.
GTEST_TEST(consumable_view, assign_from_range_returns_self) {
    std::vector<unsigned int> v = {3u, 1u, 4u};
    std::vector<unsigned int> w = {9u, 2u};

    using view_type =
        detail::consumable_view<std::views::all_t<std::vector<unsigned int> &>>;
    view_type cv(v);
    cv.advance();
    ASSERT_EQ(cv.current(), 1u);

    std::views::all_t<std::vector<unsigned int> &> rw(w);
    auto & self = (cv = rw);

    ASSERT_EQ(std::addressof(self), std::addressof(cv));
    ASSERT_FALSE(cv.empty());
    ASSERT_EQ(cv.current(), 9u);  // reassignment rewinds to the new begin
    cv.advance();
    ASSERT_EQ(cv.current(), 2u);
    cv.advance();
    ASSERT_TRUE(cv.empty());
}

////////////////////////////////////////////////////////////////////////////////
// rewind() restarts the range already held, whether owned or borrowed
////////////////////////////////////////////////////////////////////////////////

// Callers that restart a run need rewind() rather than an assignment: the
// range type may be move-only (std::ranges::owning_view is), so re-seeding
// from a saved copy is not an option.
GTEST_TEST(consumable_view, rewind_owning_range) {
    // owning_view is neither borrowed nor copyable: the primary template, which
    // owns the range and can ask it for begin() again -- so rewinding costs it
    // no extra state at all
    auto cv = detail::consumable_view(std::vector<unsigned int>{3u, 1u, 4u});
#ifndef _MSC_VER
    // Not pinned under MSVC: [[msvc::no_unique_address]] never overlaps an
    // empty member with a preceding non-empty base class, so the no_state
    // member costs 8 bytes there and only there.
    static_assert(sizeof(cv) ==
                  sizeof(detail::consumable_input_view<
                         std::views::all_t<std::vector<unsigned int>>>));
#endif
    static_assert(!std::copy_constructible<decltype(cv)>);

    cv.advance();
    cv.advance();
    ASSERT_EQ(cv.current(), 4u);
    cv.advance();
    ASSERT_TRUE(cv.empty());

    cv.rewind();
    ASSERT_FALSE(cv.empty());
    ASSERT_TRUE(EQ_RANGES(cv, {3u, 1u, 4u}));
}

GTEST_TEST(consumable_view, rewind_borrowed_range) {
    // iota_view is borrowed: the specialization, which keeps only iterators
    auto cv = detail::consumable_view(std::views::iota(0, 3));

    cv.advance();
    cv.advance();
    cv.advance();
    ASSERT_TRUE(cv.empty());

    cv.rewind();
    ASSERT_FALSE(cv.empty());
    ASSERT_EQ(cv.current(), 0);
    ASSERT_TRUE(EQ_RANGES(cv, {0, 1, 2}));

    // rewinding a partly consumed view is the same operation
    cv.rewind();
    cv.advance();
    cv.rewind();
    ASSERT_EQ(cv.current(), 0);
}

////////////////////////////////////////////////////////////////////////////////
// detail::consumable_input_view is the minimal cursor: one traversal's state,
// any input range, re-seedable from outside
////////////////////////////////////////////////////////////////////////////////

// The rewindable variant is the exception, not the default: a cursor kept per
// vertex or per stack frame never restarts itself in place, and paying for a
// remembered begin() there costs an iterator apiece for nothing. Only a
// borrowed range makes detail::consumable_view wider than
// detail::consumable_input_view, and that is precisely the shape those hot
// cursors have.
GTEST_TEST(consumable_input_view, costs_no_more_than_one_traversal_needs) {
    using span_type = std::span<const unsigned int>;
    static_assert(std::ranges::borrowed_range<span_type>);

    // two pointers, an iterator and a sentinel, and nothing else
    static_assert(sizeof(detail::consumable_input_view<span_type>) ==
                  2 * sizeof(unsigned int *));
    // the rewindable one is strictly wider here, which is why it is not what
    // dinitz, depth_first_search and strongly_connected_components use
    static_assert(sizeof(detail::consumable_view<span_type>) >
                  sizeof(detail::consumable_input_view<span_type>));
}

// Restarting is exactly what an input range cannot promise, so
// detail::consumable_view does not offer it for one;
// detail::consumable_input_view still walks it happily.
GTEST_TEST(consumable_input_view, accepts_an_input_only_range) {
    auto input_only = std::views::iota(0, 5) |
                      std::views::filter([](int i) { return i % 2 == 0; }) |
                      std::views::transform([](int i) { return i; });
    using input_view = decltype(std::views::all(input_only));

    static_assert(std::ranges::input_range<input_view>);
    static_assert(requires { detail::consumable_input_view<input_view>{}; });

    auto cv = detail::consumable_input_view(input_only);
    ASSERT_FALSE(cv.empty());
    ASSERT_EQ(cv.current(), 0);
    cv.advance();
    ASSERT_EQ(cv.current(), 2);
}

// The re-seeding path the per-vertex cursors actually use: hand it the range
// again, which the graph can always produce. dinitz::reset() does exactly this
// with `_remaining_out_arcs[u] = out_arcs(_graph, u)`.
GTEST_TEST(consumable_input_view, reseeds_from_an_externally_held_range) {
    std::vector<unsigned int> v = {3u, 1u, 4u};
    using view_type = detail::consumable_input_view<
        std::views::all_t<std::vector<unsigned int> &>>;

    view_type cv(v);
    cv.advance();
    cv.advance();
    cv.advance();
    ASSERT_TRUE(cv.empty());

    std::views::all_t<std::vector<unsigned int> &> again(v);
    cv = again;
    ASSERT_FALSE(cv.empty());
    ASSERT_TRUE(EQ_RANGES(cv, {3u, 1u, 4u}));
}

////////////////////////////////////////////////////////////////////////////////
// a default-constructed cursor is *disengaged*: it supports destruction and
// assignment only, and re-seeding engages it. The state exists so per-vertex
// cursor maps can default-construct their slots even when the range -- a
// filter_view over a capturing lambda, every filtered subgraph's incidence
// shape -- cannot be default-constructed. dinitz over any filtered subgraph
// is unconstructible without it.
////////////////////////////////////////////////////////////////////////////////

namespace disengaged_probes {
inline constexpr auto pred = [threshold = 2u](const unsigned int i) {
    return i > threshold;
};
using filtered = decltype(std::views::filter(
    std::declval<std::vector<unsigned int> &>(), pred));
static_assert(!std::default_initializable<filtered>);
static_assert(!std::ranges::borrowed_range<filtered>);
}  // namespace disengaged_probes

GTEST_TEST(consumable_input_view, default_constructed_is_assignment_only) {
    using namespace disengaged_probes;
    static_assert(
        std::default_initializable<detail::consumable_input_view<filtered>>);

    std::vector<unsigned int> v = {3u, 1u, 4u, 1u};
    detail::consumable_input_view<filtered> cv;
    cv = std::views::filter(v, pred);
    std::vector<unsigned int> took;
    for(; !cv.empty(); cv.advance()) took.push_back(cv.current());
    ASSERT_TRUE(EQ_RANGES(took, {3u, 4u}));
}

// Copying or moving a disengaged cursor must yield another disengaged cursor:
// static_map and std::vector relocate their slots wholesale, so a reseek
// through the (absent) range would fault before any slot is ever seeded.
GTEST_TEST(consumable_input_view, disengaged_cursor_survives_relocation) {
    using namespace disengaged_probes;

    detail::consumable_input_view<filtered> a;
    detail::consumable_input_view<filtered> b(std::move(a));
    detail::consumable_input_view<filtered> c(b);
    detail::consumable_input_view<filtered> d;
    d = std::move(c);

    // the relocated cursor is still assignable and usable
    std::vector<unsigned int> v = {5u, 2u, 7u};
    d = std::views::filter(v, pred);
    ASSERT_FALSE(d.empty());
    ASSERT_EQ(d.current(), 5u);
    d.advance();
    ASSERT_EQ(d.current(), 7u);
    d.advance();
    ASSERT_TRUE(d.empty());
}

////////////////////////////////////////////////////////////////////////////////
// the single-pass cursor iterator is a C++20 input iterator and advertises no
// Cpp17 category
////////////////////////////////////////////////////////////////////////////////

// iterator_concept, and no iterator_category -- `*it++` is void, so any Cpp17
// category the typedef advertised would be unmeetable.
namespace {
template <typename T>
constexpr bool advertises_cpp17_category_cv =
    requires { typename std::iterator_traits<T>::iterator_category; };
}  // namespace
using consumable_it = std::ranges::iterator_t<
    detail::consumable_input_view<std::span<const int>>>;
static_assert(std::input_iterator<consumable_it>);
static_assert(std::same_as<typename consumable_it::iterator_concept,
                           std::input_iterator_tag>);
static_assert(!advertises_cpp17_category_cv<consumable_it>);

////////////////////////////////////////////////////////////////////////////////
// detail::consumable_iterator forwards noexcept-ness instead of claiming it
////////////////////////////////////////////////////////////////////////////////

// regression: increment and both comparisons forward into an arbitrary
// wrapped iterator, so an unconditional noexcept on them turns a throwing
// increment into std::terminate.
namespace {
struct throwing_iterator {
    using value_type = int;
    using difference_type = std::ptrdiff_t;
    int operator*() const noexcept(false) { return 0; }
    throwing_iterator & operator++() noexcept(false) { return *this; }
    void operator++(int) noexcept(false) {}
    bool operator==(const throwing_iterator &) const noexcept(false) {
        return true;
    }
};
}  // namespace

GTEST_TEST(consumable_iterator, noexcept_follows_the_wrapped_iterator) {
    static_assert(std::input_iterator<throwing_iterator>);
    using CI =
        detail::consumable_iterator<throwing_iterator, throwing_iterator>;
    static_assert(!noexcept(++std::declval<CI &>()));
    static_assert(!noexcept(std::declval<CI &>()++));
    static_assert(
        !noexcept(std::declval<const CI &>() == std::declval<const CI &>()));
    static_assert(!noexcept(std::declval<const CI &>() ==
                            std::declval<const throwing_iterator &>()));

    // Positive control: nothrow wrapped iterators keep the guarantee.
    using PI = detail::consumable_iterator<const int *, const int *>;
    static_assert(noexcept(++std::declval<PI &>()));
    static_assert(
        noexcept(std::declval<const PI &>() == std::declval<const PI &>()));
    SUCCEED();
}

////////////////////////////////////////////////////////////////////////////////
// iterating an owning cursor advances through the view, so the relocation
// counter stays in step: a moved cursor resumes where iteration stopped
// instead of silently rewinding
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(consumable_input_view, iteration_keeps_the_relocation_counter) {
    detail::consumable_input_view cursor(std::vector<int>{10, 11, 12, 13});
    auto it = cursor.begin();
    ++it;
    ++it;
    ASSERT_EQ(cursor.consumed(), 2u);
    ASSERT_EQ(cursor.current(), 12);
    auto moved = std::move(cursor);
    ASSERT_EQ(moved.current(), 12);
}
