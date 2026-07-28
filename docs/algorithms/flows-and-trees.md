# Flows and spanning trees

## Maximum flow

Both maximum-flow algorithms take a digraph and a capacity per arc, and both require `outward_incidence_graph`, `inward_incidence_graph`, `has_vertex_map` and `has_arc_map` — the residual network is walked in both directions, so `static_forward_digraph` is not accepted.

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
| `minimum_cut()` | the arcs of a minimum cut, as a range |

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
    the maximum flow value after `run()` has converged. There is no
    per-arc flow accessor in the public interface yet — if you need the flow
    decomposition rather than the value and the cut, this is currently a gap.

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
    usually what you want — it is a range, it handles forests, and it does not
    need `has_vertex_map` — but the Prim route wins on dense graphs and gives
    you the tree rooted where you chose.

## What is missing

melon has no min-cost flow, no bipartite matching, no general matching, and no network simplex — the last is on the [roadmap](https://github.com/fhamonic/melon#roadmap). For those today, Boost.Graph or LEMON remain the answer.
