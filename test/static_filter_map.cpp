#undef NDEBUG
#include <gtest/gtest.h>

#include <random>

#include "melon/container/static_digraph.hpp"
#include "melon/container/static_map.hpp"
#include "melon/detail/intrusive_view.hpp"
#include "melon/mapping.hpp"

#include "ranges_test_helper.hpp"

using namespace melon;

////////////////////////////////////////////////////////////////////////////////
// static_map<K, bool> packs bits yet still models a copyable random-access
// range and an output mapping
////////////////////////////////////////////////////////////////////////////////

static_assert(std::copyable<static_map<std::size_t, bool>>);
static_assert(std::ranges::random_access_range<static_map<std::size_t, bool>>);
static_assert(
    output_mapping_of<static_map<std::size_t, bool>, std::size_t, bool>);

////////////////////////////////////////////////////////////////////////////////
// construction sizes the bit storage and fills it uniformly
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(static_map_bool, empty_constructor) {
    static_map<std::size_t, bool> map;
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());
}

GTEST_TEST(static_map_bool, size_constructor) {
    static_map<std::size_t, bool> map(0);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    static_map<std::size_t, bool> map2(5);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());
}

GTEST_TEST(static_map_bool, size_init_constructor) {
    static_map<std::size_t, bool> map(0, false);
    ASSERT_EQ(map.size(), 0);
    ASSERT_EQ(map.begin(), map.end());
    ASSERT_EQ(std::as_const(map).begin(), std::as_const(map).end());

    static_map<std::size_t, bool> map2(5, true);
    ASSERT_EQ(map2.size(), 5);
    ASSERT_NE(map2.begin(), map.end());
    ASSERT_NE(std::as_const(map2).begin(), std::as_const(map).end());

    // auto test = [](const bool a) { return a; };
    // ASSERT_TRUE(std::ranges::all_of(std::views::values(map2), test));
    // ASSERT_TRUE(
    //     std::ranges::all_of(std::views::values(std::as_const(map2)), test));

    static_map<std::size_t, bool> map3(5, false);
    ASSERT_EQ(map3.size(), 5);
    ASSERT_NE(map3.begin(), map.end());
    ASSERT_NE(std::as_const(map3).begin(), std::as_const(map).end());

    // auto test2 = [](const bool a) { return !a; };
    // ASSERT_TRUE(std::ranges::all_of(std::views::values(map3), test2));
    // ASSERT_TRUE(
    //     std::ranges::all_of(std::views::values(std::as_const(map3)), test2));
}

////////////////////////////////////////////////////////////////////////////////
// operator[] reads and writes individual bits
////////////////////////////////////////////////////////////////////////////////

GTEST_TEST(static_map_bool, accessor_uniform_read) {
    static_map<std::size_t, bool> map(5, false);
    for(std::size_t i = 0; i < map.size(); ++i) {
        ASSERT_FALSE(map[i]);
    }

    static_map<std::size_t, bool> map2(5, true);
    for(std::size_t i = 0; i < map2.size(); ++i) {
        ASSERT_TRUE(map2[i]);
    }
}

GTEST_TEST(static_map_bool, accessor_write_and_read) {
    static_map<std::size_t, bool> map(5, false);
    map[0] = true;
    map[2] = true;
    map[3] = true;

    ASSERT_TRUE(map[0]);
    ASSERT_FALSE(map[1]);
    ASSERT_TRUE(map[2]);
    ASSERT_TRUE(map[3]);
    ASSERT_FALSE(map[4]);

    map[0] = false;
    map[1] = false;
    map[2] = true;

    ASSERT_FALSE(map[0]);
    ASSERT_FALSE(map[1]);
    ASSERT_TRUE(map[2]);
    ASSERT_TRUE(map[3]);
    ASSERT_FALSE(map[4]);
}

