# Traversals and components

All of these are [ranges](index.md): the loop drives them, and `break` stops them. The graph, when it appears in a constructor, follows the [ownership rule](../views/ownership.md).

## `breadth_first_search`

```cpp
#include "melon/algorithm/breadth_first_search.hpp"

for(auto && v : breadth_first_search(graph, 0u)) std::print(" {}", v);
//  0 1 2 5 3 4
```

Requires `outward_adjacency_graph` and `has_vertex_map` — arcs are never inspected, so a graph that only lists neighbors is enough.

On a digraph $G = (V, A)$ with $n = |V|$ and $m = |A|$, the search from $s$ settles vertices in nondecreasing hop distance

$$
d(v) = \min\{\, k : s = v_0 \to v_1 \to \cdots \to v_k = v \,\},
$$

one layer at a time, in $O(n + m)$.

**Traits.**

| Flag | Default | Effect |
| --- | :-: | --- |
| `store_pred_vertices` | `false` | enables `pred_vertex(v)` and `pred_vertices_map()` |
| `store_pred_arcs` | `false` | enables `pred_arc(v)` and `pred_arcs_map()`; requires `outward_incidence_graph` |
| `store_distances` | `false` | enables `dist(v)` — the number of hops — and `dists_map()` |
| `store_traversal_range` | `false` | enables `traversal()`, a `std::span<const vertex>` of the vertices reached so far |

```cpp
struct bfs_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = true;
    static constexpr bool store_distances = true;
    static constexpr bool store_traversal_range = false;
};

breadth_first_search bfs(bfs_traits{}, graph, 0u);
bfs.run();
for(auto && v : vertices(graph))
    if(bfs.reached(v)) std::println("{} at {} hops", v, bfs.dist(v));
```

!!! note "The branchless fast path"

    When the graph knows its vertex count, the vertex type is trivially
    copyable, and no predecessor or distance is stored, melon selects a second
    implementation that preallocates the queue as a flat array of exactly
    `num_vertices + 1` entries and drops the bounds checks. It is chosen
    automatically — the class you name is the same — so the cheapest
    configuration is also the fastest, and asking for one extra map is what
    opts you out of it.

    The two implementations expose the same members with the same signatures,
    including honouring `store_traversal_range`, so which one you get is a
    performance detail and never a source-visible one.

Other members: `reset()`, `add_source(v)`, `reached(v)`, `reached_map()`, `base()`.

## `depth_first_search`

```cpp
#include "melon/algorithm/depth_first_search.hpp"

for(auto && v : depth_first_search(graph, 0u)) std::print(" {}", v);
//  0 1 3 4 2 5
```

Same requirements as BFS. The traversal is iterative — an explicit stack of partially consumed incidence ranges — so depth costs heap memory, not call frames, and a path of a million vertices does not overflow the stack.

Vertices come out in preorder of the search tree $T_s$ — each before any vertex first reached through it — in $O(n + m)$. The predecessor chain is the tree path, so

$$
\mathrm{depth}(v) = \big|\mathrm{path}_{T_s}(s, v)\big| \;\ge\; d(v),
$$

with equality only when the out-arc order happens to favour it.

**Traits.**

| Flag | Default | Effect |
| --- | :-: | --- |
| `store_pred_vertices` | `false` | enables `pred_vertex(v)` and `pred_vertices_map()` |
| `store_pred_arcs` | `false` | enables `pred_arc(v)` and `pred_arcs_map()`; requires `outward_incidence_graph` |
| `store_depth` | `false` | enables `depth(v)` and `depths_map()` — see the warning below |

!!! warning "`depth(v)` is not a distance"

    Where BFS's `dist(v)` is the true shortest-hop distance, DFS's `depth(v)`
    counts the arcs from the source *along the route DFS took* — the length of
    the `pred_vertex` chain. On the same graph it changes with the order the
    out-arcs come in, and it is never smaller than the shortest-hop distance.
    That is why the flag is `store_depth` and not `store_distances`: the two
    are not interchangeable, so do not reach for DFS when you want distances.

## `topological_sort`

```cpp
#include "melon/algorithm/topological_sort.hpp"

for(auto && v : topological_sort(graph)) std::print(" {}", v);
//  0 1 2 3 5 4
```

Yields the vertices of a DAG in an order where every arc goes forward. Requires `outward_incidence_graph`, `has_vertex_map` and `has_num_vertices` — the constructor reserves `num_vertices` and keeps an iterator into that buffer, so the count must be known; unlike the searches, it takes no source — it starts from every vertex with no incoming arc and discovers the rest by decrementing in-degrees.

Kahn's algorithm: a numbering $\pi : V \to \{1, \dots, n\}$ with $\pi(u) < \pi(v)$ for every arc $u \to v$ exists exactly when $G$ is acyclic, and is produced by repeatedly emitting a vertex of in-degree zero and deleting its out-arcs, in $O(n + m)$.

