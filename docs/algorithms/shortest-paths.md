# Shortest paths

The family melon is built around. All of them are label-setting searches over an [updatable heap](../containers/data-structures.md#updatable_d_ary_heap), all take a length map per arc, and all are configurable through [traits](#traits) — including the [semiring](#semirings), which is what lets the same traversal compute a maximum-reliability or a maximum-capacity path.

## `dijkstra`

```cpp
#include "melon/algorithm/dijkstra.hpp"

for(auto && [v, dist] : dijkstra(graph, length_map, 0u))
    std::println("vertex {} at distance {}", v, dist);
```

Requires `outward_incidence_graph`, `has_vertex_map`, and a length map modelling `input_mapping<arc_t<G>>`. Vertices come out in nondecreasing distance order, each exactly once, with its final distance.

**Members.**

| Member | Effect |
| --- | --- |
| `add_source(s)` / `add_source(s, d)` | seed a source, optionally at a nonzero distance |
| `reset()` | clear the search, keep the graph and maps |
| `finished()` / `current()` / `advance()` / `run()` | the [generator protocol](index.md) |
| `reached(v)` | `v` has entered the heap |
| `visited(v)` | `v` has been settled |
| `current_dist(v)` | tentative distance of a reached, unsettled `v` — needs `store_distances` |
| `dist(v)` | final distance — needs `store_distances` |
| `pred_arc(v)` / `pred_vertex(v)` | predecessor — needs `store_paths` |
| `path_to(t)` | the arcs of the path, **target first** — needs `store_paths` |

Multiple sources are allowed and give the distance to the *nearest* one; seeding them at different offsets is how you express a weighted multi-source problem.

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
struct reliability_traits {
    using semiring = most_reliable_path_semiring<double>;
    using heap = updatable_d_ary_heap<
        2, std::pair<vertex_t<static_digraph>, double>,
        typename semiring::less_t, vertex_map_t<static_digraph, std::size_t>,
        views::element_map<1>, views::element_map<0>>;
    static constexpr bool store_distances = true;
    static constexpr bool store_paths = false;
};

std::vector<double> proba = ...;   // one survival probability per arc
dijkstra alg(reliability_traits{}, graph, proba, 0u);
alg.run();
alg.dist(4u);   // probability of the most reliable path 0 -> 4
```

Note that the heap's comparator must be `semiring::less_t` — the two are not independently chosen, and a mismatch is caught by a `static_assert` in the algorithm.

Writing your own is four members and two types; anything satisfying the `semiring` concept in `melon/utility/semiring.hpp` will do.

## Traits

`dijkstra_default_traits<Graph, ValueType>` is the starting point, and inheriting from it and overriding one flag is the normal way to configure:

| Member | Default | Meaning |
| --- | --- | --- |
| `semiring` | `shortest_path_semiring<ValueType>` | see above |
| `heap` | binary `updatable_d_ary_heap` keyed by a `vertex_map_t<G, std::size_t>` | any `updatable_priority_queue` with entries `std::pair<vertex, length>` |
| `store_distances` | `false` | keep a distance per settled vertex |
| `store_paths` | `false` | keep a predecessor arc per reached vertex |

The default heap is worth reading once: its index map is a `vertex_map_t<_Graph, std::size_t>`, so for melon's containers the "where is this vertex in the heap" lookup is an array access rather than a hash. A 4-ary heap is often faster on large sparse graphs — change the first template argument and nothing else.

`store_paths` also costs a *vertex* map when the graph has no `arc_source` (there is no other way back from an arc to its tail), and only an arc-per-vertex map when it does. Both are selected automatically.

## `bidirectional_dijkstra`

```cpp
#include "melon/algorithm/bidirectional_dijkstra.hpp"

bidirectional_dijkstra alg(graph, length_map, 0u, 4u);
auto distance = alg.run();

if(alg.path_found())
    for(auto && a : alg.path()) std::print(" {}", a);   //  1 5 7
```

Advances a forward search from the source and a backward search from the target in alternation, stopping when their frontiers meet. On a large graph where you want one distance rather than all of them, this typically explores a small fraction of what a one-sided Dijkstra would.

It requires **both** `outward_incidence_graph` and `inward_incidence_graph` — the backward search walks in-arcs — so it does not accept a `static_forward_digraph`. It is not a range: `run()` returns the distance, and `path()` returns the arcs of the path in order from source to target.

## `network_voronoi`

```cpp
#include "melon/algorithm/network_voronoi.hpp"

std::vector<vertex_t<static_digraph>> kernels = {0u, 4u};

for(auto && [v, entry] : network_voronoi(graph, length_map, kernels)) {
    auto && [dist, kernel] = entry;
    std::println("{} belongs to {} at distance {}", v, kernel, dist);
}
```

A multi-source Dijkstra that remembers *which* source won each vertex: the graph-theoretic Voronoi diagram induced by a set of kernels. Yields `(vertex, (distance, kernel))` in nondecreasing distance order.

`set_kernels(range)` replaces the kernel set on an existing object, so a study over many kernel sets allocates once.

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

Both length maps must have the same value type — enforced by a `requires` clause. The output can be exponentially large in principle; on realistic instances it is not, but there is no cap and no ε-dominance option.

## `competing_dijkstras`

```cpp
#include "melon/algorithm/competing_dijkstras.hpp"

competing_dijkstras alg(graph, blue_length_map, red_length_map);
alg.add_blue_source(s);
alg.add_red_source(t);
alg.run();
```

Runs two searches with *different length maps* over the same graph, in one heap, where each vertex is claimed by whichever search reaches it first. `add_blue_source` and `add_red_source` seed the two sides, and `set_blue_length_map` / `set_red_length_map` swap the maps between runs.

This is the machinery behind "which vertices are strictly closer under one length function than another" — comparing a nominal and a perturbed cost, for instance — computed in a single pass instead of two searches and a subtraction. As with `biobjective_dijkstra`, both maps must share a value type.

## Choosing

| Question | Use |
| --- | --- |
| Distances from one source to everything | `dijkstra` |
| One source-to-target distance on a big graph | `bidirectional_dijkstra` |
| Nearest facility, and which one | `network_voronoi` |
| Trade-off curve between two costs | `biobjective_dijkstra` |
| Which vertices one cost function reaches first | `competing_dijkstras` |
| Unweighted hop counts | [`breadth_first_search`](traversals.md#breadth_first_search) |
| Negative arc lengths | not supported — melon has no Bellman–Ford |
