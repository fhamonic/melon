#undef NDEBUG
#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "melon/detail/intrusive_view.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

GTEST_TEST(intrusive_range, test) {
    std::vector<int> values = {1, 2, 6, 3, 7};

    auto r = intrusive_view(
        std::size_t{0u},
        [&values](const std::size_t i) -> int & { return values[i]; },
        [](const std::size_t i) -> std::size_t { return i + 1; },
        [n = values.size()](const std::size_t i) -> std::size_t {
            return i < n;
        });

    auto it = r.begin();
    auto end = r.end();

    using R = decltype(r);
    using I = decltype(it);
    using S = decltype(end);

    static_assert(requires(I i) {
        typename std::iter_difference_t<I>;
        { ++i } -> std::same_as<I &>;
        i++;
    });
    {
        {
            {
                static_assert(std::movable<I>);
                static_assert(std::weakly_incrementable<I>);
                static_assert(std::input_or_output_iterator<I>);
            }
            static_assert(requires(const I in) {
                typename std::iter_value_t<I>;
                typename std::iter_reference_t<I>;
                typename std::iter_rvalue_reference_t<I>;
                { *in } -> std::same_as<std::iter_reference_t<I>>;
                {
                    std::ranges::iter_move(in)
                } -> std::same_as<std::iter_rvalue_reference_t<I>>;
            });
            static_assert(std::indirectly_readable<I>);
            static_assert(std::input_iterator<I>);
        }
        static_assert(std::semiregular<S>);
        static_assert(std::sentinel_for<S, I>);
        static_assert(std::ranges::range<R>);
        static_assert(std::ranges::input_range<R>);
    }
    static_assert(std::movable<R>);
    static_assert(std::ranges::viewable_range<R>);

    // static_assert(std::forward_iterator<I>);
    // static_assert(std::ranges::forward_range<R>);

    ASSERT_FALSE(it == end);
    ASSERT_EQ(*it, 1);
    ++it;
    ASSERT_FALSE(it == end);
    ASSERT_EQ(*it, 2);
    ++it;
    ASSERT_FALSE(it == end);
    ASSERT_EQ(*it, 6);
    ++it;
    ASSERT_FALSE(it == end);
    ASSERT_EQ(*it, 3);
    ++it;
    ASSERT_FALSE(it == end);
    ASSERT_EQ(*it, 7);
    ++it;
    ASSERT_TRUE(it == end);
}

// ############ regression: CTAD with named (lvalue) functors #################

// The constructor took `Deref &&` etc. directly. Those are *class* template
// parameters, so they form plain rvalue references, not forwarding references,
// and only inline temporaries could be passed:
//   error: cannot bind rvalue reference of type 'lambda&&' to lvalue
GTEST_TEST(intrusive_view, accepts_named_lvalue_functors) {
    std::vector<int> values = {1, 2, 6, 3, 7};

    auto deref = [&values](const std::size_t i) -> int & { return values[i]; };
    auto incr = [](const std::size_t i) -> std::size_t { return i + 1; };
    auto cond = [n = values.size()](const std::size_t i) -> bool {
        return i < n;
    };

    auto from_lvalues = intrusive_view(std::size_t{0u}, deref, incr, cond);
    ASSERT_TRUE(EQ_RANGES(from_lvalues, values));

    // a const lvalue functor works too
    const auto & cderef = deref;
    auto from_const = intrusive_view(std::size_t{0u}, cderef, incr, cond);
    ASSERT_TRUE(EQ_RANGES(from_const, values));

    // rvalues keep working, and both spellings land on the same type
    auto from_rvalues = intrusive_view(
        std::size_t{0u},
        [&values](const std::size_t i) -> int & { return values[i]; },
        [](const std::size_t i) -> std::size_t { return i + 1; },
        [n = values.size()](const std::size_t i) -> bool { return i < n; });
    ASSERT_TRUE(EQ_RANGES(from_rvalues, values));

    // the guide decays, so nothing dangles or stores a reference
    static_assert(std::movable<decltype(from_lvalues)>);
    static_assert(std::ranges::input_range<decltype(from_lvalues)>);
}

// ################## regression: noexcept honesty ############################

// operator*, operator++ and operator==(sentinel) were unconditionally noexcept
// while invoking user-supplied functors: a throwing functor called
// std::terminate instead of propagating.
namespace {
struct throwing_deref {
    int operator()(const std::size_t i) const {
        if(i > 2) throw std::runtime_error("boom");
        return static_cast<int>(i);
    }
};
struct nothrow_deref {
    int operator()(const std::size_t i) const noexcept {
        return static_cast<int>(i);
    }
};
constexpr auto nothrow_incr = [](const std::size_t i) noexcept {
    return i + 1;
};
constexpr auto nothrow_cond = [](const std::size_t i) noexcept {
    return i < 5;
};

using throwing_view_t = decltype(intrusive_view(
    std::size_t{0}, throwing_deref{}, nothrow_incr, nothrow_cond));
using nothrow_view_t = decltype(intrusive_view(std::size_t{0}, nothrow_deref{},
                                               nothrow_incr, nothrow_cond));
using throwing_iterator = decltype(std::declval<throwing_view_t>().begin());
using nothrow_iterator = decltype(std::declval<nothrow_view_t>().begin());
}  // namespace

static_assert(!noexcept(*std::declval<const throwing_iterator &>()));
static_assert(noexcept(*std::declval<const nothrow_iterator &>()));
static_assert(noexcept(++std::declval<nothrow_iterator &>()));
static_assert(noexcept(std::declval<nothrow_iterator &>()++));
static_assert(noexcept(std::declval<const nothrow_iterator &>() ==
                       nothrow_view_t::sentinel{}));

GTEST_TEST(intrusive_view, throwing_functor_propagates) {
    auto view = intrusive_view(std::size_t{0}, throwing_deref{}, nothrow_incr,
                               nothrow_cond);
    auto it = view.begin();
    ASSERT_EQ(*it, 0);
    ++it;
    ++it;
    ++it;
    ASSERT_THROW((void)*it, std::runtime_error);  // used to std::terminate
}