Traits are `store_ranks` and `store_critical_paths`. `store_ranks` enables `rank(v)`: the number of arcs on the longest path from a source down to `v`, $\mathrm{rank}(v) = \max_{u \to v} \mathrm{rank}(u) + 1$ with $0$ at the sources, so that `rank(u) < rank(v)` for every arc `u -> v`. It is a level, not a position in the emitted sequence — vertices that no path orders relative to each other share a rank. `store_critical_paths` enables `pred_vertex(v)`, `pred_arc(v)` and `critical_path_to(t)`.

!!! warning

    The graph must be acyclic. Vertices on a cycle — and vertices behind one —
    are simply never yielded, with no error and no exception. Once the sweep is
    drained, `is_acyclic()` reports whether that happened; it is `true` exactly
    when every vertex came out. Asking before `finished()` is a precondition
    violation, since the count is only meaningful once nothing more can be
    ordered.

    ```cpp
    auto alg = topological_sort(graph);
    for(auto && v : alg) { /* ... */ }
    if(!alg.is_acyclic()) { /* the graph had a cycle */ }
    ```

    To find *where* the cycle is, run
    [`strongly_connected_components`](#strongly_connected_components): every
    component of more than one vertex is one.

## `strongly_connected_components`

```cpp
#include "melon/algorithm/strongly_connected_components.hpp"

for(auto && component : strongly_connected_components(graph)) {
    for(auto && v : component) std::print(" {}", v);
    std::println("");
}
```

Tarjan's algorithm, iterative for the same reason as DFS. Each `current()` is a *range* of the vertices of one component, and components come out in reverse topological order of the condensation — the sinks first.

Two vertices are strongly connected when each reaches the other, $u \sim v \iff u \leadsto v \wedge v \leadsto u$; the classes of $\sim$ are the components, and contracting each to a vertex gives the condensation, always a DAG. One DFS finds them all in $O(n + m)$, popping a component each time the search leaves its root.

With the `store_component_ids` traits flag, the algorithm records a component id
per vertex as each component is popped — dense, in emission order — and
`component_id(u)` answers from those ids once `u`'s component has been yielded,
so directly after a `run()`. Comparing ids is the same-component query, and
`component_ids_map()` views the whole map, for passing to anything that takes a
vertex map:

```cpp
struct scc_traits {
    static constexpr bool store_component_ids = true;
};

strongly_connected_components alg(scc_traits{}, graph);
alg.run();
if(alg.component_id(u) == alg.component_id(v)) { ... }  // mutually reachable
auto ids = alg.component_ids_map();  // ids[v] == 0 for the first component, ...
```

Without the flag none of these exist and no id map is stored.
`num_components()` is available either way: the number of components yielded so
far, `current()` included, so the component count of the graph after a `run()`.

Requires `outward_adjacency_graph` and `has_vertex_map`.

## Connected components

`connected_components` works on an [undirected graph](../graphs/undirected-graphs.md):

```cpp
#include "melon/algorithm/connected_components.hpp"

auto ugraph = views::undirect(graph);
for(auto && component : connected_components(ugraph)) { ... }
```

The classes of $u \sim v \iff$ some path joins $u$ and $v$, direction ignored: one breadth-first search per component, $O(n + m)$ in all.

For a digraph, `weakly_connected_components(g)` is the wrapper that undirects it first:

```cpp
for(auto && component : weakly_connected_components(graph)) {
    for(auto && v : component) std::print(" {}", v);
    std::println("");
}
```

It requires the digraph to be both `outward_incidence_graph` and `inward_incidence_graph`, plus `has_vertex_map`, since the underlying [`views::undirect`](../views/graphs.md#undirect) must walk both ways.

## `traversal_forest`

```cpp
#include "melon/algorithm/traversal_forest.hpp"

for(auto && tree : traversal_forest(graph)) {
    for(auto && v : tree) std::print(" {}", v);
    std::println("");
}
```

Repeats a breadth-first search from each not-yet-reached source until every vertex has been visited, yielding one range of vertices per tree. With no source range it uses `vertices(g)`; pass one to control the order and the roots:

```cpp
std::vector<vertex_t<static_digraph>> roots = {3u, 0u};
for(auto && tree : traversal_forest(graph, roots)) { ... }
```

This is the reachability-partition counterpart of `weakly_connected_components`: the trees partition the vertices, but by *forward* reachability from the chosen roots, so a vertex is placed in the first tree that reaches it.

With roots $r_1, r_2, \dots$ in the given order and $R(r)$ the set of vertices reachable from $r$, the $i$-th tree spans

$$
T_i = R(r_i) \setminus \bigcup_{j < i} T_j,
$$

an already-reached root is skipped, and the whole forest costs $O(n + m)$.

## Choosing

| Question | Use |
| --- | --- |
| Which vertices are reachable from `s`? | `breadth_first_search` / `depth_first_search` |
| How many hops away? | `breadth_first_search` with `store_distances` |
| A valid processing order for a DAG? | `topological_sort` |
| Are `u` and `v` mutually reachable? | `strongly_connected_components` |
| Are `u` and `v` connected, ignoring direction? | `weakly_connected_components` |
| Partition by reachability from given roots? | `traversal_forest` |
| Shortest path with weights? | [Shortest paths](shortest-paths.md) |
