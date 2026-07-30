# Graph containers

melon ships three digraph implementations. They differ in what they store, and therefore in what [concepts](../graphs/concepts.md) they satisfy — which is the point: pick the smallest structure that answers the questions your algorithm asks, and the compiler enforces the choice.

| | `static_digraph` | `static_forward_digraph` | `mutable_digraph` |
| --- | --- | --- | --- |
| Header | `container/static_digraph.hpp` | `container/static_forward_digraph.hpp` | `container/mutable_digraph.hpp` |
| Identifiers | consecutive from 0 | consecutive from 0 | stable, with holes |
| Out-arcs | ✓ | ✓ | ✓ |
| In-arcs | ✓ | | ✓ |
| `arc_source` | ✓ | | ✓ |
| `out_degree` in O(1) | ✓ | ✓ | |
| Modifiable | | | ✓ |
| Storage per arc | 3 integers | 1 integer | 6 integers + 1 bit |
| Storage per vertex | 2 integers | 1 integer | 4 integers + 1 bit |

## `static_digraph`

An immutable compressed-adjacency structure whose vertices and arcs are consecutive `unsigned int`s starting at 0. It supports every common lookup with good constants:

- iterate over vertices and over arcs — both are `iota` ranges, so random-access and sized;
- source and target of a given arc;
- outgoing and incoming arcs of a given vertex — contiguous subranges;
- degrees in constant time.

Internally it holds an out-arc offset array, the arc source and target arrays, an in-arc offset array, and the permutation listing in-arcs. Arcs of a vertex are consecutive integers, which is what makes the traversal loops prefetch-friendly.

### Building one

