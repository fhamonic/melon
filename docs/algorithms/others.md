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

Two things keep the search short where the bound alone prunes nothing. Identical items — same value, same cost — are searched as one entry with a count, so forty copies of an item are one decision among forty-one counts rather than $2^{40}$ subsets. And when the cost type is integral, the budget is first lowered to a multiple of the costs' greatest common divisor, since the remainder can never be spent and would otherwise keep every bound above the optimum.

`unbounded_knapsack_bnb` has the same interface, minus the gap setting described below, and allows each item to be taken any number of times — $x_i \in \mathbb{N}$. It first drops every item that copies of a cheaper one, as many as its cost pays for, are worth as much as; and when the search backs out of an item, it gives up all its copies at once as soon as one copy fewer cannot beat the incumbent, rather than one copy at a time — with a hundred items under a budget of $10^8$, the difference between a millisecond and a hundredth of one.

```cpp
#include "melon/algorithm/unbounded_knapsack_bnb.hpp"

auto alg = unbounded_knapsack_bnb(items, values, costs, 15);
alg.run();
alg.solution_value();   // 13
```

These are exact solvers on an NP-hard problem, so the running time is not bounded by anything useful. `run_with_timeout` gives up and returns the incumbent instead of running forever:

```cpp
using namespace std::chrono_literals;

if(!alg.run_with_timeout(500ms))
    std::println("timed out; best found so far: {}", alg.solution_value());
```

It returns `true` when the search completed within the budget and `false` when it was stopped; either way the solution accessors are valid. It spawns a `std::jthread` internally, so a program using it must link a threading library.

`knapsack_bnb` can also trade exactness for termination. With floating-point values, items of equal ratio make exactness itself the obstacle: the subsets that fill the budget differ from the bound, and from each other, in the last digits, and an exact search enumerates them. `set_relative_gap` lets `run()` stop at a solution proven within a relative gap of the optimum:

```cpp
alg.set_relative_gap(1e-9).run();   // solution_value() >= optimum / (1 + 1e-9)
```

The gap defaults to 0, an exact search, and survives `reset()` and `set_budget()`.

### `knapsack_dp`

`knapsack_dp` solves the same 0-1 problem through the same interface — the constructor, `set_budget`, `set_relative_gap`, `run`, `run_with_timeout` and the three accessors — so either can be written where the other was:

```cpp
#include "melon/algorithm/knapsack_dp.hpp"

auto alg = knapsack_dp(items, values, costs, 15);
alg.run();
alg.solution_value();   // 12
```

It is a dynamic program rather than a search. It starts from the *break solution* — the items that fit one after the other in ratio order — and decides the other items outward from it, alternately offering the next item the break solution lacks and dropping the last one it holds. What it carries from one item to the next is a list of *states*: for each total cost reached, the most valuable item set of that cost, kept only as long as its fractional bound can still beat the best solution found. This is the recursion of Pisinger's minknap (*A minimal algorithm for the 0-1 knapsack problem*, Operations Research 45(5), 1997), without its refinements. Identical items are decided in chunks of 1, 2, 4… copies.

The two solvers fail on different instances, which is why there are two:

| instance | `knapsack_bnb` | `knapsack_dp` |
|---|---|---|
| values unrelated, or loosely related, to costs | fast | fast |
| classes of items of nearly equal ratio, costs unrelated within a class | fast | ten to a hundred times slower |
| costs around $10^6$ and beyond | slower as they grow — a tenth of a second at $10^6$ — but it finishes | time and memory grow with the costs; may not finish |
| value tied to the cost by a fixed charge — value = cost + constant | may not finish beyond a few hundred items | milliseconds with costs up to $10^3$, slower as they grow |
| classes whose costs are multiples of a class unit — pack sizes of a product | may not finish | milliseconds |
| floating-point costs | fine | nothing bounds the number of states |

The branch and bound relies on its bound to discard solutions, and enumerates where the bound cannot tell them apart; the dynamic program relies on item sets sharing a total cost, and keeps one state per cost where they do not.

`knapsack_dp` asks two things `knapsack_bnb` does not. The cost type must represent twice the budget, and the value type twice the optimum: the program works on item sets that overshoot the budget until a later step brings them back under it. And its memory follows the number of states — at most one per total cost an item set can reach, so bounded by twice the budget when costs are integral, but that bound is no comfort with costs around $10^8$, where a search among near-tied classes gathers a hundred million states, gigabytes, within seconds.

`set_state_limit` bounds that memory. The search gives up, keeping its incumbent, at the state that would exceed the limit — about 75 bytes per state of the limit with 64-bit costs and values, so ten million states are some 750 MB:

```cpp
alg.set_state_limit(10'000'000).run();
if(alg.state_limit_reached())
    std::println("gave up; best found so far: {}", alg.solution_value());
```

`run_with_timeout` returns `false` for a search that gave up at the limit, as for one that ran out of time. There is no limit by default — an exact solver that stops short has to be asked to — and, like the gap, the limit survives `reset()` and `set_budget()`.

None of the three is a [range](index.md) — the answer is a single solution, not a sequence.

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
