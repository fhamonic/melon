#undef NDEBUG
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/static_digraph.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/static_digraph_builder.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// a mapping is subscriptable by the key AND readable through a const access;
// output/contiguous refine what the subscript allows
////////////////////////////////////////////////////////////////////////////////

static_assert(mapping<std::vector<int>, std::size_t>);
static_assert(!mapping<std::vector<int>, std::string>);
static_assert(std::same_as<mapped_value_t<std::vector<int>, std::size_t>, int>);
static_assert(
    std::same_as<mapped_reference_t<std::vector<int> &, std::size_t>, int &>);
static_assert(
    std::same_as<mapped_const_reference_t<std::vector<int>, std::size_t>,
                 const int &>);

static_assert(mapping_of<std::vector<int>, std::size_t, int>);
static_assert(!mapping_of<std::vector<int>, std::size_t, double>);
static_assert(output_mapping<std::vector<int>, std::size_t>);
static_assert(output_mapping_of<std::vector<int>, std::size_t, int>);

// A subscript returning void reads nothing, so it is not a mapping.
struct void_subscript_map {
    void operator[](int) const {}
};
static_assert(!mapping<void_subscript_map, int>);

// std::vector<bool> subscripts to a proxy, which still assigns through, so it
// models the output protocol; a const vector is read-only.
static_assert(mapping_of<std::vector<bool>, std::size_t, bool>);
static_assert(output_mapping_of<std::vector<bool>, std::size_t, bool>);
static_assert(!contiguous_mapping<std::vector<bool>, std::size_t>);
static_assert(mapping_of<const std::vector<int>, std::size_t, int>);
static_assert(!output_mapping<const std::vector<int>, std::size_t>);

static_assert(contiguous_mapping<std::vector<int>, std::size_t>);
static_assert(contiguous_mapping_of<std::vector<int>, std::size_t, int>);
// std::map::operator[] inserts and cannot be offered const, so a raw std::map
// is not readable through a const access and models no mapping concept at
// all; it is used through maps::mapping_all, whose views subscript a const
// base via at().
static_assert(!mapping<std::map<std::string, int>, std::string>);
static_assert(!contiguous_mapping<std::map<std::size_t, int>, std::size_t>);

////////////////////////////////////////////////////////////////////////////////
// output_mapping means the write *lands*: the subscript must return an lvalue
// reference into storage, or a proxy standing in for one -- never a prvalue of
// the value type itself
////////////////////////////////////////////////////////////////////////////////

namespace {
// A computed map: `operator[]` returns a fresh value every call, so
// `m[k] = v` assigns into a temporary and the write is lost. It is a perfectly
// good *readable* mapping.
template <typename V>
struct computed_map {
    V operator[](int) const { return V{}; }
};
}  // namespace

static_assert(mapping_of<computed_map<std::string>, int, std::string>);
static_assert(!output_mapping<computed_map<std::string>, int>);
// The class-type case is the one that needs the disequality: for a scalar the
// requires-expression already rejects it, assignment to a scalar prvalue being
// ill-formed. Both must stay rejected.
static_assert(mapping_of<computed_map<int>, int, int>);
static_assert(!output_mapping<computed_map<int>, int>);

static_assert(!std::same_as<mapped_reference_t<std::vector<bool>, std::size_t>,
                            mapped_value_t<std::vector<bool>, std::size_t>>);
static_assert(output_mapping_of<std::vector<bool>, std::size_t, bool>);

////////////////////////////////////////////////////////////////////////////////
// only the adaptors and the canned maps are mapping_views; a raw mapping is
// not
////////////////////////////////////////////////////////////////////////////////

static_assert(!mapping_view<std::vector<int>, std::size_t>);
static_assert(mapping_view<mapping_ref_view<std::vector<int>>, std::size_t>);
static_assert(mapping_view<maps::true_map, int>);
static_assert(mapping_view<maps::false_map, int>);
static_assert(mapping_view<maps::identity_map, int>);
static_assert(mapping_view<maps::element_map<0>, std::pair<int, int>>);

////////////////////////////////////////////////////////////////////////////////
// mapping_ref_view is a shallow-const handle that binds to lvalues only
////////////////////////////////////////////////////////////////////////////////

// Shallow const, like std::ranges::ref_view: constness rides on Map, so a
// const view over a mutable map still writes through.
static_assert(output_mapping_of<const mapping_ref_view<std::vector<int>>,
                                std::size_t, int>);
static_assert(
    !output_mapping<mapping_ref_view<const std::vector<int>>, std::size_t>);
