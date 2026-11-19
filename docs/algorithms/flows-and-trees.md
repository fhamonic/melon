# Flows and spanning trees

## Maximum flow

Both maximum-flow algorithms take a digraph and a capacity per arc, and both require `outward_incidence_graph`, `inward_incidence_graph`, `has_vertex_map` and `has_arc_map` — the residual network is walked in both directions, so `static_forward_digraph` is not accepted. The capacity map's value type must have a `std::numeric_limits` specialization: a type without one has no usable infinity, and it is rejected at the constraint.

They are not [ranges](index.md): `run()` computes the flow, and the results are read afterwards.

Both solve, for capacities $c \ge 0$,

$$
\begin{aligned}
\max_{f} \quad & \sum_{a \in \delta^+(s)} f(a) \\
\text{s.t.} \quad & \sum_{a \in \delta^+(v)} f(a) = \sum_{a \in \delta^-(v)} f(a) && \forall v \in V \setminus \{s, t\}, \\
& 0 \le f(a) \le c(a) && \forall a \in A,
\end{aligned}
$$

by augmenting along $s$–$t$ paths of the residual network $G_f$, which carries each arc $a$ with capacity $c(a) - f(a)$ and its reverse with capacity $f(a)$. By max-flow min-cut the optimum equals $\min c(S, V \setminus S)$ over the cuts with $s \in S$, $t \notin S$ — the one `minimum_cut()` returns.

### `edmonds_karp`

```cpp
#include "melon/algorithm/edmonds_karp.hpp"

std::vector<int> capacity = ...;   // one per arc

edmonds_karp alg(graph, capacity, 0u, 4u);
alg.run();

std::println("max flow = {}", alg.flow_value());
for(auto && a : alg.minimum_cut()) std::print(" {}", a);
```

Augments along shortest unsaturated paths, found by breadth-first search. The $s$–$t$ distance in $G_f$ never decreases and each arc can be the bottleneck $O(n)$ times, so there are $O(nm)$ augmentations of $O(m)$ each: $O(nm^2)$ in the worst case, independent of the capacity values.

### `dinitz`

```cpp
#include "melon/algorithm/dinitz.hpp"

dinitz alg(graph, capacity, 0u, 4u);
alg.run();
std::println("max flow = {}", alg.flow_value());
```

Dinitz's algorithm: rank the vertices by BFS distance from $s$ in $G_f$, keep only the arcs $u \to v$ with $\mathrm{lev}(v) = \mathrm{lev}(u) + 1$ — the level graph — and push a *blocking flow* through it, one that saturates an arc on every $s$–$t$ path. Each phase raises the $s$–$t$ distance, so there are at most $n - 1$ phases of $O(nm)$: $O(n^2 m)$, and much better than that in practice. It keeps a per-vertex *consumable view* of the remaining out- and in-arcs so a saturated arc is never rescanned within a phase.

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

## Generalized maximum flow

### `onaga`

```cpp
#include "melon/algorithm/onaga.hpp"

std::vector<double> gain = ...;       // one per arc, in (0, 1]
std::vector<double> capacity = ...;   // one per arc, infinity for none

onaga alg(graph, gain, capacity, s, t, supply);
alg.run();

std::println("flow reaching t = {}", alg.flow_value());
for(auto && v : vertices(graph)) std::print(" {}", alg.potential(v));
```

