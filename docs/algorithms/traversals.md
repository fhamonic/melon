# Traversals and components

All of these are [ranges](index.md): the loop drives them, and `break` stops them. The graph, when it appears in a constructor, follows the [ownership rule](../views/ownership.md).

## `breadth_first_search`

```cpp
#include "melon/algorithm/breadth_first_search.hpp"

for(auto && v : breadth_first_search(graph, 0u)) std::print(" {}", v);
//  0 1 2 5 3 4
```

Requires `outward_adjacency_graph` and `has_vertex_map` — arcs are never inspected, so a graph that only lists neighbors is enough.

**Traits.**

| Flag | Default | Effect |
| --- | :-: | --- |
| `store_pred_vertices` | `false` | enables `pred_vertex(v)` |
| `store_pred_arcs` | `false` | enables `pred_arc(v)`; requires `outward_incidence_graph` |
| `store_distances` | `false` | enables `dist(v)` — the number of hops |
| `store_traversal_range` | `false` | enables `traversal()`, the vertices reached so far |

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

Other members: `reset()`, `add_source(v)`, `reached(v)`, `reached_map()`.

## `depth_first_search`

```cpp
#include "melon/algorithm/depth_first_search.hpp"

for(auto && v : depth_first_search(graph, 0u)) std::print(" {}", v);
//  0 1 3 4 2 5
```

Same requirements as BFS, and the same traits minus `store_traversal_range`. The traversal is iterative — an explicit stack of partially consumed incidence ranges — so depth costs heap memory, not call frames, and a path of a million vertices does not overflow the stack.

## `topological_sort`

```cpp
#include "melon/algorithm/topological_sort.hpp"

for(auto && v : topological_sort(graph)) std::print(" {}", v);
//  0 1 2 3 5 4
```

Yields the vertices of a DAG in an order where every arc goes forward. Requires `outward_incidence_graph` and `has_vertex_map`; unlike the searches, it takes no source — it starts from every vertex with no incoming arc and discovers the rest by decrementing in-degrees.

Traits are the same three flags as DFS. `dist(v)` here is the rank in the order.

!!! warning

    The graph must be acyclic. Vertices on a cycle are simply never yielded —
    there is no error and no exception — so if the input might contain one,
    compare what you got against `num_vertices(graph)`, or run
    [`strongly_connected_components`](#strongly_connected_components) first.

## `strongly_connected_components`

```cpp
#include "melon/algorithm/strongly_connected_components.hpp"

for(auto && component : strongly_connected_components(graph)) {
    for(auto && v : component) std::print(" {}", v);
    std::println("");
}
```

Tarjan's algorithm, iterative for the same reason as DFS. Each `current()` is a *range* of the vertices of one component, and components come out in reverse topological order of the condensation — the sinks first.

`same_component(u, v)` answers the query directly after a `run()`.

Requires `outward_adjacency_graph` and `has_vertex_map`.

## Connected components

`connected_components` works on an [undirected graph](../graphs/undirected-graphs.md):

```cpp
#include "melon/algorithm/connected_components.hpp"

auto ugraph = views::undirect(graph);
for(auto && component : connected_components(ugraph)) { ... }
```

For a digraph, `weakly_connected_components(g)` is the wrapper that undirects it first:

```cpp
for(auto && component : weakly_connected_components(graph)) {
    for(auto && v : component) std::print(" {}", v);
    std::println("");
}
```

It requires the digraph to be both `outward_adjacency_graph` and `inward_adjacency_graph`, since the underlying [`views::undirect`](../views/graphs.md#undirect) must walk both ways.

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
