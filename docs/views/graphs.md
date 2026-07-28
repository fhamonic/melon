# Graph views

A view is a graph that computes its answers from another graph instead of storing them. It holds a reference (or ownership, see [Ownership and mapping views](ownership.md)), copies no adjacency, and satisfies the same [concepts](../graphs/concepts.md) as any container — so every algorithm accepts one, and views compose with each other.

```cpp
for(auto && [v, dist] : dijkstra(views::reverse(graph), length_map, t)) { ... }
```

Views live in `namespace melon::views`, one header each under `melon/views/`.

## `reverse`

`views::reverse(g)` presents `g` with every arc turned around: what was an out-arc is an in-arc, sources become targets.

```cpp
#include "melon/views/reverse.hpp"

auto r = views::reverse(graph);

out_arcs(r, v);       // == in_arcs(graph, v)
arc_source(r, a);     // == arc_target(graph, a)
out_neighbors(r, v);  // == in_neighbors(graph, v)
```

Arc and vertex identifiers are unchanged, so a length map built for `graph` is a valid length map for `views::reverse(graph)` — which is what makes the one-liner above compute distances *to* `t`:

```cpp
// shortest distance from every vertex to t
for(auto && [v, dist] : dijkstra(views::reverse(graph), length_map, t)) { ... }
```

The view exposes an inward capability exactly when the underlying graph has the matching outward one, and vice versa. Reversing a `static_forward_digraph`, which has no in-arcs, yields a graph with no *out*-arcs — correct, and rejected at compile time by anything that needs to move forward.

## `subgraph`

`views::subgraph(g, vertex_filter, arc_filter)` restricts a graph to the elements its filters accept. Both filters are [mappings](../graphs/mappings.md) to `bool` and both default to `views::true_map`:

```cpp
#include "melon/views/subgraph.hpp"

auto keep = create_vertex_map<bool>(graph, true);
keep[1u] = false;

auto sub = views::subgraph(graph, keep);
for(auto && v : vertices(sub)) { ... }   // 1 is gone
```

Filtering is **consistent, not merely lazy**: an arc is visible only if its own filter accepts it *and* both endpoints pass the vertex filter. `out_arcs(sub, v)` drops arcs pointing at a disabled vertex, `in_arcs` drops arcs coming from one, and `arcs(sub)` is filtered accordingly. You never see a dangling arc.

Because `true_map` is an empty type held with `[[no_unique_address]]`, the specializations matter:

- with both filters defaulted, every accessor forwards straight through — an unfiltered `subgraph` costs nothing and adds no `filter_view`;
- with only an arc filter, only the arc ranges are wrapped;
- with a vertex filter, arc ranges also check the far endpoint, which is where the extra `arc_target` lookup per arc comes from.

### Filters you can flip

When the filter maps are writable, the view forwards the mutation:

```cpp
auto sub = views::subgraph(graph, create_vertex_map<bool>(graph, true),
                                  create_arc_map<bool>(graph, true));

sub.disable_vertex(2u);
sub.enable_arc(7u);
```

`disable_vertex` / `enable_vertex` and `disable_arc` / `enable_arc` are constrained on the filter being an `output_mapping_of<..., bool>`, so they simply do not exist on a view built over `true_map` or a lambda. This is how you get a "graph with elements temporarily switched off" — the pattern flow and branch-and-bound codes want — without rebuilding anything.

A [`static_filter_map`](../containers/data-structures.md#static_filter_map) is a natural filter here: one bit per element, and `filter()` to enumerate what is on.

### `induced_subgraph`

When the subgraph is defined by a *list* of vertices rather than a predicate, `views::induced_subgraph(g, vertices_range)` is more direct: it builds the boolean filter once from the range and keeps the range itself as its `vertices()`, so iterating the subgraph iterates your list rather than scanning and filtering the whole vertex set.

```cpp
std::vector<vertex_t<static_digraph>> keep = {0u, 2u, 5u};
auto ind = views::induced_subgraph(graph, keep);

for(auto && v : vertices(ind)) { ... }   // 0, 2, 5 — in your order
for(auto && a : arcs(ind)) { ... }       // only arcs with both ends in the list
```

The vertex range is held by the view, so it must outlive it; the boolean map is owned.

## `undirect`

`views::undirect(g)` presents a digraph as an [undirected graph](../graphs/undirected-graphs.md): each arc becomes an edge with the same identifier, and the incidence of a vertex is the concatenation of its out- and in-incidences.

```cpp
#include "melon/views/undirect.hpp"

auto ugraph = views::undirect(graph);
for(auto && e : kruskal(ugraph, cost_map)) { ... }
```

It requires the underlying graph to be both an `outward_incidence_graph` and an `inward_incidence_graph`. Since edges keep the arc identifiers, arc maps double as edge maps.

## `complete_digraph`

`views::complete_digraph<V, A>` is a view over nothing at all: the complete digraph on `n` vertices, with all `n(n-1)` arcs computed arithmetically from their identifier.

```cpp
#include "melon/views/complete_digraph.hpp"

views::complete_digraph cd(4);

num_vertices(cd);   // 4
num_arcs(cd);       // 12
arc_source(cd, 5);  // 1
arc_target(cd, 5);  // 3
```

Arc `a` leaves vertex `a / (n - 1)`; self-loops are skipped, so the targets of vertex `u` are the other `n - 1` vertices in order. There is no storage and no allocation, which makes it the right input for a dense problem — a TSP instance, a metric closure — where the arc data lives in a `views::map` over the endpoint coordinates rather than in a container:

```cpp
auto dist = [&](auto a) {
    return euclidean(pos[arc_source(cd, a)], pos[arc_target(cd, a)]);
};

for(auto && [v, d] : dijkstra(cd, dist, 0u)) { ... }
```

The template parameters are the integer types for vertices and arcs, both `unsigned int` by default — worth widening for large `n`, since the arc count is quadratic.

## Composition

Views are graphs, so they nest, and the compiler tracks the capabilities through the stack:

```cpp
auto v = views::reverse(views::subgraph(graph, keep));
for(auto && [u, d] : dijkstra(v, length_map, t)) { ... }
```

Each layer is a thin object holding a pointer (or, for an rvalue, the graph itself) plus its filters, so the stack costs no allocation and no indirection beyond the accessor calls, which inline.

Two things to keep in mind:

- **Identifiers pass through unchanged.** `reverse`, `subgraph` and `undirect` never renumber, which is precisely why maps built on the base graph stay usable at every level.
- **Lifetime follows the reference.** `views::subgraph(graph, keep)` keeps pointers to `graph` and `keep`; both must outlive the view and any algorithm holding it. Passing a temporary graph makes the view own it instead — see [Ownership and mapping views](ownership.md).
