# A first graph

This page walks through the program on the [home page](../index.md), then extends it: reading distances and paths, attaching your own data, running the search on a view, and switching to a mutable graph.

## Building a graph

`static_digraph` is melon's workhorse: an immutable compressed-adjacency structure whose vertices and arcs are consecutive integers from 0. It has no `add_arc` — arcs are collected first, then the structure is built in one pass, which is what makes its lookups cheap.

```cpp
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

using namespace melon;

// six vertices, 0 to 5; one double of data per arc
static_digraph_builder<static_digraph, double> builder(6);

builder.add_arc(0, 1, 7.0)
    .add_arc(0, 2, 9.0)
    .add_arc(0, 5, 14.0)
    .add_arc(1, 3, 15.0)
    .add_arc(2, 3, 12.0)
    .add_arc(2, 5, 2.0)
    .add_arc(3, 4, 6.0)
    .add_arc(5, 4, 9.0);

auto [graph, length_map] = builder.build();
```

The builder is variadic in its arc properties: `static_digraph_builder<static_digraph, double, std::string>` would take an extra argument per `add_arc` and return an extra map from `build()`. With no property at all, `build()` still returns a tuple, so the idiom is `auto [graph] = builder.build();`.

!!! warning "`build()` renumbers the arcs"

    Arcs are sorted by source, then by target, before the graph is built. An
    arc's identifier is therefore its rank in that order, not its insertion
    index. The property maps are permuted along with it, so `length_map[a]`
    is always the right length for arc `a` — but do not record the value
    `add_arc` was called with as an arc id.

## What you got back

`graph` is a `static_digraph`. `length_map` is a plain `std::vector<double>`, which is all melon needs: it is indexed by arc, and `std::vector` already models [`mapping`](../graphs/mappings.md).

The two type aliases you will use everywhere are `vertex_t<G>` and `arc_t<G>` — for `static_digraph` both are `unsigned int`, which is why the sources below are written `0u`:

```cpp
static_assert(std::same_as<vertex_t<static_digraph>, unsigned int>);
static_assert(std::same_as<arc_t<static_digraph>, unsigned int>);
```

## Looking around

Every accessor is a free function in `namespace melon`, called unqualified thanks to the `using namespace melon` above:

```cpp
std::println("{} vertices, {} arcs", num_vertices(graph), num_arcs(graph));

for(auto && v : vertices(graph)) {
    std::println("vertex {} has out-degree {}", v, out_degree(graph, v));

    for(auto && a : out_arcs(graph, v))
        std::println("  arc {} to {} of length {}", a,
                     arc_target(graph, a), length_map[a]);

    // same traversal, arcs elided
    for(auto && w : out_neighbors(graph, v)) std::println("  -> {}", w);
}

// every arc with both its endpoints, in one range
for(auto && [a, st] : arcs_entries(graph)) {
    auto && [s, t] = st;
    std::println("arc {}: {} -> {}", a, s, t);
}
```

`static_digraph` stores in-arcs too, so `in_arcs`, `in_neighbors` and `in_degree` work as well. A structure that only stores out-arcs would still answer `out_neighbors`, `arcs` and `arcs_entries` — melon [synthesizes them](../reference/customization-points.md) — but calling `in_arcs` on it would be a compile error, which is exactly the point.

## Running an algorithm

`dijkstra` deduces its template arguments from its constructor, so you name the class and pass the graph, the length map, and a source:

```cpp
#include "melon/algorithm/dijkstra.hpp"

for(auto && [v, dist] : dijkstra(graph, length_map, 0u)) {
    std::println("vertex {} at distance {}", v, dist);
}
```

The loop drives the search: each iteration settles one vertex and yields it with its final distance, in nondecreasing order. Nothing runs before the loop starts, and `break` stops the search where it stands — no visitor and no callback needed. That is worth using:

```cpp
// stop as soon as the target is settled
for(auto && [v, dist] : dijkstra(graph, length_map, 0u)) {
    if(v == 4u) { std::println("d(0, 4) = {}", dist); break; }
}
```

The object can also be held and stepped by hand, which is what you want when two searches must advance together — see [Algorithms are ranges](../algorithms/index.md). It is move-only — a copy is a compile error — and `reset()` restores the constructor's state while reusing its allocations; `add_source` asserts the vertex is untouched ([The 1.0 contract](../contract.md)).

## Distances and paths