static_assert(
    mapping_of<mapping_ref_view<const std::vector<int>>, std::size_t, int>);
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

////////////////////////////////////////////////////////////////////////////////
// mapping_owning_view owns its map with deep const and survives moves
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// the owning view stays std::movable even over a non-assignable lambda
////////////////////////////////////////////////////////////////////////////////

// A capturing lambda is move-constructible but not assignable. The owning view
// stores it in a movable-box so that it stays std::movable anyway, which is
// what lets algorithms hold a `maps::map(...)` by value.
GTEST_TEST(mapping_owning_view, keeps_a_lambda_assignable) {
    int offset = 10;
    auto lambda = [offset](std::size_t i) { return offset + int(i); };
    using lambda_t = decltype(lambda);

    static_assert(!std::is_copy_assignable_v<lambda_t>);
    static_assert(std::movable<mapping_owning_view<lambda_t>>);

    auto view = maps::map(lambda);
    static_assert(std::movable<decltype(view)>);
    static_assert(mapping_view<decltype(view), std::size_t>);
    ASSERT_EQ(view[5u], 15);

    auto other = maps::map([](std::size_t i) { return int(i) * 2; });
    static_assert(!std::same_as<decltype(other), decltype(view)>);

    auto reassigned = maps::map(lambda);
    reassigned = maps::map(lambda);
    ASSERT_EQ(reassigned[1u], 11);
}

////////////////////////////////////////////////////////////////////////////////
// the views dispatch all three subscript protocols: operator[], operator()
// and at()
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(mapping_views, dispatch_the_three_subscript_protocols) {
    struct call_op {
        int operator()(std::size_t i) const { return int(i) + 1; }
    };
    struct at_only {
        std::vector<int> v{7, 8, 9};
        int at(std::size_t i) const { return v[i]; }
    };

    ASSERT_EQ(maps::map(call_op{})[3u], 4);

    at_only m;
    ASSERT_EQ(mapping_ref_view(m)[1u], 8);
}

////////////////////////////////////////////////////////////////////////////////
// maps::mapping_all references lvalues, owns rvalues, passes views through --
// and is SFINAE-friendly
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(mapping_all, selects_the_right_adaptor) {
    std::vector<int> v{1, 2, 3};

    auto from_lvalue = maps::mapping_all(v);
    static_assert(std::same_as<decltype(from_lvalue),
                               mapping_ref_view<std::vector<int>>>);
    ASSERT_EQ(std::addressof(from_lvalue.base()), std::addressof(v));

    auto from_rvalue = maps::mapping_all(std::vector<int>{4, 5});
    static_assert(std::same_as<decltype(from_rvalue),
                               mapping_owning_view<std::vector<int>>>);
    ASSERT_EQ(from_rvalue[1u], 5);

    // An existing view is passed through rather than wrapped again.
    auto passed_through = maps::mapping_all(maps::true_map{});
    static_assert(std::same_as<decltype(passed_through), maps::true_map>);

    static_assert(std::same_as<maps::mapping_all_t<std::vector<int> &>,
                               mapping_ref_view<std::vector<int>>>);
    static_assert(std::same_as<maps::mapping_all_t<std::vector<int>>,
                               mapping_owning_view<std::vector<int>>>);
}

// regression: mapping_all_fn::operator() used to be unconstrained, so its
// noexcept-specifier was instantiated for every argument and hard-errored
// outside the immediate context. `requires { maps::mapping_all(x); }` blew up
// instead of yielding false, which made maps::mapping_all_t unusable in a
// constraint.
namespace {
struct not_a_mapping {
    not_a_mapping(const not_a_mapping &) = delete;
    not_a_mapping(not_a_mapping &&) = delete;
};

template <typename T>
concept can_mapping_all =
    requires(T && t) { maps::mapping_all(std::forward<T>(t)); };
}  // namespace

static_assert(can_mapping_all<std::vector<int> &>);
static_assert(can_mapping_all<std::vector<int>>);
static_assert(can_mapping_all<const std::vector<int> &>);
static_assert(can_mapping_all<maps::true_map>);
static_assert(!can_mapping_all<not_a_mapping>);