GTEST_TEST(static_map_bool, accessor_extensive_write_and_read) {
    const std::size_t num_bools = 153;
    std::vector<bool> datas(num_bools);
    static_map<std::size_t, bool> map(num_bools, false);

    auto gen = std::bind(std::uniform_int_distribution<>(0, 1),
                         std::default_random_engine());

    for(std::size_t i = 0; i < num_bools; ++i) {
        bool b = gen();
        datas[i] = b;
        map[i] = b;
    }

    for(std::size_t i = 0; i < num_bools; ++i) {
        ASSERT_EQ(datas[i], map[i]);
    }
}

GTEST_TEST(static_map_bool, iterator_extensive_read) {
    const std::size_t num_bools = 153;
    std::vector<bool> datas(num_bools);
    static_map<std::size_t, bool> map(num_bools);

    auto gen = std::bind(std::uniform_int_distribution<>(0, 1),
                         std::default_random_engine());

    for(std::size_t i = 0; i < num_bools; ++i) {
        bool b = gen();
        datas[i] = b;
        map[i] = b;
    }

    static_assert(std::random_access_iterator<std::vector<bool>::iterator>);
    static_assert(
        std::random_access_iterator<static_map<std::size_t, bool>::iterator>);

    // ASSERT_TRUE(std::ranges::equal(std::views::values(map), datas));
}

////////////////////////////////////////////////////////////////////////////////
// iterators -- const_iterator included -- are genuinely random access
////////////////////////////////////////////////////////////////////////////////

// regression: iterator_base<I> used to name `iterator` instead of `I` in the
// bodies of post-increment, post-decrement, operator+ and operator-.
// const_iterator satisfied std::random_access_iterator all the same, so the
// breakage only surfaced as a hard error deep inside whichever algorithm used
// those ops.
GTEST_TEST(static_filter_map, const_iterator_random_access_operations) {
    using map_type = static_filter_map<std::size_t>;
    static_assert(std::random_access_iterator<map_type::iterator>);
    static_assert(std::random_access_iterator<map_type::const_iterator>);

    map_type map(70, false);
    map[0] = true;
    map[3] = true;
    map[65] = true;
    const map_type & cmap = map;

    auto it = cmap.begin();
    ASSERT_TRUE(*it);

    const auto post_inc = it++;  // operator++(int)
    ASSERT_TRUE(*post_inc);
    ASSERT_FALSE(*it);

    const auto post_dec = it--;  // operator--(int)
    ASSERT_FALSE(*post_dec);
    ASSERT_TRUE(*it);

    ASSERT_TRUE(*(it + 3));        // operator+(iterator, n)
    ASSERT_TRUE(*(3 + it));        // operator+(n, iterator)
    ASSERT_TRUE(*((it + 5) - 2));  // operator-(iterator, n)
    ASSERT_TRUE(it[3]);            // operator[](n)
    ASSERT_TRUE(*(cmap.begin() + 65));
    ASSERT_EQ((cmap.begin() + 65) - cmap.begin(), 65);

    // the same set on the mutable iterator, which always worked
    auto mit = map.begin();
    ASSERT_TRUE(*(mit++));   // yields index 0, leaves mit on index 1
    ASSERT_FALSE(*(mit--));  // yields index 1, leaves mit back on index 0
    ASSERT_TRUE(*(mit + 3));
    ASSERT_TRUE(*(3 + mit));
    ASSERT_TRUE(*((mit + 5) - 2));
    ASSERT_TRUE(mit[3]);
}

////////////////////////////////////////////////////////////////////////////////
// filter() yields exactly the keys whose bit is set

GTEST_TEST(static_map_bool, filter) {
    const std::size_t num_bools = 153;
    static_filter_map<std::size_t> map(num_bools, false);
    std::vector<std::size_t> indices;

    auto gen = std::bind(std::uniform_int_distribution<>(0, 1),
                         std::default_random_engine());

    for(std::size_t i = 0; i < num_bools; ++i) {
        bool b = gen();
        if(!b) continue;
        indices.emplace_back(i);
        map[i] = b;
    }

    static_assert(std::random_access_iterator<std::vector<bool>::iterator>);
    static_assert(
        std::random_access_iterator<static_map<std::size_t, bool>::iterator>);

    ASSERT_TRUE(EQ_MULTISETS(
        map.filter(std::views::iota(std::size_t{0}, num_bools)), indices));

    ASSERT_TRUE(EQ_MULTISETS(
        map.filter(std::views::iota(int{0}, int{num_bools})), indices));
}

