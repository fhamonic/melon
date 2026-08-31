# Flows and spanning trees

## Maximum flow

Both maximum-flow algorithms take a digraph and a capacity per arc, and both require `outward_incidence_graph`, `inward_incidence_graph`, `has_vertex_map` and `has_arc_map` — the residual network is walked in both directions, so `static_forward_digraph` is not accepted. The capacity map's value type must have a `std::numeric_limits` specialization: a type without one has no usable infinity, and it is rejected at the constraint.

They are not [ranges](index.md): `run()` computes the flow, and the results are read afterwards.

### `edmonds_karp`

```cpp
#include "melon/algorithm/edmonds_karp.hpp"

std::vector<int> capacity = ...;   // one per arc

edmonds_karp alg(graph, capacity, 0u, 4u);
alg.run();

std::println("max flow = {}", alg.flow_value());
for(auto && a : alg.minimum_cut()) std::print(" {}", a);
```

Augments along shortest unsaturated paths, found by breadth-first search — O(V·E²) in the worst case, independent of the capacity values.

### `dinitz`

```cpp
#include "melon/algorithm/dinitz.hpp"

dinitz alg(graph, capacity, 0u, 4u);
alg.run();
std::println("max flow = {}", alg.flow_value());
```

Dinitz's algorithm: rank the vertices by BFS, then push blocking flows through the level graph — O(V²·E), and much better than that in practice. It keeps a per-vertex *consumable view* of the remaining out- and in-arcs so a saturated arc is never rescanned within a phase.

**Prefer `dinitz`** unless you have a specific reason not to: same interface, same results, better asymptotics.

### Common members

| Member | Effect |
| --- | --- |
| `set_source(s)` / `set_target(t)` | change the terminals |
| `reset()` | zero the flow, keep the graph and capacities |
| `run()` | compute a maximum flow |
| `flow_value()` | the value of the flow — the sum over the source's out-arcs |
| `flow(a)` | the flow carried by the arc `a` |
| `flows_map()` | a read-only view of the per-arc flows, for bulk reads and composition |
| `minimum_cut()` | the arcs of a minimum cut, as a range |

!!! warning "Set the terminals before running, and run before reading the cut"

    The two-argument constructor `dinitz(graph, capacity)` builds the working
    maps but leaves the terminals unset — it exists so you can pick them later,
    and change them, without paying for the maps again. `run()`,
    `flow_value()` and `minimum_cut()` all read them, so call `set_source` and
    `set_target` first, or use the four-argument constructor. The terminals
    must also be distinct: a flow from a vertex to itself is ill-posed. All of
    it is asserted, as every precondition in melon is.

    `minimum_cut()` carries a second one: it reads the reachability the
    *final, failed* search leaves behind, so it names a minimum cut only once
    `run()` has converged. `reset()`, `set_source()` and `set_target()` each
    invalidate it again. `flow(a)` and `flows_map()` have no such restriction —
    every augmentation preserves conservation, so they are readable throughout.

    The same applies to `edmonds_karp`.

`set_source`, `set_target` and `reset()` chain, so a series of *s*–*t* computations on one graph reuses all the allocations:

```cpp
dinitz alg(graph, capacity);
for(auto && [s, t] : pairs) {
    alg.reset().set_source(s).set_target(t).run();
    record(s, t, alg.flow_value());
}
```