// regression, same shape as the one above one layer down: the subscript
// members of the two views reach detail::mapping_subscript, which ends in a
// static_assert, through a decltype(auto) return type. Computing that type
// instantiates the body, so without the guards on those members merely
// *asking* `mapping<M, K>` about a map the dispatch rejects fails to compile
// instead of yielding false.
//
// A mutable lambda is what reaches this: its operator() is non-const, so it is
// not readable through a const access and is correctly not a mapping (contract
// Ruling 4) -- but it has to *say* so rather than hard-error.
// Built through mapping_owning_view directly, not maps::map: the factory
// rejects a mutable lambda outright with a friendly static_assert (below), and
// what is under test here is the layer beneath it -- that the *concepts* answer
// false about such a view instead of hard-erroring.
namespace {
auto mutable_lambda = [counter = std::ptrdiff_t{0}](unsigned k) mutable {
    return k + (counter++);
};
using mutable_lambda_map_t = mapping_owning_view<decltype(mutable_lambda)>;
}  // namespace

// Named concepts rather than bare requires-expressions: outside a template
// there is no substitution to suppress the failure, so `requires { m[0u]; }`
// on a map lacking the subscript is a hard error rather than false -- the very
// distinction under test here.
namespace {
template <typename M>
concept subscriptable_mutable = requires(M & m) { m[0u]; };
template <typename M>
concept subscriptable_const = requires(const M & m) { m[0u]; };
}  // namespace

static_assert(!mapping<mutable_lambda_map_t, unsigned>);
static_assert(!output_mapping<mutable_lambda_map_t, unsigned>);
// Only the const subscript leaves the overload set; the map stays usable
// through the non-const one.
static_assert(subscriptable_mutable<mutable_lambda_map_t>);
static_assert(!subscriptable_const<mutable_lambda_map_t>);
static_assert(
    subscriptable_mutable<decltype(maps::map([](unsigned k) { return k; }))>);
static_assert(
    subscriptable_const<decltype(maps::map([](unsigned k) { return k; }))>);

// The supported spelling for a stateful map: a const lambda handing out a
// reference into storage it does not own. Readable const, and writes land.
namespace {
std::vector<int> external_storage(4, 0);
auto ref_lambda_map =
    maps::map([](unsigned k) -> int & { return external_storage[k]; });
using ref_lambda_map_t = decltype(ref_lambda_map);
}  // namespace

static_assert(mapping_of<ref_lambda_map_t, unsigned, int>);
static_assert(output_mapping_of<ref_lambda_map_t, unsigned, int>);

// maps::map rejects a mutable lambda with a friendly static_assert naming the
// remedy. That the assert *fires* cannot be asserted from inside a compiling
// test, so what is pinned here is the discriminator it fires on -- and above
// all that it stays silent for everything else, a false positive being the
// costly direction: it would reject a legitimate map outright.
namespace {
struct const_functor {
    int operator()(unsigned k) const { return int(k); }
};
struct mutable_functor {
    int counter = 0;
    int operator()(unsigned k) { return int(k) + (counter++); }
};
struct noexcept_mutable_functor {
    int operator()(unsigned k) noexcept { return int(k); }
};
struct ref_qualified_functor {
    int operator()(unsigned k) const & { return int(k); }
};
}  // namespace

static_assert(detail::has_non_const_call_operator<decltype(mutable_lambda)>);
static_assert(detail::has_non_const_call_operator<mutable_functor>);
static_assert(detail::has_non_const_call_operator<noexcept_mutable_functor>);

static_assert(!detail::has_non_const_call_operator<const_functor>);
static_assert(!detail::has_non_const_call_operator<ref_qualified_functor>);
static_assert(!detail::has_non_const_call_operator<decltype([](unsigned k) {
    return k;
})>);
// The detector abstains on generic lambdas, mutable ones included, and that is
// not a hole to be plugged with sharper detection: the guard concepts are the
// real gate, and the assertion below is what keeps it true.
static_assert(
    !detail::has_non_const_call_operator<decltype([](auto k) { return k; })>);
static_assert(!detail::has_non_const_call_operator<decltype([](auto k) mutable {
    return k;
})>);
static_assert(
    !mapping<mapping_owning_view<decltype([](auto k) mutable { return k; })>,
             unsigned>);

GTEST_TEST(mapping, writes_through_a_reference_returning_lambda_map_land) {
    ref_lambda_map[2u] = 42;
    ASSERT_EQ(external_storage[2], 42);
}

// an lvalue is referenced, an rvalue is owned -- the std::views::all contract
static_assert(std::same_as<maps::mapping_all_t<std::vector<int> &>,
                           mapping_ref_view<std::vector<int>>>);
static_assert(std::same_as<maps::mapping_all_t<const std::vector<int> &>,
                           mapping_ref_view<const std::vector<int>>>);
