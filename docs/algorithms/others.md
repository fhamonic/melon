# Combinatorial and geometric

Two families that are not graph algorithms but ship with melon because network-optimization code keeps needing them: knapsack solvers, and segment-intersection detection.

## Knapsack

```cpp
#include "melon/algorithm/knapsack_bnb.hpp"

std::vector<std::size_t> items = {0u, 1u, 2u, 3u, 4u};
std::vector<int> values = {10, 7, 1, 3, 2};
std::vector<int> costs  = { 9, 12, 2, 7, 5};

auto alg = knapsack_bnb(items, values, costs, 15);   // last argument: the budget
alg.run();

for(auto && i : alg.solution_items()) std::print(" {}", i);   //  0 4
alg.solution_value();   // 12
alg.solution_cost();    // 14
```

Branch and bound over items sorted by value/cost ratio, bounded by the fractional (LP) relaxation, and written iteratively rather than recursively. The arguments are a **range of items** plus a value [mapping](../graphs/mappings.md) and a cost mapping over them — so the items need not be integers and the data need not be materialized:

```cpp
auto alg = knapsack_bnb(std::move(items), std::move(values),
                        [&](auto i) { return costs[i]; }, 15);
```

`unbounded_knapsack_bnb` has the identical interface and allows each item to be taken any number of times; it additionally prunes items dominated by a better ratio at no greater cost.

```cpp
#include "melon/algorithm/unbounded_knapsack_bnb.hpp"

auto alg = unbounded_knapsack_bnb(items, values, costs, 15);
alg.run();
alg.solution_value();   // 13
```

Both are exact solvers on an NP-hard problem, so the running time is not bounded by anything useful. `run_with_timeout` gives up and returns the incumbent instead of running forever:

```cpp
using namespace std::chrono_literals;

if(!alg.run_with_timeout(500ms))
    std::println("timed out; best found so far: {}", alg.solution_value());
```

It returns `true` when the search completed within the budget and `false` when it was stopped; either way the solution accessors are valid. It spawns a `std::jthread` internally, so a program using it must link a threading library.

Neither is a [range](index.md) — the answer is a single solution, not a sequence.

## `bentley_ottmann`

```cpp
#include "melon/algorithm/bentley_ottmann.hpp"

using coord = numeric::integer<std::int64_t>;
using point = std::tuple<coord, coord>;
using segment = std::tuple<point, point>;

std::vector<segment> segments = {{{0, 0}, {4, 4}},
                                 {{0, 4}, {4, 0}},
                                 {{2, -1}, {2, 5}}};
auto ids = std::views::iota(0ul, segments.size());

for(auto && [p, intersecting] : bentley_ottmann(ids, segments)) {
    // p is the intersection point, in exact rational coordinates
    for(auto && i : intersecting) std::print(" {}", i);
}
//  2 0 1     — all three segments meet at (2, 2)
```

The Bentley–Ottmann sweep-line: a range yielding every intersection point together with **all** the segment identifiers passing through it, in lexicographic order of the point. Reporting the full set per point rather than one pair per crossing is what makes degenerate inputs — three or more segments through one point, overlapping collinear segments — come out right.

The arguments are a range of segment identifiers and a mapping from identifier to segment, in the same shape as the knapsack solvers.

### Exact arithmetic

An intersection point is generally not representable in the coordinate type of the input: two integer segments cross at a rational point. melon's answer is `rational<NumT, DenT>` from `melon/numeric/rational.hpp`, and the intersection coordinates come back as rationals with `num()` and `den()` accessors:

```cpp
std::print("({}/{}, {}/{})", std::int64_t(std::get<0>(p).num()),
                             std::int64_t(std::get<0>(p).den()),
                             std::int64_t(std::get<1>(p).num()),
                             std::int64_t(std::get<1>(p).den()));
```

Fractions are not normalized, so the point `(2, 2)` may be reported as `64/32, 64/32`. Compare with cross-multiplication, not by inspecting the numerator.

!!! warning "Use `numeric::integer<T>`, not a raw integer type"

    `numeric::integer<T>` is `numeric::rational<T, numeric::const_value<int, 1>>` — an integer that
    participates in melon's exact rational arithmetic. Instantiating the sweep
    on raw `int` coordinates compiles, and then divides by zero at run time.
    Feed it `numeric::integer<std::int64_t>`, or a
    [`bounded_value`](../containers/data-structures.md#other-utilities)
    if you want the intermediate widening checked.

The geometric predicates live in `melon/utility/geometry.hpp` behind the `cartesian_point`, `cartesian_segment` and `cartesian_line` concepts, so a point type of yours — any type answering the concept, not just `std::tuple` — works as well.

## Sampling

`alias_method_sampler` builds Walker's alias table over a range of items and a probability mapping, then samples in O(1):

```cpp
#include "melon/utility/alias_method_sampler.hpp"

std::vector<double> weight = {0.1, 0.6, 0.3};
alias_method_sampler sampler(std::views::iota(0ul, weight.size()),
                             [&](std::size_t i) { return weight[i]; });

std::mt19937 rng(42);
auto item = sampler(rng);
```

The probability argument is a callable, and the probabilities are expected to sum to 1. Construction is O(n); each sample afterwards is one uniform integer, one uniform real and one branch. Use it for repeated sampling from a fixed distribution — random-restart heuristics, randomized rounding, Monte-Carlo over a fixed graph.