GTEST_TEST(static_filter_map, filter_on_empty_map) {
    static_filter_map<std::size_t> map;
    const std::vector<std::size_t> none;
    ASSERT_EQ(map.size(), 0);
    ASSERT_TRUE(EQ_RANGES(
        map.filter(std::views::iota(std::size_t{0}, std::size_t{0})), none));
}

GTEST_TEST(static_filter_map, filter_matches_reference_over_sizes_and_ranges) {
    std::default_random_engine engine(20240728u);
    auto coin = std::bind(std::uniform_int_distribution<>(0, 3), engine);

    for(const std::size_t size :
        {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{63},
         std::size_t{64}, std::size_t{65}, std::size_t{127}, std::size_t{128},
         std::size_t{129}, std::size_t{192}}) {
        static_filter_map<std::size_t> map(size, false);
        std::vector<bool> reference(size, false);
        for(std::size_t i = 0; i < size; ++i) {
            const bool b = (coin() == 0);
            map[i] = b;
            reference[i] = b;
        }

        for(const std::size_t lo :
            {std::size_t{0}, std::size_t{1}, std::size_t{63}, std::size_t{64},
             std::size_t{65}, size / 2, size}) {
            for(const std::size_t hi :
                {std::size_t{0}, std::size_t{1}, std::size_t{63},
                 std::size_t{64}, std::size_t{65}, size / 2, size}) {
                if(lo > hi) continue;
                std::vector<std::size_t> expected;
                for(std::size_t i = lo; i < std::min(hi, size); ++i)
                    if(reference[i]) expected.emplace_back(i);
                ASSERT_TRUE(
                    EQ_RANGES(map.filter(std::views::iota(lo, hi)), expected))
                    << "size=" << size << " lo=" << lo << " hi=" << hi;
            }
        }
    }
}

