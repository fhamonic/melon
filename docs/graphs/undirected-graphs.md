# Undirected graphs

melon treats the directed case as [primitive](concepts.md#why-a-directed-multigraph) and layers the undirected one on top of it. The concepts live in `melon/graph.hpp`, after the directed ones, and are deliberately parallel to them, with **edge** replacing **arc**.

## `undirected_graph`

```cpp
template <typename T>
concept undirected_graph = requires(const T & t) {
    melon::vertices(t);
    melon::edges(t);
    melon::edge_endpoints(t, std::declval<edge_t<T>>());
};
```

An undirected graph `g` provides:

- `melon::vertices(g)` — as in the directed case, and with the same `vertex_t<G>`;
- `melon::edges(g)` — a range of edge identifiers, of type `edge_t<G>`;
- `melon::edge_endpoints(g, e)` — a `std::pair` of the two endpoints of `e`, **in no meaningful order**.

The last point is the whole difference. A directed graph answers `arc_source` and `arc_target` separately, because the pair is ordered; an undirected one hands both endpoints back at once, and code that cares which is which is by definition not undirected code.

The optional refinements are:

| Concept | Requires | Notes |
| --- | --- | --- |
| `has_num_edges<G>` | `melon::num_edges(g)` | free when `edges(g)` is sized |
| `has_incidence<G>` | `melon::incidence(g, v)` | a range of `(edge, other endpoint)` tuple-likes; a self-loop appears twice |
| `has_degree<G>` | `melon::degree(g, v)` | via a member or ADL `degree`, or derived when `incidence(g, v)` is sized; a self-loop counts twice |
| `has_edge_map<G, T>` | `create_edge_map<T>(g)` | yields `edge_map_t<G, T>` |

## Incidence, not adjacency

`incidence(g, v)` yields pairs rather than bare edges:

```cpp
for(auto && [e, w] : incidence(ug, v)) {
    // e is an edge incident to v; w is its other endpoint
}
```

This is the shape undirected traversals actually need. Returning only the edge would force every caller to re-derive "which endpoint is not `v`" from `edge_endpoints`, with a comparison per step — and that comparison is wrong for a self-loop. The implementation knows the answer already, so it returns it. The entries are tuple-likes rather than literally `std::pair`, so a `std::views::zip` over an edge array and a neighbour array qualifies.

**A self-loop is incident to its vertex at both ends.** `incidence(g, v)` therefore lists it **twice**, each time with `v` itself as the other endpoint, and `degree(g, v)` counts it twice — the graph-theory convention, and what LEMON and Boost.Graph do. This is part of the concepts, not of any one implementation: a member `degree` must agree with the incidence range it summarises, and an algorithm that sums degrees — a Laplacian, a parity check — assumes it. A graph listing a loop once satisfies `has_incidence` today and silently breaks such an algorithm tomorrow.

## Attaching data

Exactly as in the directed case, with `edge` in place of `arc`:

```cpp
edge_map_t<G, double> cost = create_edge_map<double>(g);
edge_map_t<G, bool> in_tree = create_edge_map<bool>(g, false);
```

## Getting one: `views::undirect`

melon ships no standalone undirected container. Undirected graphs are obtained by viewing a directed one through [`views::undirect`](../views/graphs.md#undirect):

```cpp
#include "melon/views/undirect.hpp"

auto [graph, cost_map] = builder.build();
auto ugraph = views::undirect(graph);

static_assert(undirected_graph<decltype(ugraph)>);
```

The view requires its argument to be both an `outward_incidence_graph` and an `inward_incidence_graph` — it must be able to walk arcs in both directions to present a single undirected incidence — so `static_digraph` and `mutable_digraph` qualify but `static_forward_digraph` does not.

Two properties make it cheap to use:

- **Edges keep the arc identifiers.** `edge_t<undirect_view<G>>` is `arc_t<G>`, and `edges(ug)` is `arcs(g)`. An arc map built on the digraph is therefore already a valid edge map on the view — which is why the snippet below passes `cost_map`, built by the digraph builder, straight to Kruskal.
- **Nothing is copied.** The view holds a reference and rewrites no adjacency; `incidence(ug, v)` is the concatenation of the underlying out- and in-incidences.

```cpp
#include "melon/algorithm/kruskal.hpp"

for(auto && e : kruskal(ugraph, cost_map)) {
    auto [u, v] = edge_endpoints(ugraph, e);
    std::println("tree edge {}: {} -- {}", e, u, v);
}
```

`degree(ugraph, v)` is an O(1) member: it adds the wrapped digraph's `out_degree` and `in_degree`, which is exactly what `incidence(v)` yields — a self-loop sits in both lists and so counts twice. It does *not* go through the incidence range, which is a concatenation of two ranges of different types and may not be sized.

## Algorithms on undirected graphs

| Algorithm | Signature |
| --- | --- |
| [`kruskal`](../algorithms/flows-and-trees.md#kruskal) | `kruskal(ugraph, cost_map)` — minimum spanning forest, as a range of edges |
| [`connected_components`](../algorithms/traversals.md#connected-components) | `connected_components(ugraph)` — a range of ranges of vertices |

`weakly_connected_components(g)` is a convenience wrapper that undirects a digraph and runs `connected_components` on it; it requires `g` to be both an outward- and an inward-incidence graph — what `views::undirect` itself needs.

```cpp
for(auto && component : weakly_connected_components(graph)) {
    for(auto && v : component) std::print(" {}", v);
    std::println("");
}
```

## Bringing your own undirected graph

The same rules as for [custom directed graphs](custom-graphs.md) apply: provide members or ADL free functions named `vertices`, `edges`, `edge_endpoints`, and optionally `incidence`, `num_edges`, `degree` and `create_edge_map` — with a self-loop [listed and counted twice](#incidence-not-adjacency). Wrapping happens through `views::graph_all`, whose `graph_ref_view` and `graph_owning_view` forward the undirected protocol exactly as they forward the directed one — see [Ownership and mapping views](../views/ownership.md).

## A graph that is both

Nothing stops a type from modelling `graph` and `undirected_graph` at once — an undirected container that also reads each edge as two opposite arcs, LEMON-style, is the natural example. The wrappers keep both halves: `views::graph_all(g)` forwards arcs and edges alike, so `dijkstra(g, …)` reads the arcs and `kruskal(g, …)` the edges of the same object, and both still do through `views::with_vertex_maps` and `views::reverse`. `views::subgraph` is the exception: it filters vertices and arcs and has no edge filter, so a subgraph of such a type is a `graph` only.

Where one half has to go — a function template overloaded on `graph_view` and `undirected_graph_view` is ambiguous for such a type — [`views::as_directed` and `views::as_undirected`](../views/graphs.md#as_directed-as_undirected) hide the other. Neither converts anything: `views::undirect` is what makes edges out of arcs.
