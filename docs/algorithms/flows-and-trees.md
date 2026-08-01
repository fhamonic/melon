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
    `set_target` first, or use the four-argument constructor. Both are
    asserted, as every precondition in melon is.

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

melon has no min-cost flow, no bipartite matching, no general matching, and no network simplex — the last is on the [roadmap](https://github.com/fhamonic/melon#roadmap). For those today, Boost.Graph or LEMON remain the answer.
