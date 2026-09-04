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

$$
\begin{aligned}
\max_{x} \quad & \sum_{i} v_i x_i \\
\text{s.t.} \quad & \sum_{i} c_i x_i \le B, \\
& x_i \in \{0, 1\} && \forall i.
\end{aligned}
$$

Branch and bound over items sorted by value/cost ratio, bounded by the fractional (LP) relaxation $x_i \in [0, 1]$ — solved greedily in ratio order, whole items until the first that does not fit, taken fractionally — so a node whose bound cannot beat the incumbent is pruned. Written iteratively rather than recursively. The arguments are a **range of items** — a `std::ranges::random_access_range` — plus a value [mapping](../graphs/mappings.md) and a cost mapping over them — so the items need not be integers and the data need not be materialized:

```cpp
auto alg = knapsack_bnb(std::move(items), std::move(values),
                        [&](auto i) { return costs[i]; }, 15);
```

`unbounded_knapsack_bnb` has the identical interface and allows each item to be taken any number of times — $x_i \in \mathbb{N}$; it additionally prunes items dominated by a better ratio at no greater cost.

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

For segments $s_1, \dots, s_n$ it reports every point $p \in \bigcup_{i \ne j} s_i \cap s_j$ with its full set $\{\, i : p \in s_i \,\}$. A vertical line sweeps left to right; the segments crossing it are kept ordered by height and only neighbours in that order are tested, so with $k$ reported points the sweep costs $O((n + k) \log n)$ rather than the $O(n^2)$ of all pairs.

The arguments are a range of segment identifiers — a `forward_range`, stored in the class through `std::views::all` — and a mapping from identifier to segment. The range is kept because `reset()` re-seeds the event queue from it and replays the sweep.

`bentley_ottmann_traits` has a single flag, `report_endpoints` (default `true`), and — alone in the library — the `Traits` parameter comes first in the template parameter list.

### Exact arithmetic

An intersection point is generally not representable in the coordinate type of the input: two integer segments cross at a rational point. melon's answer is `rational<NumT, DenT>` from `melon/numeric/rational.hpp`, and the intersection coordinates come back as rationals with `num()` and `den()` accessors:

```cpp
std::print("({}/{}, {}/{})", std::int64_t(std::get<0>(p).num()),
                             std::int64_t(std::get<0>(p).den()),
                             std::int64_t(std::get<1>(p).num()),
                             std::int64_t(std::get<1>(p).den()));
```

Fractions are not normalized, so the point `(2, 2)` may be reported as `64/32, 64/32`. Compare with cross-multiplication, not by inspecting the numerator.

!!! note "Raw integer coordinates are exact too"

    Where the geometry divides — intersection points and slopes — raw integral
    coordinates are promoted to `numeric::rational`, so `int` segments yield
    exact rational intersections rather than truncated ones (or a division by
    zero on vertical lines, as they once produced).
    `numeric::integer<T>` — that is,
    `numeric::rational<T, numeric::const_value<int, 1>>` — works as well, and a
    [`bounded_value`](../containers/data-structures.md#other-utilities)
    coordinate checks the intermediate widening on top.

The geometric predicates live in `melon/numeric/geometry.hpp` behind the `cartesian_point`, `cartesian_segment` and `cartesian_line` concepts, so a point type of yours works as well — provided it speaks the std tuple protocol (`std::tuple_size` plus `std::get`, which `std::tuple`, `std::pair` and `std::array` all do) with the exact arity and `cartesian_coordinate` leaves — coordinates that compare with `==` and `<` and are not themselves tuples. That last part makes the three categories pairwise disjoint: a segment passed where a point belongs is a constraint failure, not a silently wrong predicate. The extent checks — `point_on_segment`, `segments_intersection` and the overlaps — further require a `common_cartesian_segment`, one whose endpoints share a single point type, in the spirit of `std::ranges::common_range`; only `segment_to_line` accepts mixed endpoint types. The concepts probe qualified `std::get`, so a member or ADL `get` alone is not enough.

## Sampling

`alias_method_sampler` builds Walker's alias table over a range of items and a probability mapping, then samples in $O(1)$:

```cpp
#include "melon/utility/alias_method_sampler.hpp"

std::vector<double> weight = {0.1, 0.6, 0.3};
alias_method_sampler sampler(std::views::iota(0ul, weight.size()), weight);

std::mt19937 rng(42);
auto item = sampler(rng);
```

The probability argument is a [mapping](../graphs/mappings.md) from the items to non-negative floating-point weights — a container indexed by the item, a graph's vertex or arc map when the items are its vertices or arcs, or a callable, which needs no wrapping. It is read once at construction and not stored, so it may go out of scope afterwards. Like `std::discrete_distribution`, the weights need not sum to 1 — the table is normalized by their sum, $\Pr[X = i] = w_i / \sum_j w_j$. Construction is $O(n)$: the scaled weights are split into $n$ bins holding at most two items each, so a sample is one uniform bin, one uniform real and one branch. Use it for repeated sampling from a fixed distribution — random-restart heuristics, randomized rounding, Monte-Carlo over a fixed graph.