GTEST_TEST(static_map_bool, filter_bench) {
    std::cout << "density,map_filter,manual_filter\n";

    auto gen = std::bind(std::uniform_real_distribution<>(0, 1),
                         std::default_random_engine());

    for(double hole_density : std::initializer_list<double>{
            1.0, 0.99, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1, 0.01, 0}) {
        std::cout << hole_density << ',';
        const std::size_t num_bools = 13451;
        static_filter_map<std::size_t> map(num_bools, false);
        std::vector<std::size_t> indices;

        std::size_t hole_size = 3;

        for(std::size_t i = 0; i < num_bools - hole_size; ++i) {
            if(gen() > hole_density) continue;
            for(std::size_t j = 0; j < hole_size; ++j) map[i + j] = true;
        }
        for(std::size_t i = 0; i < num_bools; ++i) {
            if(map[i]) indices.emplace_back(i);
        }

        static_assert(std::random_access_iterator<std::vector<bool>::iterator>);
        static_assert(std::random_access_iterator<
                      static_map<std::size_t, bool>::iterator>);

        {
            auto start = std::chrono::high_resolution_clock::now();

            std::vector<std::size_t> ids;
            for(auto && i :
                map.filter(std::views::iota(std::size_t{0}, num_bools))) {
                ids.emplace_back(i);
            }
            ASSERT_TRUE(EQ_MULTISETS(ids, indices));

            auto stop = std::chrono::high_resolution_clock::now();
            auto duration =
                duration_cast<std::chrono::microseconds>(stop - start);

            std::cout << duration.count() << ',';
        }

        {
            auto start = std::chrono::high_resolution_clock::now();

            std::vector<std::size_t> ids;
            for(std::size_t i = 0; i < num_bools; ++i) {
                if(!map[i]) continue;
                ids.emplace_back(i);
            }
            ASSERT_TRUE(EQ_MULTISETS(ids, indices));

            auto stop = std::chrono::high_resolution_clock::now();
            auto duration =
                duration_cast<std::chrono::microseconds>(stop - start);

            std::cout << duration.count() << std::endl;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// the filter() bit scan never reads outside the allocated buffer
////////////////////////////////////////////////////////////////////////////////

// regression: when the size is an exact multiple of the span width, end_it._p
// addresses one span past the last allocated one. The span-skipping loop used
// to accept cursor._p == end_it._p and dereference it, reading past the
// buffer. A single set bit early on is what exposes it: nothing later stops
// the scan before it runs off the end.
GTEST_TEST(static_filter_map, filter_scan_stays_inside_the_buffer) {
    for(const std::size_t num_bools : {std::size_t{64}, std::size_t{128},
                                       std::size_t{192}, std::size_t{256}}) {
        static_filter_map<std::size_t> map(num_bools, false);
        map[3] = true;

        ASSERT_TRUE(
            EQ_RANGES(map.filter(std::views::iota(std::size_t{0}, num_bools)),
                      {std::size_t{3}}));

        // no bits at all: the scan must still terminate without dereferencing
        // the one-past-the-end span
        const std::vector<std::size_t> none;
        map[3] = false;
        ASSERT_TRUE(EQ_RANGES(
            map.filter(std::views::iota(std::size_t{0}, num_bools)), none));

        // only the very last bit, reached by scanning every span
        map[num_bools - 1] = true;
        ASSERT_TRUE(
            EQ_RANGES(map.filter(std::views::iota(std::size_t{0}, num_bools)),
                      {num_bools - 1}));
    }
}

// Bounds outside [0, size] must be clamped rather than turned into span
// pointers outside the allocation.
GTEST_TEST(static_filter_map, filter_clamps_out_of_range_bounds) {
    static_filter_map<int> map(70, false);
    map[0] = true;
    map[69] = true;

    const std::vector<int> none;
    ASSERT_TRUE(EQ_RANGES(map.filter(std::views::iota(-32, 200)), {0, 69}));
    ASSERT_TRUE(EQ_RANGES(map.filter(std::views::iota(-32, 0)), none));
    ASSERT_TRUE(EQ_RANGES(map.filter(std::views::iota(100, 200)), none));
    ASSERT_TRUE(EQ_RANGES(map.filter(std::views::iota(69, 200)), {69}));
}

////////////////////////////////////////////////////////////////////////////////
// filter() takes its bit-scan fast path for every value category of the range
////////////////////////////////////////////////////////////////////////////////

// regression: the dispatch tested `std::same_as<R, iota_view<K, K>>` on the
// *deduced* type of a forwarding reference, which is iota_view<K, K> & for an
// lvalue. Passing a named range silently fell back to the generic
// filter/transform pipeline.
namespace {
template <typename T>
struct is_intrusive_view : std::false_type {};
template <typename I, typename A, typename B, typename C>
struct is_intrusive_view<intrusive_view<I, A, B, C>> : std::true_type {};

using filter_map = static_filter_map<int>;
using iota_int = std::ranges::iota_view<int, int>;

template <typename R>
inline constexpr bool takes_bitscan_path =
    is_intrusive_view<decltype(std::declval<const filter_map &>().filter(
        std::declval<R>()))>::value;
}  // namespace

static_assert(takes_bitscan_path<iota_int &>);        // lvalue
static_assert(takes_bitscan_path<const iota_int &>);  // const lvalue
static_assert(takes_bitscan_path<iota_int &&>);       // rvalue
// a range whose value type is not the key type still uses the generic path
static_assert(!takes_bitscan_path<std::ranges::iota_view<long, long> &>);

GTEST_TEST(static_filter_map, filter_lvalue_and_rvalue_agree) {
    filter_map map(200, false);
    const std::vector<int> expected = {0, 5, 63, 64, 65, 130, 199};
    for(const int i : expected) map[static_cast<std::size_t>(i)] = true;

    auto named_range = std::views::iota(0, 200);
    ASSERT_TRUE(EQ_RANGES(map.filter(named_range), expected));
    ASSERT_TRUE(EQ_RANGES(map.filter(std::as_const(named_range)), expected));
    ASSERT_TRUE(EQ_RANGES(map.filter(std::views::iota(0, 200)), expected));
}
