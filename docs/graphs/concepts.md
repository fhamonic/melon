# Graph concepts

melon has no graph base class. What algorithms take is a *type that satisfies a concept*, and this page describes the concepts, in the order they build on each other. They all live in `melon/graph.hpp`.

## Why a directed multigraph

A graph represents relations between objects: the objects are the **vertices**, and the relations are **edges** connecting them. melon sticks to edges joining pairs of vertices — [hypergraphs](https://en.wikipedia.org/wiki/Hypergraph) are out of scope.

Although it may seem counterintuitive, directed graphs are the more general abstraction. In memory, data is fundamentally ordered; specifying that a pair must be treated as *unordered*, as the endpoints of an undirected edge are, takes extra work rather than less. So the directed case is the primitive one, and [undirected graphs](undirected-graphs.md) get their own concepts on top.

A directed edge is called an **arc**, drawn as an arrow from its *source* to its *target*. A graph may hold several arcs with the same endpoints, which makes it a [multigraph](https://en.wikipedia.org/wiki/Multigraph). In a multigraph an arc cannot be identified by its endpoints alone and needs an identifier of its own — which is convenient anyway, since data is attached to arcs and an identifier indexes far better than a pair of vertices.

The mathematical structure melon abstracts is therefore the **directed multigraph**, and that is what the root concept expresses.

## `graph`

```cpp
template <typename T>
concept graph = has_vertices<T> && has_arcs<T> &&
                requires(const T & t) {
                    { melon::arcs_entries(t) } -> cpo::arc_entries_range_of<T>;
                };
```

An instance `g` of a graph type `G` must provide:

- `melon::vertices(g)` — a range of the graph's vertices, of type `vertex_t<G>`;
- `melon::arcs(g)` — a range of the graph's arc identifiers, of type `arc_t<G>`;
- `melon::arcs_entries(g)` — a range whose elements are pairs `(a, (s, t))`, where `a` is an arc and `s`, `t` are its source and target.

The return-type constraint on the last line is what pins that entry shape: each element must be a tuple-like of size 2 whose `std::get<0>` converts to `arc_t<G>` and whose `std::get<1>` is itself a tuple-like of size 2 with both elements converting to `vertex_t<G>`. A type whose `arcs_entries` yields anything else — flat `(a, s, t)` triples, say — fails `graph` here, with a diagnostic naming this requirement, rather than compiling until the first view or algorithm destructures an entry.

Nothing is required of `vertex_t<G>` and `arc_t<G>` beyond identifying vertices and arcs unambiguously: no duplicates in `vertices(g)` or `arcs(g)`. The simplest implementation, and probably the most efficient, uses integers.

Only `vertices` is truly primitive: [`arcs` and `arcs_entries` are synthesized](../reference/customization-points.md) from the incidence functions when the type does not define them. A structure exposing `vertices`, `out_arcs` and `arc_target` is already a `graph`.

Two optional refinements report sizes:

```cpp
template <typename T> concept has_num_vertices = ...;  // melon::num_vertices(g)
template <typename T> concept has_num_arcs = ...;      // melon::num_arcs(g)
```

Both are satisfied for free when the corresponding range is a `sized_range`.

## Incidence

An arc is *incident* to a vertex `v` when `v` is one of its endpoints. Iterating the outgoing arcs of a vertex is the classical lookup operation, and it is expressed by:

```cpp
template <typename T>
concept has_out_arcs =
    graph<T> &&
    requires(const T & t, const vertex_t<T> & v) {
        melon::out_arcs(t, v);
    } &&
    std::convertible_to<std::ranges::range_value_t<out_arcs_range_t<T>>,
                        arc_t<T>>;
```

Listing arcs is rarely enough on its own: a traversal also needs where an arc leads.

```cpp
template <typename T>
concept has_arc_target =
    graph<T> && requires(const T & t, const arc_t<T> & a) {
                      melon::arc_target(t, a);
                  };
```

Since the two are almost always needed together, they are grouped:

```cpp
template <typename T>
concept outward_incidence_graph =
    graph<T> && has_out_arcs<T> && has_arc_target<T>;

template <typename T>
concept inward_incidence_graph =
    graph<T> && has_in_arcs<T> && has_arc_source<T>;
```

They stay separately available because the four capabilities are genuinely independent: a graph may satisfy `outward_incidence_graph` and `has_arc_source` without storing in-arcs — that is exactly `static_digraph` if you drop its reverse index — and an algorithm should require only what it uses.

`has_out_degree` and `has_in_degree` cover `melon::out_degree(g, v)` and `melon::in_degree(g, v)`; both are satisfied automatically when the incidence range is sized.

## Adjacency

Two vertices are *adjacent* when an arc connects them. Iterating neighbors directly, without naming the arcs, is the other classical lookup:

```cpp
template <typename T>
concept outward_adjacency_graph =
    graph<T> && requires(const T & t, const vertex_t<T> & v) {
                      melon::out_neighbors(t, v);
                  };
```

This is not redundant with incidence. Every `outward_incidence_graph` is an `outward_adjacency_graph` — melon derives `out_neighbors` by transforming `out_arcs` through `arc_target` — but the converse fails: a structure holding a flat arc list *plus* per-vertex neighbor lists can answer `out_neighbors` without ever being able to name the arcs leaving a vertex, so it satisfies `outward_adjacency_graph` and not `has_out_arcs`. Algorithms that never look at arc data, such as [`breadth_first_search`](../algorithms/traversals.md) in its default configuration, require only adjacency and therefore accept both.

`inward_adjacency_graph` is the symmetric concept, on `melon::in_neighbors`.

## Attaching data to vertices and arcs

Each graph implementation decides how data is attached to its vertices and arcs — no assumption is made about the underlying identifier types, which range from integers in most cases to plain old structs. The concepts `has_vertex_map<G, T>` and `has_arc_map<G, T>` express that a graph of type `G` can produce maps holding a `T` per vertex or per arc:

```cpp
G g = ...;
vertex_map_t<G, T> vertex_map = create_vertex_map<T>(g);
arc_map_t<G, T> arc_map = create_arc_map<T>(g);
```

The stored values are [default-initialized](https://en.cppreference.com/w/cpp/language/default_initialization) — for primitive types, indeterminate. Pass a second argument to initialize them:

```cpp
vertex_map_t<G, int> vertex_map = create_vertex_map<int>(g, 0);
```

!!! note

    `has_vertex_map<G>` and `has_arc_map<G>` default their value type to
    `std::size_t`, so an algorithm needing scratch indices can write
    `requires has_vertex_map<Graph>` and mean it.

Every request also carries a **role**, a further defaulted template parameter: `create_vertex_map<T, Role>(g)`, `vertex_map_t<G, T, Role>`, `has_vertex_map<G, T, Role>`. A role names *which* map an algorithm is asking for — `dijkstra_roles::heap_index` rather than "some `std::size_t` per vertex" — so that a graph, or a [view](../views/graphs.md#with_vertex_maps-with_arc_maps-with_edge_maps) handing out storage that already exists, can tell two same-typed maps apart. A request naming no role carries `default_role`. A factory with a single template parameter answers every role with its standard map, which is what every container does, and a role never narrows satisfiability: `has_vertex_map<G, T, Role>` holds whenever `has_vertex_map<G, T>` does. See [Map roles](../algorithms/index.md#map-roles).

This design choice has several advantages:

- the graph interface stays entirely agnostic of the types of its vertices and arcs;
- algorithms that must attach data can state that requirement as a concept;
- the actual types `vertex_map_t<G, T>` and `arc_map_t<G, T>` are the implementation's choice and a full customization point, allowing a custom allocator, a flat array for integral identifiers, or a hash map for anything else;
- a mutable implementation can have the graph and its maps hold mutual references, so maps resize when vertices are created, or hand their memory back to the parent graph on destruction for recycling.

What comes back is described by the [mapping concepts](mappings.md).

## Mutation

Graphs that can be modified opt in through one concept per operation, so a structure that supports only some of them is still usable for the rest:

| Concept | Operation |
| --- | --- |
| `has_vertex_creation<G>` | `create_vertex(g)` |
| `has_is_valid_vertex<G>` | `is_valid_vertex(g, v)` |
| `has_vertex_removal<G>` | `has_is_valid_vertex<G>` plus `remove_vertex(g, v)` |
| `has_arc_creation<G>` | `create_arc(g, u, v)` |
| `has_is_valid_arc<G>` | `is_valid_arc(g, a)` |
| `has_arc_removal<G>` | `has_is_valid_arc<G>` plus `remove_arc(g, a)` |
| `has_change_arc_source<G>` | `change_arc_source(g, a, s)` |
| `has_change_arc_target<G>` | `change_arc_target(g, a, t)` |

Removal concepts refine the validity query, because after a removal the only way to tell a live identifier from a dangling one is to ask. The query also stands alone as `has_is_valid_vertex` / `has_is_valid_arc`, so a read-only view over a mutable graph can forward the question without forwarding the mutation.

## What the containers satisfy

| | `static_digraph` | `static_forward_digraph` | `mutable_digraph` | `views::complete_digraph` |
| --- | :-: | :-: | :-: | :-: |
| `graph` | ✓ | ✓ | ✓ | ✓ |
| `has_num_vertices` / `has_num_arcs` | ✓ | ✓ | ✓ | ✓ |
| `has_arc_source` | ✓ | | ✓ | ✓ |
| `has_arc_target` | ✓ | ✓ | ✓ | ✓ |
| `outward_incidence_graph` | ✓ | ✓ | ✓ | ✓ |
| `inward_incidence_graph` | ✓ | | ✓ | ✓ |
| `outward_adjacency_graph` | ✓ | ✓ | ✓ | ✓ |
| `inward_adjacency_graph` | ✓ | | ✓ | ✓ |
| `has_out_degree` | ✓ | ✓ | | ✓ |
| `has_in_degree` | ✓ | | | ✓ |
| `has_vertex_map` / `has_arc_map` | ✓ | ✓ | ✓ | ✓ |
| mutation concepts | | | ✓ | |

`static_forward_digraph` is the honest illustration of why the hierarchy is split so finely: it stores only forward adjacency, so it cannot answer `arc_source` or `in_arcs`, and any algorithm requiring `inward_incidence_graph` rejects it at compile time — while Dijkstra, BFS, DFS and topological sort all accept it.

`mutable_digraph` misses `has_out_degree` because its incidence ranges are intrusive linked lists and therefore not sized; counting neighbors there is `std::ranges::distance(out_arcs(g, v))`, and the concept correctly refuses to pretend it is O(1).

## Using them in your own code

Constrain on the least you need, and the diagnostic does the rest:

```cpp
template <inward_incidence_graph G, mapping<arc_t<G>> WeightMap>
    requires has_vertex_map<G, double>
auto compute_potentials(const G & g, const WeightMap & w) {
    auto potential = create_vertex_map<double>(g, 0.0);
    for(auto && v : vertices(g))
        for(auto && a : in_arcs(g, v)) potential[v] += w[a];
    return potential;
}
```

Called with a `static_forward_digraph`, this fails immediately on `inward_incidence_graph` rather than deep inside the loop.

## Next

- [Mappings](mappings.md) — the other half of the model.
- [Undirected graphs](undirected-graphs.md) — `edges`, `incidence`, `degree`.
- [Bringing your own graph](custom-graphs.md) — the minimum you must write.
- [Customization points](../reference/customization-points.md) — which functions are synthesized from which.
