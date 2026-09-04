# Shortest paths

The family melon is built around. Most are label-setting searches over a [heap](../containers/data-structures.md#heaps) — an updatable one for the single-label algorithms, while `biobjective_dijkstra`'s traits ask only for a plain `priority_queue` and default to a non-updatable `d_ary_heap`; the two [`bellman_ford`](#bellman_ford) variants at the end are label-correcting relaxation sweeps instead, with no heap and no non-negativity requirement on the lengths. All take a length map per arc, and all are configurable through [traits](#traits) — including the [semiring](#semirings), which is what lets the same traversal compute a maximum-reliability or a maximum-capacity path.

## `dijkstra`

```cpp
#include "melon/algorithm/dijkstra.hpp"

for(auto && [v, dist] : dijkstra(graph, length_map, 0u))
    std::println("vertex {} at distance {}", v, dist);
```

Requires `outward_incidence_graph`, `has_vertex_map`, and a length map modelling `mapping<arc_t<G>>`. Vertices come out in nondecreasing distance order, each exactly once, with its final distance.

**Members.**

| Member | Effect |
| --- | --- |
| `add_source(s)` / `add_source(s, d)` | seed a source, optionally at a nonzero distance |
| `reset()` | clear the search, keep the graph and maps |
| `finished()` / `current()` / `advance()` / `run()` | the [generator protocol](index.md) |
| `reached(v)` | `v` has entered the heap |
| `visited(v)` | `v` has been settled |
| `current_dist(v)` | tentative distance of a reached, unsettled `v` — read from the heap, no flag needed |
| `dist(v)` | final distance — needs `store_distances` |
| `dists_map()` | a read-only view of the stored distances — needs `store_distances`; only visited vertices hold meaningful values |
| `pred_arc(v)` / `pred_vertex(v)` | predecessor — needs `store_paths` |
| `path_to(t)` | the arcs of the path, **target first** — needs `store_paths` |

Multiple sources are allowed and give the distance to the *nearest* one; seeding them at different offsets is how you express a weighted multi-source problem.

`dijkstra(graph, length_map)` without a source is the same two-phase pattern the flow algorithms use: it builds the maps so you can seed and re-seed cheaply. Unlike them it is harmless to run unseeded — a search with no source is simply `finished()` from the start.

```cpp
struct traits : dijkstra_default_traits<static_digraph, double> {
    static constexpr bool store_distances = true;
    static constexpr bool store_paths = true;
};

dijkstra alg(traits{}, graph, length_map, 0u);
alg.run();

std::println("d(0,4) = {}", alg.dist(4u));
for(auto && a : alg.path_to(4u)) std::print(" {}", a);   //  7 5 1
```

`path_to` walks predecessors from the target back to the source, so the arcs come out in reverse; reverse the range, or read it as-is when you only need the set.

!!! note "Prefetching"

    Before relaxing a vertex, `dijkstra` issues explicit prefetches for the
    out-arc range, the arc targets and the length values it is about to read.
    That is what the [`contiguous_mapping`](../graphs/mappings.md) concept is
    for; on a map that is not contiguous, the prefetch calls compile to
    nothing and the algorithm is simply a normal Dijkstra.

## Semirings

The relaxation step is `semiring::plus(dist_u, length_a)` and the comparison `semiring::less`. Swapping the semiring therefore changes what "shortest" means, without touching the traversal:

| Semiring | `plus` | `less` | `zero` | Computes |
| --- | --- | --- | --- | --- |
| `shortest_path_semiring<T>` | `+` | `<` | `0` | minimum total length |
| `most_reliable_path_semiring<T>` | `*` | `>` | `1` | maximum product of probabilities |
| `max_capacity_path_semiring<T>` | `min` | `>` | `max()` | widest bottleneck path |
| `minimum_spanning_tree_semiring<T>` | takes the arc length | `<` | `0` | Prim's tree order |

```cpp
template <typename Graph>
struct reliability_traits : dijkstra_default_traits<Graph, double> {
    using semiring = most_reliable_path_semiring<double>;
    using heap = updatable_d_ary_heap<
        2, std::pair<vertex_t<Graph>, double>, typename semiring::less_t,
        vertex_map_t<Graph, std::size_t, dijkstra_roles::heap_index>,
        maps::element<1>, maps::element<0>>;
    static constexpr bool store_distances = true;
};

std::vector<double> proba = ...;   // one survival probability per arc
dijkstra alg(reliability_traits<views::graph_all_t<static_digraph &>>{},
             graph, proba, 0u);
alg.run();
alg.dist(4u);   // probability of the most reliable path 0 -> 4
```

Traits are written against the graph type the algorithm *stores* — the view `views::graph_all` wraps the argument in, `graph_ref_view<static_digraph>` for an lvalue container — which is why the example is a template over it rather than a struct naming a container: a graph wrapped in a type-changing view (a [map-providing view](../views/graphs.md#with_vertex_maps-with_arc_maps-with_edge_maps), say) answers the heap-index role with a different map type, and a hard-coded one would no longer match.

Note that the heap's comparator must be `semiring::less_t` — the two are not independently chosen. A `static_assert` checks the heap's entry type; the comparator direction is your responsibility.

Writing your own is four members and two types; anything satisfying the `semiring` concept in `melon/numeric/semiring.hpp` will do.

An unreached vertex's distance is the semiring's `infty`. For `shortest_path_semiring` over an IEEE floating-point `T` that is `std::numeric_limits<T>::infinity()`; over an integral `T` no value absorbs under `+`, so `max()` stands in and `bellman_ford` — the one algorithm that relaxes arcs out of possibly-unreached vertices — guards each relaxation against it. A semiring whose `plus` genuinely absorbs `infty` may promise so with `static constexpr bool infty_is_absorbing = true` — `shortest_path_semiring` promises it exactly when `T` is IEEE floating point, the reliability and capacity semirings always (their `infty` is `0` under multiply and min), and `minimum_spanning_tree_semiring` never, since its `plus` keeps the last arc's weight — which lifts that guard; promise it falsely and unreached vertices relax as reached — for integral lengths, signed overflow.

## Traits

`dijkstra_default_traits<Graph, ValueType>` is the starting point, and inheriting from it and overriding one flag is the normal way to configure:

| Member | Default | Meaning |
| --- | --- | --- |
| `semiring` | `shortest_path_semiring<ValueType>` | see above |
| `heap` | binary `updatable_d_ary_heap` keyed by `vertex_map_t<G, std::size_t, dijkstra_roles::heap_index>` | any `updatable_priority_queue` with entries `std::pair<vertex, length>`; one publishing `index_map_type` must name that very map |
| `store_distances` | `false` | keep a distance per settled vertex |
| `store_paths` | `false` | keep a predecessor arc per reached vertex |

The default heap is worth reading once: its index map is the graph's answer for the `dijkstra_roles::heap_index` [role](index.md#map-roles), `vertex_map_t<Graph, std::size_t, dijkstra_roles::heap_index>`, so for melon's containers the "where is this vertex in the heap" lookup is an array access rather than a hash. A 4-ary heap is often faster on large sparse graphs — change the first template argument and nothing else. A custom `updatable_d_ary_heap` must spell that same index map: the algorithm `static_assert`s it (`heap_index_map_agrees`), because a different but range-constructible map compiles and leaves the heap indexing a private copy. Heaps that do not publish `index_map_type` are not checked.

`store_paths` also costs a *vertex* map when the graph has no `arc_source` (there is no other way back from an arc to its tail), and only an arc-per-vertex map when it does. Both are selected automatically.

## `a_star`

```cpp
#include "melon/algorithm/a_star.hpp"

// per-vertex lower bound on the remaining distance to t
auto h = maps::function(
    [&](vertex_t<G> v) { return euclidean_distance(coords[v], coords[t]); });

for(auto && [v, dist] : a_star(graph, length_map, h, s)) {
    if(v != t) continue;
    std::println("d(s,t) = {}", dist);
    break;
}
```

`dijkstra` guided by a heuristic: a lower bound, per vertex, on the remaining distance to the target. Same requirements plus the heuristic modelling `mapping<vertex_t<G>>` with the length map's value type, and the same members and traits; each vertex still comes out once with its final plain distance — the heuristic never appears in any result — but ordered by `dist + h` instead of `dist`, so vertices that cannot lie on a good path toward the target are settled late or never, and breaking at the target is the whole point.

The heuristic must be **consistent**: relaxing an arc may never improve `dist + h` — under the default semiring, `h(u) <= length(a) + h(v)` for every arc `a : u -> v`. Exact remaining distances and Euclidean bounds on geometric graphs qualify; a merely *admissible* bound does not — `a_star` never re-settles a vertex, so an inconsistent heuristic silently yields wrong distances. Beware floating-point erosion: a mathematically consistent bound computed in `double` can violate the inequality by a rounding error, so shave it (`h * (1 - 1e-9)`) when in doubt. Debug builds assert consistency at every examined arc.

There is no defaulted zero heuristic: with `h == 0` this is `dijkstra` carrying double-width heap entries, so spell `dijkstra` — the equivalence is pinned by the differential tests instead.

## `bidirectional_dijkstra`

```cpp
#include "melon/algorithm/bidirectional_dijkstra.hpp"

bidirectional_dijkstra alg(graph, length_map, 0u, 4u);
auto distance = alg.run().dist();

if(alg.path_found())
    for(auto && a : alg.path()) std::print(" {}", a);   //  1 5 7
```

Advances a forward search from the source and a backward search from the target in alternation, stopping when their frontiers meet. On a large graph where you want one distance rather than all of them, this typically explores a small fraction of what a one-sided Dijkstra would.

It requires **both** `outward_incidence_graph` and `inward_incidence_graph` — the backward search walks in-arcs — so it does not accept a `static_forward_digraph`. It is not a range: `run()` drains the search and returns the algorithm like every other `run()` in the library, `dist()` then reads the distance (idempotently — a second `run()` is a no-op), and `path()` returns the arcs of the path in order from source to target.

Like the one-sided search, it exposes `add_source(s)` / `add_source(s, d)` and `add_target(t)` / `add_target(t, d)`, so either side can be seeded with several vertices at chosen offsets. `pred_arc(v)`, `succ_arc(v)`, `path_found()` and `path()` are gated on `Traits::store_paths`, which the default traits set to `true`.

## `network_voronoi`

```cpp
#include "melon/algorithm/network_voronoi.hpp"

std::vector<vertex_t<static_digraph>> kernels = {0u, 4u};

for(auto && [v, entry] : network_voronoi(graph, length_map, kernels)) {
    auto && [dist, kernel] = entry;
    std::println("{} belongs to {} at distance {}", v, kernel, dist);
}
```

A multi-source Dijkstra that remembers *which* source won each vertex: the graph-theoretic Voronoi diagram induced by a set of kernels. Yields `(vertex, (distance, kernel))` in nondecreasing distance order. A vertex equidistant from several kernels belongs to the one with the smallest vertex id — the tie-break is deterministic, not an artifact of heap order.

`set_kernels(range)` replaces the kernel set on an existing object, so a study over many kernel sets allocates once.

Iteration yields each vertex once and then forgets it. For per-vertex lookup after a `run()`, opt into storage through the traits — `store_distances` gates `dist(v)` and `dists_map()`, `store_clusters` gates `cluster(v)` and `clusters_map()` — the same shape as `dijkstra`'s `store_distances`:

```cpp
struct storing_traits : network_voronoi_default_traits<static_digraph, int> {
    static constexpr bool store_distances = true;
    static constexpr bool store_clusters = true;
};

network_voronoi alg(storing_traits{}, graph, length_map, kernels);
alg.run();
auto d = alg.dist(v);      // distance to the nearest kernel
auto k = alg.cluster(v);   // that kernel's vertex id
```

`network_voronoi_traits` also requires a third flag, `store_cluster_adjacency` — currently inert — so a from-scratch traits struct must declare it; inheriting `network_voronoi_default_traits`, as above, is the easier route.

## `biobjective_dijkstra`

```cpp
#include "melon/algorithm/biobjective_dijkstra.hpp"

std::vector<int> blue = ...;   // first objective, per arc
std::vector<int> red  = ...;   // second objective, per arc

biobjective_dijkstra alg(graph, blue, red);
alg.add_source(0u, 0, 0);
alg.run();

for(auto && [b, r] : alg.pareto_front(4u))
    std::println("(blue {}, red {})", b, r);
// (blue 20, red 7)
// (blue 27, red 5)
// (blue 28, red 4)
```

A label-setting algorithm for the bi-objective shortest path problem: instead of one distance per vertex it maintains the set of Pareto-optimal `(blue, red)` labels, discarding dominated ones as they are generated. `pareto_front(v)` is the resulting range of nondominated cost pairs, and `is_dominated(v, label)` answers the query directly.

The two length maps may have different value types — the blue and red objectives are tracked independently. The output can be exponentially large in principle; on realistic instances it is not, but there is no cap and no ε-dominance option.

## `competing_dijkstras`

```cpp
#include "melon/algorithm/competing_dijkstras.hpp"

competing_dijkstras alg(graph, blue_length_map, red_length_map);
alg.add_blue_source(s);
alg.add_red_source(t);
alg.run();
```

Runs two searches with *different length maps* over the same graph, in one heap, where each vertex is claimed by whichever search reaches it first. `add_blue_source` and `add_red_source` seed the two sides, and `set_blue_length_map` / `set_red_length_map` swap the maps between runs.

This is the machinery behind "which vertices are strictly closer under one length function than another" — comparing a nominal and a perturbed cost, for instance — computed in a single pass instead of two searches and a subtraction. Both maps must share a value type — enforced by a `requires` clause.

## `bellman_ford`

```cpp
#include "melon/algorithm/bellman_ford.hpp"

bellman_ford alg(graph, length_map);
alg.add_source(s);
alg.run();

bool r = alg.reached(v);   // does a shortest path exist
auto d = alg.dist(v);      // its length, or the semiring's infty
```

Label-correcting relaxation passes over all arcs: negative lengths are allowed, which is exactly the case `dijkstra`'s precondition rules out. The price is O(n·m) against Dijkstra's O((m+n) log n) — with early termination as soon as a pass changes nothing — so prefer `dijkstra` whenever lengths are nonnegative.

What remains is a precondition on the *graph*, mirroring dijkstra's on the lengths: **no negative cycle reachable from the sources**. With the default traits it is uncheckable, and a violation silently yields meaningless distances — with `store_paths`, even a `path_to()` that never terminates. If you cannot rule negative cycles out, opt into detection:

```cpp
struct detecting_traits {
    using semiring = shortest_path_semiring<int>;
    static constexpr bool store_paths = true;
    static constexpr bool detect_negative_cycles = true;
};

auto alg = bellman_ford(detecting_traits{}, graph, length_map);
alg.add_source(s).run();
if(alg.found_negative_cycle()) {
    for(auto && a : alg.negative_cycle()) { ... }  // the culprit, in path order
}
```

`detect_negative_cycles` spends one extra certifying relaxation pass — n−1 passes compute every distance, so an n-th one can only certify — and enables `found_negative_cycle()`; combined with `store_paths` it also enables `negative_cycle()`, returning the arcs of one reachable negative cycle, each arc's target being the next one's source. Without the flags, both accessors leave the overload set and their state leaves the object.

`store_paths` in the traits enables `pred_arc` / `pred_vertex` / `path_to`, exactly as in `dijkstra`. There is no `store_distances` flag: the distance map *is* the algorithm's working state, so `dist()` is always available.

Unlike every other algorithm on this page, `bellman_ford` needs no incidence lists — relaxation reads `arcs_entries`, which the base `graph` concept already synthesizes — so it also runs on arc-list-only structures that expose nothing but `vertices`, `arcs`, `arcs_entries` and the [vertex-map creation](../graphs/custom-graphs.md) its state lives in, like every algorithm's.

## `bellman_ford_moore`

```cpp
#include "melon/algorithm/bellman_ford_moore.hpp"

bellman_ford_moore alg(graph, length_map);
alg.add_source(s).run();
```

The queue variant of `bellman_ford`, and the algorithm LEMON ships under the plain `BellmanFord` name: each round rescans the out-arcs of only the vertices the previous round improved, instead of sweeping every arc. On sparse graphs — road networks — most arcs are quiet in most rounds and this wins by a wide margin; on dense graphs the queue bookkeeping loses to the plain arc sweep. The other price is the constraint: the queue needs `out_arcs`, so `bellman_ford_moore` requires an `outward_incidence_graph` and rejects the arc-list-only structures `bellman_ford` accepts.

Everything else is identical to `bellman_ford`: the same traits shape, the same negative-cycle precondition with the same `detect_negative_cycles` opt-in — the certifying pass becomes a certifying *round* — and the same `found_negative_cycle()` / `negative_cycle()` / `path_to()` accessors under the same flags.

## Choosing

| Question | Use |
| --- | --- |
| Distances from one source to everything | `dijkstra` |
| One source-to-target distance on a big graph | `bidirectional_dijkstra` |
| One source-to-target distance, with a lower bound on remaining distance at hand | `a_star` |
| Nearest facility, and which one | `network_voronoi` |
| Trade-off curve between two costs | `biobjective_dijkstra` |
| Which vertices one cost function reaches first | `competing_dijkstras` |
| Unweighted hop counts | [`breadth_first_search`](traversals.md#breadth_first_search) |
| Negative arc lengths, sparse graph | `bellman_ford_moore` |
| Negative arc lengths, dense graph or arc-list-only structure | `bellman_ford` |