The default Dijkstra keeps no history: it holds one status map and one heap, and forgets a vertex once it has yielded it. Asking for `dist(v)` or `path_to(v)` after the fact requires telling it to remember, through a [traits type](../algorithms/shortest-paths.md#traits):

```cpp
struct my_traits : dijkstra_default_traits<static_digraph, double> {
    static constexpr bool store_distances = true;
    static constexpr bool store_paths = true;
};

dijkstra alg(my_traits{}, graph, length_map, 0u);
alg.run();

std::println("d(0, 4) = {}", alg.dist(4u));
for(auto && a : alg.path_to(4u))  // arcs, from the target back to the source
    std::println("  via arc {}", a);
```

`dist()` and `path_to()` are constrained on those flags, so the default configuration does not merely assert at runtime — the members are not there to call.

## Attaching your own data

Ask the graph for the map; it picks the storage:

```cpp
auto visits = create_vertex_map<int>(graph, 0);   // one int per vertex, zeroed
auto in_tree = create_arc_map<bool>(graph, false);

visits[3u] += 1;
in_tree[0u] = true;

static_assert(std::same_as<decltype(visits), vertex_map_t<static_digraph, int>>);
```

For `static_digraph` those are `static_map`s — a single allocation, indexed in constant time. A graph with `std::string` vertices could return a hash map instead, and the code above would not change. That indirection is what lets an algorithm require `has_vertex_map<G, int>` and work on both.

## Running on a view

`views::reverse(g)` is a graph whose arcs point the other way. It holds a reference and rewrites nothing, so running Dijkstra on it computes distances *to* the source instead of *from* it:

```cpp
#include "melon/views/reverse.hpp"

for(auto && [v, dist] : dijkstra(views::reverse(graph), length_map, 4u)) {
    std::println("vertex {} is at distance {} from 4", v, dist);
}
```

Views are graphs, so they nest — and, as in `std::ranges`, they pipe. These two lines describe the same adapted graph, and for a plain adaptor like `views::reverse` the call and pipe spellings even build exactly the same type:

```cpp
auto v1 = views::reverse(views::subgraph(g, keep_vertex));
auto v2 = g | views::subgraph(keep_vertex) | views::reverse;
```

(The one difference: `views::subgraph(keep_vertex)` used as a pipe stage keeps its own copy of the filter, so the pipeline can never outlive it.)

Passed an ordinary (lvalue) graph, a view holds a reference to it; passed a temporary, it takes ownership — the same rule as `std::views::all`, and you rarely need to think about it. When you do (returning a view from a function, storing one in a class), [Ownership and mapping views](../views/ownership.md) is the chapter that spells the rules out. See [Graph views](../views/graphs.md) for every adaptor.

The length map does not have to be a container either — a callable works directly, and no storage is materialized at all:

```cpp
// unit lengths, computed on the fly
for(auto && [v, hops] : dijkstra(graph, [](auto &&) { return 1; }, 0u))
    std::println("vertex {} at {} hops", v, hops);
```

The lambda is wrapped into a mapping for you: every algorithm routes its map arguments through `maps::mapping_all`, which [subscripts a callable by calling it](../graphs/mappings.md#mapping-views). The same holds for a `std::map`, whose `operator[]` is not const-callable — though the wrap reads via `at()` only when the wrapped map is const; a wrapped non-const `std::map` lvalue still uses the inserting `operator[]` ([Mappings](../graphs/mappings.md) has the details).

## When the graph must change

`static_digraph` cannot grow. `mutable_digraph` can: it supports vertex and arc creation and removal, and re-pointing an arc's endpoints.

```cpp
#include "melon/container/mutable_digraph.hpp"
#include "melon/graph.hpp"   // container headers do not pull in the free functions

mutable_digraph g;
auto a = create_vertex(g);
auto b = create_vertex(g);
auto c = create_vertex(g);

auto ab = create_arc(g, a, b);
create_arc(g, a, c);
create_arc(g, c, b);

remove_arc(g, ab);
remove_vertex(g, c);
```

Removals do not renumber what remains, so identifiers stay valid — but they leave holes in the identifier space. `vertices(g)` is unaffected: it is always a walk over an intrusive linked list, and a removal simply unlinks the vertex. [Graph containers](../containers/graphs.md) compares the two structures and their costs.

When the mutations are done, the graph can be compacted back into a dense `static_digraph`:

```cpp
#include "melon/utility/make_static_digraph.hpp"

auto [sg] = make_static_digraph(g);
```

`make_static_digraph` rebuilds any outward-incidence graph with vertices renumbered `0..n-1` — holes closed — and returns the builder's tuple shape. It takes three optional arguments beyond the graph: a comparator that chooses the new vertex order, a tuple of vertex maps, and a tuple of arc maps to translate onto the new identifiers ([Rebuilding as a static_digraph](../containers/graphs.md#rebuilding-as-a-static_digraph) has the details):

```cpp
auto [sg, deg_map, len_map] = make_static_digraph(
    g, by_degree_cmp, std::tie(degree_map), std::tie(length_map));
```

The maps are only read (any [`mapping`](../graphs/mappings.md) works), and each comes back as a `static_map` over the new handles.

## Next steps

- [Graph concepts](../graphs/concepts.md) — what `graph`, `outward_incidence_graph` and friends actually require, and why the model is a directed multigraph.
- [Algorithms are ranges](../algorithms/index.md) — the generator protocol, and what to do when you need more than a `for` loop.
- [Coming from Boost.Graph or LEMON](coming-from.md) — if you have written this program before, in another library.