The [builder](#the-builder) is the usual route. The direct constructor is available when you already hold the endpoint arrays:

```cpp
std::vector<unsigned int> sources = {0, 0, 1, 2, 2};
std::vector<unsigned int> targets = {1, 2, 2, 0, 1};
static_digraph graph(3, sources, targets);
```

!!! warning

    The constructor requires `sources` to be **sorted**, and every endpoint to
    be less than `num_vertices`. Both are checked by `assert`, so a release
    build will silently produce a corrupt structure instead. Prefer the
    builder, which sorts for you.

## `static_forward_digraph`

The same compressed layout with the reverse index and the source array dropped: one integer per arc instead of three. It answers `vertices`, `arcs`, `out_arcs`, `arc_target`, `out_neighbors` and `out_degree`, and nothing about the reverse direction.

```cpp
static_assert(outward_incidence_graph<static_forward_digraph>);
static_assert(!has_arc_source<static_forward_digraph>);
static_assert(!inward_incidence_graph<static_forward_digraph>);
```

That is enough for Dijkstra, BFS, DFS, topological sort and strongly connected components. It is not enough for anything that walks backwards — [`bidirectional_dijkstra`](../algorithms/shortest-paths.md#bidirectional_dijkstra) and the flow algorithms — nor for [`views::undirect`](../views/graphs.md#undirect). Use it when the graph is large, the traversal is forward-only, and memory is the constraint.

!!! note

    Storing paths in an algorithm is one of the things that silently needs the
    reverse direction: `dijkstra` with `store_paths = true` keeps an explicit
    predecessor-vertex map when the graph has no `arc_source`, and only
    predecessor arcs when it does. It works either way; it just costs one more
    map on a forward-only graph.

## `mutable_digraph`

The structure to use when the topology changes. Vertices and arcs are integers again, but the incidence lists are intrusive doubly-linked lists threaded through the arc records, so insertion and removal are O(1) and do not move anything.

```cpp
#include "melon/container/mutable_digraph.hpp"
#include "melon/graph.hpp"

mutable_digraph g;
auto a = create_vertex(g);
auto b = create_vertex(g);
auto c = create_vertex(g);

auto ab = create_arc(g, a, b);
auto ac = create_arc(g, a, c);
auto cb = create_arc(g, c, b);

change_arc_target(g, ac, b);   // re-point an arc without recreating it
remove_arc(g, ab);
remove_vertex(g, c);           // also removes every arc incident to c, here cb
```

!!! note

    Container headers do not include `melon/graph.hpp`, so the free functions
    — `create_vertex`, `vertices`, `num_vertices`, `create_vertex_map` — are
    not in scope from `container/mutable_digraph.hpp` alone. Any algorithm
    header, or the builder, pulls it in; when you include only a container,
    include `melon/graph.hpp` too.

Two consequences of the design are worth knowing:

**Identifiers survive removals, so they get holes.** Removing a vertex or an arc does not renumber the rest — an identifier you stored stays valid — but the live identifiers are then no longer consecutive. `vertices(g)` and `arcs(g)` are filtered ranges rather than `iota`s, and `is_valid_vertex(g, v)` / `is_valid_arc(g, a)` are how you tell a live identifier from a stale one.

**Degrees are not O(1).** The incidence lists are not sized ranges, so `mutable_digraph` does not satisfy `has_out_degree` or `has_in_degree`. Count with `std::ranges::distance(out_arcs(g, v))` when you must, and prefer an algorithm that does not need degrees.

Vertex and arc maps are still handed out by the graph, and are sized to the current identifier space:

```cpp
auto mark = create_vertex_map<bool>(g, false);
```

## The builder

`static_digraph_builder<G, Properties...>` collects arcs and their per-arc data, then produces the graph and one map per property in a single pass.

```cpp
#include "melon/utility/static_digraph_builder.hpp"

static_digraph_builder<static_digraph, double, std::string> builder(6);

builder.add_arc(0, 1, 7.0, "a")
       .add_arc(2, 5, 2.0, "b")
       .add_arc(0, 2, 9.0, "c");

auto [graph, length_map, name_map] = builder.build();
```

- `add_arc` returns the builder, so calls chain.
- `build()` returns a `std::tuple` — with no properties it is a one-element tuple, so the idiom stays `auto [graph] = builder.build();`.
- The property maps are `std::vector<Property>`, which is an `output_mapping` and a `contiguous_mapping`; nothing else is required of them.
- The builder works for any `G` constructible from `(num_vertices, sources, targets)` — `static_forward_digraph` as well as `static_digraph`.

**`build()` sorts the arcs** by source, then by target, and permutes the property maps along with them. An arc's final identifier is its rank in that order, not the order you called `add_arc` in. `length_map[a]` is always correct for arc `a`; what you must not do is remember an insertion index and use it as an arc later.

## Generating a graph

`erdos_renyi<G>(n, p)` builds a random digraph on `n` vertices, including each of the `n(n-1)` possible arcs independently with probability `p`:

```cpp
#include "melon/utility/erdos_renyi.hpp"

auto graph = erdos_renyi<static_digraph>(1000, 0.01);
```

It uses a static `std::mt19937` seeded from `std::random_device`, so the sequence is not reproducible across runs and the generator is not thread-safe. For a controlled experiment, build the arcs yourself with your own engine and feed the builder.

## Printing a graph

`graphviz_printer<G>` renders a graph to a DOT stream, with optional per-vertex and per-arc labels, positions, sizes and colors. Every setter takes a [mapping](../graphs/mappings.md), so a lambda wrapped in `maps::map` is enough and no map has to be materialized.

```cpp
#include <iterator>

#include "melon/utility/graphviz_printer.hpp"

using color = std::tuple<unsigned char, unsigned char, unsigned char>;

graphviz_printer printer(graph);
printer.set_vertex_label_map(maps::map([](auto && v) { return std::to_string(v); }))
       .set_arc_color_map(maps::map([&](auto && a) -> color {
           return in_tree[a] ? color{255, 0, 0} : color{64, 64, 64};
       }))
       .print(std::ostream_iterator<char>(std::cout));
```

Colors are `(r, g, b)` triples of `unsigned char`; vertex positions are `(x, y)` pairs of `double` and are scaled to the page size set by `page_size(width, height)`.

!!! note

    Unlike the algorithms, these setters take a `mapping` **directly**
    and do not apply `maps::mapping_all` themselves — so a callable must be
    wrapped in `maps::map` here, where an algorithm would have accepted it
    bare.

!!! note

    `print` writes through `std::format_to`, so it takes an **output
    iterator**, not a stream — `std::ostream_iterator<char>(std::cout)`, or a
    `std::back_inserter` into a string. Passing `std::cout` directly is a
    long template error from inside `<format>`.

## Choosing

- Topology fixed, both directions needed → **`static_digraph`**.
- Topology fixed, forward traversal only, memory tight → **`static_forward_digraph`**.
- Topology changes → **`mutable_digraph`**.
- Topology is a *restriction* of another graph → do not build anything, use [`views::subgraph`](../views/graphs.md#subgraph).
- Topology is implicit (a complete graph, a grid) → [`views::complete_digraph`](../views/graphs.md#complete_digraph), or [your own type](../graphs/custom-graphs.md).