!!! warning

    `flow_value()` sums the flow on the arcs leaving the source, so it is only
    the maximum flow value after `run()` has converged. `flow(a)` and
    `flows_map()` read the same state: zero after `reset()`, a maximum flow
    once `run()` has converged, and a valid (conserved, capacity-feasible)
    intermediate flow in between. Like every melon map view, `flows_map()`
    refers into the algorithm — it is valid while the algorithm lives and
    stays put. To keep the flows and discard the algorithm, extract them from
    an expiring object — `std::move(alg).flows_map()` moves the stored map
    into an owning view; see
    [Ownership](../views/ownership.md#getting-a-result-map-out-the-s_map-accessors).
    Extraction is terminal: call nothing else on the algorithm afterwards.

## Minimum-cost flow

### `network_simplex`

```cpp
#include "melon/algorithm/network_simplex.hpp"

std::vector<int> capacity = ...;   // one per arc
std::vector<int> cost = ...;       // one per arc
std::vector<int> supply = ...;     // one per vertex, summing to zero

network_simplex alg(graph, capacity, cost, supply);
alg.run();

if(alg.status() == mcf_status::optimal)
    std::println("min cost = {}", alg.total_cost());
```

The primal network simplex, in the implementation lineage of LEMON's: it
minimizes `sum cost(a) * flow(a)` subject to `0 <= flow(a) <= capacity(a)`
and, at every vertex, outflow minus inflow equal to `supply(v)`. Supplies
must sum to zero (asserted); a capacity equal to
`std::numeric_limits<...>::max()` means *unbounded above*. Worst-case
exponential pivots like every simplex — and in practice the fastest exact
method known for the problem.

Nothing is renumbered, no problem copy is made, and the artificial root is
not even materialized: the algorithm runs in the graph's own id spaces,
keeps every piece of state in maps the graph itself hands out
(`create_vertex_map` / `create_arc_map`), and reads capacities, costs and
supplies through the given mappings — the root is as implicit as its
virtual arcs, marked by a vertex being its own parent in the basis tree.
Arc endpoints come from `arc_source` / `arc_target` where the graph
answers them; each endpoint the graph cannot answer gets a map built once
from `arcs_entries`, so — like
[`bellman_ford`](shortest-paths.md#bellman_ford) — an arc-list graph
qualifies as long as it has the map factories. **Neither id space carries
an integrality requirement**: vertex and arc ids may be any copyable,
equality-comparable type the graph's factories and the mappings accept —
the entering-arc search walks the graph's own `arcs()` range through a
resumable cursor — so a `mutable_digraph` with holes from removals
qualifies, and so does a graph whose handles are structs.

Speed still favors a static rebuild for a graph whose `arcs()` range
pointer-chases: the search lands in the innermost pivot loop, and solving
directly on a `mutable_digraph` measures 2–3× slower than one
[`make_static_digraph`](../containers/graphs.md#rebuilding-as-a-static_digraph)
call (maps translated in the same call) followed by the solve, builder
included. That rebuild is `O(n + m)`, which any solve worth timing
dominates: the simplex is worst-case exponential in pivots, and every
pivot already scans a block of arcs.

```cpp
auto [sg, new_supply, new_capacity, new_cost] = make_static_digraph(
    g, std::less{}, std::tie(supply), std::tie(capacity, cost));
network_simplex alg(sg, new_capacity, new_cost, new_supply);
```

Capacities and supplies share one value domain — their `std::common_type`,
so mixed widths widen instead of truncating — and it must be a **signed**
number type, like the cost type. Both are enforced at the constraint:
in unsigned arithmetic no reduced cost ever tests negative, so the algorithm
would silently report every feasible instance infeasible.

`run()` leaves one of three verdicts in `status()`:

| `mcf_status` | Meaning |
| --- | --- |
| `optimal` | `flow(a)` is a minimum-cost flow, `potential(v)` its dual certificate |
| `infeasible` | the supplies cannot be routed within the capacities |
| `unbounded` | a negative-cost cycle of unbounded capacity exists — no finite optimum |

`status()` is meaningful once `finished()` is true; while pivots remain it
still reads `optimal`, because infeasibility is only detectable at
termination.

The algorithm is steppable: one `advance()` is one simplex pivot, so pivots
can be capped, counted or watched, with `finished()` a `const` read as in
every melon algorithm. `run()` is `while(!finished()) advance();`.

With `status() == optimal`, `total_cost()` sums `cost * flow` in a widened
accumulator (`int64_t` for integral inputs — the default traits'
`total_cost_type`), `flow(a)` reads one arc and `potential(v)` one vertex:
every arc satisfies complementary slackness against the potentials, with
reduced cost `cost(a) + potential(source) - potential(target)`. Bulk access
mirrors the flow pair: `flows_map()` / `potentials_map()` refer into the
algorithm, and `std::move(alg).flows_map()` extracts an owning map as a
terminal operation — see
[Ownership](../views/ownership.md#getting-a-result-map-out-the-s_map-accessors).

`reset()` re-reads the maps, so mutating costs or supplies and calling
`reset().run()` re-solves the new problem while reusing every allocation.
The mappings are read **live** during the pivots: they must keep answering,
with unchanged values, from `reset()` until the last query — mutating one
mid-`run()` corrupts the basis silently, and a lambda mapping is
re-evaluated on every read, so cache anything expensive yourself.

Any [mapping](../graphs/mappings.md) fits each slot, and their read
patterns differ: capacities and costs are read from the pivot loops, but
the supply mapping is read exactly once per vertex, inside `reset()` only.
That makes a lambda the natural supply for the common case of a handful of
terminals — a single-source, single-sink problem needs no vertex-sized
vector at all:

```cpp
// route q units from s to t, every other vertex balanced
network_simplex alg(graph, capacity, cost, [s, t, q](const auto & v) {
    return v == s ? q : v == t ? -q : 0;
});
```

The entering-arc pivot rule is LEMON's block search, tunable through a
traits type (`network_simplex_traits`) passed dijkstra-style as a leading
constructor argument — `network_simplex(my_traits{}, graph, capacity, cost,
supply)` — which also carries `total_cost_type`.

## Minimum spanning tree

### `kruskal`

```cpp
#include "melon/algorithm/kruskal.hpp"
#include "melon/views/undirect.hpp"

auto ugraph = views::undirect(graph);

for(auto && e : kruskal(ugraph, cost_map)) std::print(" {}", e);
//  5 6 0 1 7
```

Kruskal's algorithm on an [undirected graph](../graphs/undirected-graphs.md), backed by [`disjoint_sets`](../containers/data-structures.md#disjoint_sets). It *is* a range: edges are yielded in increasing cost order as they are accepted into the tree, so you can stop early — after `k` edges, or once the running total exceeds a budget:

```cpp
int budget = 100, total = 0;
for(auto && e : kruskal(ugraph, cost_map)) {
    if(total + cost_map[e] > budget) break;
    total += cost_map[e];
    chosen.push_back(e);
}
```

On a disconnected graph it yields a minimum spanning **forest** — one tree per component, with no marker between them. Pair it with [`connected_components`](traversals.md#connected-components) if you need to know which is which.

Since `views::undirect` keeps the arc identifiers as edge identifiers, the cost map produced by [`static_digraph_builder`](../containers/graphs.md#the-builder) is directly usable, as above.

!!! note "The Prim alternative"

    There is no separate Prim implementation: running [`dijkstra`](shortest-paths.md#semirings)
    with `minimum_spanning_tree_semiring` and `store_paths = true` performs
    Prim's traversal, and `pred_arc(v)` gives the tree edges. Kruskal is
    usually what you want — it is a range and it handles forests — but the
    Prim route wins on dense graphs and gives
    you the tree rooted where you chose.

## What is missing

melon has no bipartite matching and no general matching. For those today, Boost.Graph or LEMON remain the answer.