static_assert(std::same_as<maps::mapping_all_t<std::vector<int>>,
                           mapping_owning_view<std::vector<int>>>);

////////////////////////////////////////////////////////////////////////////////
// the canned maps answer for any key type
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(canned_maps, constant_and_identity) {
    static_assert(maps::true_map{}[0]);
    static_assert(!maps::false_map{}[0]);
    static_assert(maps::identity_map{}[42] == 42);

    // They answer for any key type, which is what makes them usable as the
    // default filter/priority map of the algorithms.
    ASSERT_TRUE(maps::true_map{}[std::string("anything")]);
    ASSERT_FALSE(maps::false_map{}[std::make_pair(1, 2)]);
    ASSERT_EQ(maps::identity_map{}[std::string("x")], "x");
}

GTEST_TEST(canned_maps, element_map_projects_tuple_elements) {
    const auto entry = std::make_pair(1, std::make_tuple(2, 3.5));

    using first = maps::element_map<0>;
    using second_first = maps::element_map<1, 0>;
    using second_second = maps::element_map<1, 1>;

    ASSERT_EQ(first{}[entry], 1);
    ASSERT_EQ(second_first{}[entry], 2);
    ASSERT_EQ(second_second{}[entry], 3.5);

    static_assert(first{}[std::make_pair(4, 5)] == 4);
    static_assert(
        mapping_of<maps::element_map<1>, std::pair<int, double>, double>);
}

////////////////////////////////////////////////////////////////////////////////
// maps::element_map's noexcept follows std::get, so variant access propagates
////////////////////////////////////////////////////////////////////////////////

// regression: std::get is noexcept for tuple/pair/array but throws
// bad_variant_access for std::variant, so an unconditional noexcept on the
// accessor chain turned that throw into std::terminate.
namespace {
using int_pair = std::pair<int, int>;
using nested_pair = std::pair<std::pair<int, int>, int>;
using int_variant = std::variant<int, double>;
using variant_pair = std::pair<std::variant<int, double>, int>;
}  // namespace

static_assert(noexcept(
    std::declval<const maps::element_map<1> &>()[std::declval<int_pair &>()]));
static_assert(noexcept(std::declval<const maps::element_map<0, 1> &>()
                           [std::declval<nested_pair &>()]));
static_assert(
    !noexcept(std::declval<
              const maps::element_map<0> &>()[std::declval<int_variant &>()]));
static_assert(!noexcept(std::declval<const maps::element_map<0, 0> &>()
                            [std::declval<variant_pair &>()]));

GTEST_TEST(element_map, variant_access_propagates_instead_of_terminating) {
    int_variant v{3.5};  // holds the double alternative
    maps::element_map<0> first;
    ASSERT_THROW((void)first[v], std::bad_variant_access);
}

////////////////////////////////////////////////////////////////////////////////
// map_if's const subscript returns a reference, not a copy
////////////////////////////////////////////////////////////////////////////////

// regression: detail::vertex_map_if / detail::arc_map_if returned `auto` from
// the const subscript and decltype(auto) from the mutable one, so every read
// through a const algorithm object copied the mapped value (dijkstra reads its
// distance and predecessor maps that way).
static_assert(std::same_as<decltype(std::declval<detail::vertex_map_if<
                                        true, static_digraph, std::string> &>()
                                        [std::declval<const unsigned &>()]),
                           std::string &>);
static_assert(std::same_as<decltype(std::declval<const detail::vertex_map_if<
                                        true, static_digraph, std::string> &>()
                                        [std::declval<const unsigned &>()]),
                           const std::string &>);
static_assert(std::same_as<decltype(std::declval<const detail::arc_map_if<
                                        true, static_digraph, std::string> &>()
                                        [std::declval<const unsigned &>()]),
                           const std::string &>);

GTEST_TEST(map_if, const_subscript_returns_a_reference) {
    static_digraph_builder<static_digraph> builder(3);
    builder.add_arc(0u, 1u);
    auto [graph] = builder.build();

    detail::vertex_map_if<true, static_digraph, std::string> map(graph);
    map[0u] = "written through the mutable overload";

    const auto & cmap = map;
    ASSERT_EQ(cmap[0u], "written through the mutable overload");
    // same object, not a copy
    ASSERT_EQ(std::addressof(cmap[0u]), std::addressof(map[0u]));

    // the disabled specialization is still an empty, freely constructible stub
    static_assert(std::is_empty_v<
                  detail::vertex_map_if<false, static_digraph, std::string>>);
}
