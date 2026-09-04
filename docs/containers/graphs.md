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
| Storage per arc | 3 handles | 1 handle | 6 handles + 1 bit |
| Storage per vertex | 2 handles | 1 handle | 4 handles + 1 bit |
| Template | `basic_static_digraph<V, A>` | `basic_static_forward_digraph<V, A>` | `basic_mutable_digraph<V, A>` |

Each name is an alias of the class template on its row instantiated at `unsigned int` for both handle types; see [handle types](#handle-types) for choosing others.

## `static_digraph`

An immutable compressed-adjacency structure whose vertices and arcs are consecutive integers starting at 0 — `unsigned int`s unless you [pick another handle type](#handle-types). It supports every common lookup with good constants:

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

The same compressed layout with the reverse index and the source array dropped: one handle per arc instead of three. It answers `vertices`, `arcs`, `out_arcs`, `arc_target`, `out_neighbors` and `out_degree`, and nothing about the reverse direction.

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

## Handle types

The three containers are aliases of class templates over their vertex and arc handle types, the way `std::string` is `std::basic_string<char>`:

```cpp
template <std::unsigned_integral V = unsigned int,
          std::unsigned_integral A = unsigned int>
class basic_static_digraph;
using static_digraph = basic_static_digraph<>;
// likewise basic_static_forward_digraph / static_forward_digraph
//      and basic_mutable_digraph / mutable_digraph
```

`V` is the vertex type and `A` the arc type, both **unsigned**: every bound in the containers is a `<` comparison, and the intrusive lists of `mutable_digraph` use the handle's maximum as their null marker, so a signed handle is rejected at instantiation. Everything downstream is generic over `vertex_t<G>` and `arc_t<G>`, so the algorithms, the [builder](#the-builder), [`make_static_digraph`](#rebuilding-as-a-static_digraph) and the views take a non-default instantiation without further ado.

Pick the width from the graph size and the memory budget:

```cpp
// more than 2^32 arcs: 64-bit arc handles, vertices still fit in 32 bits
basic_static_digraph<unsigned int, std::uint64_t> huge(n, sources, targets);

// many small graphs held at once: 16-bit handles halve every array and map
basic_static_forward_digraph<std::uint16_t, std::uint16_t> tiny(n, sources, targets);

// arc maps are keyed on the chosen type
auto length = create_arc_map<double>(huge);   // static_map<std::uint64_t, double>
```

The handle type bounds the graph. The static containers accept up to `std::numeric_limits<V>::max()` vertices and `std::numeric_limits<A>::max()` arcs; `mutable_digraph` one fewer of each, since the maximum is its null marker. Past that the constructor, or `create_vertex` / `create_arc`, `assert`s rather than wrapping to a corrupt structure.

!!! note "Spell the alias, do not forward-declare it"

    `static_digraph`, `static_forward_digraph` and `mutable_digraph` are
    aliases of class template instantiations, not classes:
    `class static_digraph;`
    in your code does not compile. Include the header instead.

## The builder

`static_digraph_builder<G, Properties...>` collects arcs and their per-arc data, then produces the graph and one map per property in a single pass.

```cpp
#include "melon/utility/static_digraph_builder.hpp"

static_digraph_builder<static_digraph, double, std::string> builder(6);

builder.add_arc({0, 1}, 7.0, "a")
       .add_arc({2, 5}, 2.0, "b")
       .add_arc({0, 2}, 9.0, "c");

auto [graph, length_map, name_map] = builder.build();
```

- `add_arc` takes the endpoints as one pair, `{source, target}`, followed by one value per property, so the call shows where the topology stops and the data begins. A builder without properties also accepts the plain `add_arc(source, target)`, since nothing can follow the endpoints there. It returns the builder, so calls chain.
- `add_arcs` appends a whole range at once: a range of endpoint pairs on a property-less builder, otherwise a range of tuple-likes holding the pair and then the property values — the shape `std::views::zip` produces. The properties are copied out of the entries; pipe the range through `std::views::as_rvalue` to move them out instead. Copying the arcs of an existing graph is one call:

    ```cpp
    static_digraph_builder<static_digraph, double> copy(num_vertices(g));
    copy.add_arcs(std::views::zip(
        arcs_entries(g) | std::views::values,
        arcs(g) | std::views::transform([&](auto a) { return length[a]; })));
    ```
- `build()` returns a `std::tuple` — with no properties it is a one-element tuple, so the idiom stays `auto [graph] = builder.build();`.
- Both are ref-qualified: `build()` on an lvalue builder copies the property vectors, `std::move(builder).build()` — or a whole chain started from a temporary, `static_digraph_builder<G, P>(n).add_arc(…).build()` — moves them out and leaves the builder moved-from. `build()` is not idempotent either way: it sorts in place.
- The property maps are `std::vector<Property>`, which is an `output_mapping` and a `contiguous_mapping`; nothing else is required of them.
- The builder works for any `G` constructible from `(num_vertices, sources, targets)` — `static_forward_digraph` as well as `static_digraph`.

**`build()` sorts the arcs** by source, then by target, and permutes the property maps along with them. An arc's final identifier is its rank in that order, not the order you called `add_arc` in. `length_map[a]` is always correct for arc `a`; what you must not do is remember an insertion index and use it as an arc later.

## Rebuilding as a static_digraph

`make_static_digraph` rebuilds any outward-incidence graph as a `static_digraph`, renumbering the vertices `0..n-1` and translating any maps you pass onto the new identifiers. It returns the builder's tuple shape:

```cpp
#include "melon/utility/make_static_digraph.hpp"

// compact a mutable_digraph after removals: holes closed, identifiers dense
auto [sg] = make_static_digraph(g);

// choose the new vertex order and carry maps across
auto [sg, new_weights, new_lengths] = make_static_digraph(
    g, by_degree_cmp, std::tie(weight_map), std::tie(length_map));
```

Everything after the graph is optional. The second argument is a strict weak order on the old vertices — new vertex 0 is the smallest under it (`std::less` by default); the comparator is taken by value, so a stateful one is copied. The last two are tuples of vertex maps and arc maps; each comes back as a `static_map` over the new handles, in the same position of the returned tuple. Any [mapping](../graphs/mappings.md) can go in — the maps are only read, so `std::tie` / `std::forward_as_tuple` pass them without copying, and `std::make_tuple` hands the call ownership of a map you no longer need. The vertex and arc counts must fit `static_digraph`'s handle type, asserted in debug builds.

The two use cases it exists for:

- **Compacting.** A `mutable_digraph` accumulates holes as it edits; once the topology settles, one call produces the dense, cache-friendly structure the algorithms run fastest on.
- **Reordering.** The vertex order of a `static_digraph` is its memory layout. Renumbering along a better order (by degree, by BFS discovery, by geometric proximity) is a locality optimization the comparator expresses directly.

!!! warning "Every identifier changes"

    Like the builder, the rebuild renumbers vertices *and* arcs. The maps you
    pass in are translated for you; a vertex or arc identifier you stored
    anywhere else refers to the old graph only. The correspondence is not
    returned — if you need it, pass it in as data: a vertex map holding each
    vertex's own identifier comes back as the new-to-old table.

It requires `outward_incidence_graph` (the rebuild walks `out_arcs` and `arc_target`) and `has_vertex_map` — the concepts remove the overload otherwise. Arcs are emitted grouped by new source in the order the old graph lists them, which satisfies the sorted-sources precondition of `static_digraph`'s constructor; within one source they are *not* sorted by target the way the builder sorts them.

## Generating a graph

`erdos_renyi<G>(n, p)` builds a random digraph on `n` vertices, including each of the `n(n-1)` possible arcs independently with probability `p`:

```cpp
#include "melon/utility/erdos_renyi.hpp"

std::mt19937 gen{42};
auto graph = erdos_renyi<static_digraph>(1000, 0.01, gen);   // reproducible
auto quick = erdos_renyi<static_digraph>(1000, 0.01);        // seeded from random_device
```

The three-argument overload takes your generator by reference — the caller owns the seed, so it is the reproducible form and the one safe to call concurrently (each thread with its own generator). The two-argument convenience overload seeds a *local* engine from `std::random_device` per call: thread-safe, but not reproducible.

## Printing a graph

`graphviz_printer<G>` renders a graph to a DOT stream, with optional per-vertex and per-arc labels, positions, sizes and colors. Every setter takes a [mapping](../graphs/mappings.md), so a lambda wrapped in `maps::function` is enough and no map has to be materialized. The constructor is `explicit`, references the graph, and refuses a temporary one (the rvalue overload is deleted — the printer would dangle).

```cpp
#include <iterator>

#include "melon/utility/graphviz_printer.hpp"

using color = std::tuple<unsigned char, unsigned char, unsigned char>;

graphviz_printer printer(graph);
printer.set_vertex_label_map(maps::function([](auto && v) { return std::to_string(v); }))
       .set_arc_color_map(maps::function([&](auto && a) -> color {
           return in_tree[a] ? color{255, 0, 0} : color{64, 64, 64};
       }))
       .print(std::ostream_iterator<char>(std::cout));
```

Colors are `(r, g, b)` triples of `unsigned char`; vertex positions are `(x, y)` pairs of `double` and are scaled to the page size set by `page_size(width, height)`.

!!! note

    Unlike the algorithms, these setters take a `mapping` **directly**
    and do not apply `maps::mapping_all` themselves — so a callable must be
    wrapped in `maps::function` here, where an algorithm would have accepted it
    bare.

!!! note

    `print` writes through `std::format_to`, so it takes an **output
    iterator**, not a stream — `std::ostream_iterator<char>(std::cout)`, or a
    `std::back_inserter` into a string. Passing `std::cout` directly is a
    long template error from inside `<format>`.

## Choosing

- Topology fixed, both directions needed → **`static_digraph`**.
- Topology fixed, forward traversal only, memory tight → **`static_forward_digraph`**.
- Topology changes → **`mutable_digraph`**; once it settles, [compact it](#rebuilding-as-a-static_digraph).
- Topology is a *restriction* of another graph → do not build anything, use [`views::subgraph`](../views/graphs.md#subgraph).
- Topology is implicit (a complete graph, a grid) → [`views::complete_digraph`](../views/graphs.md#complete_digraph), or [your own type](../graphs/custom-graphs.md).