The requirements of [maximum flow](#maximum-flow) hold here too: `outward_incidence_graph`, `inward_incidence_graph`, `has_vertex_map` and `has_arc_map`. The gain and capacity maps model `mapping_view` over the arcs, and their `std::common_type` must be a floating-point type. Flows are fractional whatever the capacities, so integral capacities pass with floating-point gains, and an integral common type is rejected at the constraint.

A *lossy* network gives each arc $a = (u, v)$ a gain $0 < g(a) \le 1$ next to its capacity $c(a) \ge 0$. The flow $f(a)$ is measured at $u$, and $g(a)\, f(a)$ of it arrives at $v$. The source $s$ sends at most its supply $b$, the other vertices may keep flow but not create it, and the value is the flow that reaches $t$:

$$
\begin{aligned}
\max_{f} \quad & \sum_{a \in \delta^-(t)} g(a)\, f(a) - \sum_{a \in \delta^+(t)} f(a) \\
\text{s.t.} \quad & \sum_{a \in \delta^+(v)} f(a) \le \sum_{a \in \delta^-(v)} g(a)\, f(a) && \forall v \in V \setminus \{s, t\}, \\
& \sum_{a \in \delta^+(s)} f(a) - \sum_{a \in \delta^-(s)} g(a)\, f(a) \le b, \\
& 0 \le f(a) \le c(a) && \forall a \in A.
\end{aligned}
$$

Onaga's algorithm (K. Onaga, *Dynamic programming of optimum flows in lossy communication nets*, IEEE Trans. Circuit Theory, 1966) augments along a highest-gain residual path from $s$ to $t$ until the supply is sent or no path remains. The residual network carries each arc with capacity $c(a) - f(a)$ and gain $g(a)$, and its reverse, while $f(a) > 0$, with gain $1 / g(a) > 1$. A gain above 1 rules out Dijkstra's algorithm on the gains themselves, so each search runs on gains relabeled by vertex labels $p$, $g(a)\, p(v) / p(u) \le 1$, and rescales the vertices it settled so that the labels stay feasible and the path found is tight (É. Tardos and K. D. Wayne, *Simple generalized maximum flow algorithms*, IPCO 1998). The number of augmentations can be exponential. `advance()` makes one, so a caller can bound them.

The result comes with its certificate. `potential(v)` is the gain $\pi(v)$ of a best residual path from $v$ to $t$, 0 if there is none. With the arc duals $(g(a)\, \pi(v) - \pi(u))^+$ on the arcs of finite capacity, the potentials are an optimal dual solution once `run()` has converged, and `flow_value()` equals their value:

$$
\pi(s)\, b + \sum_{a = (u, v) \,:\, c(a) < \infty} c(a)\, \big(g(a)\, \pi(v) - \pi(u)\big)^+ .
$$

After a run cut short by bounding the `advance()` calls, `compute_potentials()` gives a feasible dual of the residual network reached, whose value, `flow_value() + potential(s) * remaining_supply()`, bounds the maximum from above.

| Member | Effect |
| --- | --- |
| `onaga(graph, gains, capacities)` / `onaga(graph, gains, capacities, s, t, supply)` | construct; the gain and capacity maps are read live |
| `set_source(s, supply)` / `set_target(t)` | change the terminals; both keep the flow |
| `set_start_labels(labels)` | the labels each run starts from, 1 by default; kept by `reset()` |
| `reset()` | zero the flow, keep the graph, the maps, the terminals and the start labels |
| `finished()` / `advance()` | whether the run is over; one augmentation, or none and `finished()` |
| `run()` | augment until finished, then compute the potentials; idempotent |
| `compute_potentials()` | the last search, for a run driven by `advance()` |
| `flow_value()` / `remaining_supply()` | the flow that reached $t$; the supply not sent |
| `flow(a)` / `flows_map()` | the flow of an arc, measured at its source |
| `potential(v)` / `potentials_map()` | the dual; valid until the next `advance()`, `reset()` or setter |

An infinite capacity is unbounded, and a capacity of 0 removes its arc, so a run on a subgraph needs no copy of the graph: zero the capacities of the arcs outside it. The maps are read live, like those of [`network_simplex`](#network_simplex). Change them between runs, then call `reset()`. A change during a run leaves flows over their capacities.

Many sources against one target is the pattern the start labels exist for. Labels feasible for every run are set once, and each search then starts close to tight. The potentials of a run with a supply of 0 are such labels: the best gains to $t$ over the arcs of positive capacity.

```cpp
onaga probe(graph, gain, capacity, s0, t, 0.0);
probe.run();

onaga alg(graph, gain, capacity);
alg.set_start_labels(probe.potentials_map());
for(auto && [s, supply] : sources) {
    alg.reset().set_source(s, supply).set_target(t).run();
    record(s, alg.flow_value(), alg.potential(s));
}
```

!!! warning "Preconditions no concept can check"

    - **Gains in $(0, 1]$.** A gain above 1 can close a flow-generating cycle,
      which augmenting paths never cancel: the flow returned is then not
      maximum, without a word.
    - **Feasible start labels:** $p(t) = 1$ and $p(u) \ge g(a)\, p(v)$ on every
      arc of positive capacity. Infeasible labels let a vertex settle before
      its best path is known. On an object that carries flow,
      `set_start_labels()` must be followed by `reset()` before the next
      search, in either order, as asserted: start labels need not be feasible
      for the arcs that carry flow backward.
    - **A finite supply.** The tolerances are relative to it, and an infinite
      supply sends nothing; it is asserted.
    - **`potential(v)` after `compute_potentials()`**, which `run()` calls,
      and before the next `advance()`, `reset()` or setter; it is asserted.
      `run()` spends one search on the potentials: a caller that needs only
      the flow drives `while(!alg.finished()) alg.advance();` instead.

The traits, passed as a leading constructor argument (`onaga(my_traits{}, graph, gain, capacity, s, t, supply)`), carry the heap and four constants:

- `heap<Compare>`, an alias template. The algorithm instantiates it with `std::greater<value>`, a **max**-heap on the relabeled gains, since a min-heap would settle the worst paths first: the traits choose the structure, never the order. The default is a 4-ary `updatable_d_ary_heap`, and a binary one is spelled
  ```cpp
  struct binary_heap_traits : onaga_default_traits<static_digraph, double> {
      template <std::strict_weak_order<double, double> Compare>
      using heap = updatable_d_ary_heap<
          2, std::pair<vertex_t<static_digraph>, double>, Compare,
          vertex_map_t<static_digraph, std::size_t, onaga_roles::heap_index>,
          maps::element<1>, maps::element<0>>;
  };
  ```
- `arc_full` and `negligible_flow`, relative tolerances: an arc is full within `arc_full` of its capacity, and a flow under `negligible_flow` times the supply is none. At 0, the rounding residue of a push that fills an arc leaves it open, and the searches go on augmenting along it. The defaults are $10^{-11}$ and $10^{-12}$ at `double`, scaled by the type's epsilon for `float` and `long double`.
- `sparse_reset`, on by default: `reset()` restores only what the searches since the last `reset()` reached, instead of every vertex and arc. Runs that end with the potentials search reach most of the graph and gain little, within 1%; runs of a single short search on a large graph gain the most, 5 ms against 480 ms for 20,000 one-search runs on a 300 × 300 grid.
- `flowing_arc_lists`, on by default: the arcs that took flow are kept in per-vertex lists, and the searches find the reverse residual arcs there rather than among every in-arc (from $s$) and out-arc (from $t$). Single runs measured 13% faster on random and grid networks. Ties between equal gains break in another order, so a run cut short may follow another of the best paths; a converged run reaches the same optimum, up to rounding.


## Minimum-cost flow

### `network_simplex`

```cpp
#include "melon/algorithm/network_simplex.hpp"

std::vector<int> capacity = ...;   // one per arc
std::vector<int> cost = ...;       // one per arc
std::vector<int> supply = ...;     // one per vertex, summing to zero

network_simplex alg(graph, capacity, cost, supply);
alg.run();

if(alg.optimal())
    std::println("min cost = {}", alg.total_cost());
```

Requires a `graph_view` with default-initializable vertex and arc ids, `has_num_vertices`, `has_num_arcs`, and **both** `has_vertex_map` and `has_arc_map` — the state lives in factory-created maps; capacity, cost and supply maps modelling `mapping_view` over their ids, with a **signed** value domain (the capacity/supply `std::common_type`) and a **signed** cost type, each with a `std::numeric_limits` specialization.

The minimum-cost flow problem, over a capacity $u$ and a cost $c$ per arc and a supply $b$ per vertex summing to zero:

$$
\begin{aligned}
\min_{f} \quad & \sum_{a \in A} c(a)\, f(a) \\
\text{s.t.} \quad & \sum_{a \in \delta^+(v)} f(a) - \sum_{a \in \delta^-(v)} f(a) = b(v) && \forall v \in V, \\
& 0 \le f(a) \le u(a) && \forall a \in A.
\end{aligned}
$$

The primal network simplex, in LEMON's lineage. A basis is a spanning tree $T$ — rooted at an implicit vertex that is never materialized — together with potentials $\pi$ whose reduced costs $c^\pi(a) = c(a) + \pi(\mathrm{src}\, a) - \pi(\mathrm{tgt}\, a)$ vanish on $T$. A pivot picks an entering arc violating optimality — $c^\pi(a) < 0$ at flow $0$, or $c^\pi(a) > 0$ at flow $u(a)$ — pushes the largest feasible amount around the cycle it closes in $T$, and drops the arc that hit its bound. No such arc means optimal. Exponentially many pivots in the worst case, like every simplex — and in practice the fastest exact method known.

A capacity equal to `std::numeric_limits<...>::max()` means *unbounded above*. Capacities and supplies share one value domain — their `std::common_type` — which, like the cost type, must be **signed**; both are enforced at the constraint, since in unsigned arithmetic no reduced cost ever tests negative. The cost type must also keep $n \cdot \max_a |c(a)|$ — the price of the artificial arcs carrying the initial basis — below **half** its range; it is asserted, and the fix is a wider cost type.

`run()` leaves exactly one of three verdicts true:

| Predicate | Meaning |
| --- | --- |
| `optimal()` | `flow(a)` is a minimum-cost flow, `potential(v)` its dual certificate |
| `infeasible()` | the supplies cannot be routed within the capacities |
| `unbounded()` | a negative-cost cycle of unbounded capacity exists — no finite optimum |

All three are false while `finished()` is not: infeasibility is only detectable at termination. One `advance()` is one pivot, so pivots can be capped, counted or watched.

With `optimal()`, `total_cost()` sums $\sum_a c(a)\, f(a)$ in a widened accumulator (the traits' `total_cost_type`, `int64_t` for integral inputs), `flow(a)` reads one arc and `potential(v)` one vertex — every arc satisfies complementary slackness against the potentials. `flows_map()` / `potentials_map()` refer into the algorithm, and `std::move(alg).flows_map()` extracts an owning map as a terminal operation — see [Ownership](../views/ownership.md#getting-a-result-map-out-the-s_map-accessors).

Nothing is renumbered and no problem copy is made: the state lives in maps the graph hands out, and arc endpoints come from `arc_source` / `arc_target` or, where the graph cannot answer, a map rebuilt from `arcs_entries` on every `reset()` — so an arc-list graph qualifies, though unlike with [`bellman_ford`](shortest-paths.md#bellman_ford) only once it also answers `num_vertices` / `num_arcs` and hands out both map kinds. Neither id space needs to be integral: a `mutable_digraph` with holes from removals qualifies, and so does a graph whose handles are structs. Speed still favors a static rebuild for a graph whose `arcs()` range pointer-chases — solving directly on a `mutable_digraph` measures 2–3× slower than one [`make_static_digraph`](../containers/graphs.md#rebuilding-as-a-static_digraph) call followed by the solve:

```cpp
auto [sg, new_supply, new_capacity, new_cost] = make_static_digraph(
    g, std::less{}, std::tie(supply), std::tie(capacity, cost));
network_simplex alg(sg, new_capacity, new_cost, new_supply);
```

The mappings are read **live**: `reset()` re-reads them, so mutating costs or supplies and calling `reset().run()` re-solves while reusing every allocation — the graph's vertex and arc sets, however, are fixed at construction: every state map is created once and never resized, so a graph that grew needs a new algorithm. Mutating a mapping mid-`run()` corrupts the basis silently, and a lambda mapping is re-evaluated on every read. The supply is the exception, read exactly once per vertex inside `reset()`, which makes a lambda the natural supply for a handful of terminals:

```cpp
// route q units from s to t, every other vertex balanced
network_simplex alg(graph, capacity, cost, [s, t, q](const auto & v) {
    return v == s ? q : v == t ? -q : 0;
});
```

The pivot rule is selectable through a traits type passed as a leading constructor argument — `network_simplex(my_traits{}, graph, capacity, cost, supply)` — which also carries `total_cost_type`: `pivot_rules::block_search<Factor, MinSize>` (the default — the most negative reduced cost of a $\sqrt{m}$-sized block), `pivot_rules::first_eligible`, and `pivot_rules::best_eligible` (Dantzig's full scan). A custom rule is a movable value type constructed from the arc count — `reset()` rebuilds it by assignment — whose `find_entering_arc(context)` returns the entering arc as an `std::optional`, holding no references into the algorithm. The traits' `arc_mixing` flag — LEMON's storage mixing as a strided scan order — defaults to **off**: it measured 7–15% slower on families whose packing never inflated the pivot count.

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

The tree minimizing $\sum_{e \in T} w(e)$ over all spanning trees $T$: edges are scanned in nondecreasing $w$, and $e = \{u, v\}$ is accepted exactly when $u$ and $v$ lie in different sets, which then merge — $O(m \log m)$ for the sort, near-linear for the unions.

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
