# Deferred: `detail::views::concat` makes public range shapes depend on the standard library

*Raised 2026-07-31, during the pre-1.0 review. **Not fixed** — this is a design
decision for the maintainer, not a defect with one obvious repair. Everything
else from that review is fixed; this file is the open item.*

## What happens

[`include/melon/detail/concat_view.hpp`](include/melon/detail/concat_view.hpp)
picks its implementation at compile time:

```cpp
#if defined(__cpp_lib_ranges_concat)
inline constexpr auto concat = std::views::concat;
#else
// melon's own concat_view
#endif
```

`std::views::concat` is a C++26 library feature. Both of the toolchains melon
documents as supported land on different sides of that `#if`:

| configuration | `__cpp_lib_ranges_concat` | implementation |
| --- | --- | --- |
| GCC 14 / C++23 (the documented minimum) | undefined | melon's fallback |
| GCC 15 / C++26 (the CI reference) | defined | `std::views::concat` |

The two do not produce the same kind of range. melon's fallback always ends at
`std::default_sentinel` and is never sized; `std::views::concat` is a
`common_range` when its bases are, and a `sized_range` when its bases are.

Two public entry points are built on it:

- `undirect_view::incidence(u)` — [`views/undirect.hpp`](include/melon/views/undirect.hpp)
- `bidirectional_dijkstra::path()` — [`algorithm/bidirectional_dijkstra.hpp`](include/melon/algorithm/bidirectional_dijkstra.hpp)

## Why it matters more than a type difference

Measured on the same compiler, same headers, only `-std` changed:

```
--- c++23 ---                        --- c++26 ---
has_degree<undirect_view>   = 0      has_degree<undirect_view>   = 1
incidence sized_range       = 0      incidence sized_range       = 1
incidence common_range      = 0      incidence common_range      = 1
incidence forward_range     = 1      incidence forward_range     = 1
```

A **concept** answered differently in two supported configurations. That is the
part the 1.0 contract is supposed to make impossible: code written against
`melon::has_degree` compiled on the CI reference toolchain and failed on the
documented minimum one.

`docs/contract.md` Ruling 10 exempts "the exact type of members documented only
by concept", which covers the *spelling* of the returned range. It does not
cover a range category, and it certainly does not cover a concept flipping.

### Half of it is already fixed

`undirect_view` now carries an O(1) `degree(u)` member
(`out_degree(u) + in_degree(u)`, which is exactly `incidence(u)`'s cardinality,
self-loops counted twice — the same answer the CPO's size-the-range fallback
gave, without the walk). `has_degree<undirect_view<G>>` is therefore `true` in
both configurations and no longer depends on this `#if`.

**What remains** is the shape of `incidence()` and `path()` themselves:
`common_range` and `sized_range` still differ between C++23 and C++26. A user
calling `std::ranges::size(incidence(g, v))`, or requiring `common_range` of
either, still compiles on one supported toolchain and not the other.

## The options

### 1. Pin the shape: always use melon's fallback for 1.x *(recommended)*

Delete the `#if` and the `std::views::concat` branch. Every 1.x build produces
the same range regardless of standard library, and the contract holds without
a caveat. Move to `std::views::concat` in 2.0, when the floor can be C++26 —
a clean semver story: 1.x pins, 2.0 upgrades.

*Cost:* forgoes std's implementation (multi-range, random-access-capable,
common and sized when its bases allow) for the whole 1.x series, in two views
whose ranges are almost always consumed by a plain `for` loop.

### 2. Keep the `#if`, document the variance in Ruling 10

*Cost:* the contract acquires a clause saying some range properties depend on
the toolchain. Nothing about `has_degree`-style flipping reads well in a
document whose opening promise is that following the rulings keeps code
compiling across 1.x. Every future concat-based member inherits the clause.

### 3. Teach the fallback std's shape

Give `concat_view` an `end()` returning an iterator when both bases are common
ranges, and a `size()` when both are sized. Then both branches agree on the
properties that matter, and the `#if` becomes a pure quality-of-implementation
switch.

*Cost:* new range machinery — roughly 40 lines plus its own tests — and it
still will not match `std::views::concat` exactly (N ranges, random access,
`operator[]`). It is the most work and the least finished-feeling of the three.

### 4. Raise the floor to C++26 / GCC 15

Deletes the problem and a supported toolchain with it. GCC 14 is what Ubuntu
24.04 LTS ships; dropping it for one view's range category is not a trade worth
making.

## Recommendation

Option 1 before tagging 1.0. It costs a shape upgrade in a detail view and buys
a contract that is true as written on every configuration in the README's
support table.

## Reproducing

```sh
cat > /tmp/probe.cpp <<'EOF'
#include <cstdio>
#include "melon/all.hpp"
using namespace melon;
using UG = undirect_view<views::graph_all_t<static_digraph &>>;
int main() {
    std::printf("has_degree      = %d\n", int(melon::has_degree<UG>));
    std::printf("incidence sized = %d\n",
                int(std::ranges::sized_range<incidence_range_t<UG>>));
    std::printf("incidence common= %d\n",
                int(std::ranges::common_range<incidence_range_t<UG>>));
}
EOF
for std in c++23 c++26; do
    echo "--- $std ---"
    g++-15 -std=$std -Iinclude -o /tmp/probe /tmp/probe.cpp && /tmp/probe
done
```

`has_degree` now reads `1` in both; the two range properties still differ, and
that is the open half.
