# Performance

melon's stated goal is to be as fast as [LEMON](https://lemon.cs.elte.hu/trac/lemon) while being pleasanter to use than [Boost.Graph](https://www.boost.org/doc/libs/release/libs/graph/). This page describes the mechanisms that make that plausible, and what you have to do to get them.

**Measured numbers live in a separate repository.** Benchmarks against Boost.Graph and LEMON — with their instances, methodology and raw timings — are at [fhamonic/melon_benchmark](https://github.com/fhamonic/melon_benchmark). Nothing on this page is a substitute for measuring your own workload.

## Where the speed comes from

### No indirection to erase

Every graph, mapping and algorithm is a template parameter. There is no virtual dispatch, no type erasure, no `std::function` in a hot loop, and no property-map lookup through a tag. `melon::out_arcs(g, v)` is a customization point object whose `operator()` forwards to a member call, which inlines to a pair of pointer arithmetic operations for `static_digraph`. The cost of the genericity is paid entirely at compile time.

### Layout that the prefetcher can follow

`static_digraph` is a compressed adjacency structure: the out-arcs of a vertex are a *consecutive range of integers*, and the arc data indexed by them is a flat array. Iterating a vertex's neighborhood is a linear scan of two arrays.

The maps handed out by `create_vertex_map` and `create_arc_map` are [`static_map`](containers/data-structures.md#static_map)s — a single allocation, no capacity slack, `data()` exposed — which is exactly the `contiguous_mapping` requirement.

### Explicit prefetching

Before relaxing a vertex, `dijkstra` and the flow algorithms issue `__builtin_prefetch` for the ranges and mapped values they are about to read:

```cpp
auto && out_arcs_range = melon::out_arcs(_graph, t);
prefetch_range(out_arcs_range);
prefetch_mapped_values(out_arcs_range, arc_targets_map(_graph));
prefetch_mapped_values(out_arcs_range, _length_map);
```

Both helpers are guarded by `if constexpr` on contiguity: on a non-contiguous map, or on a non-GCC/Clang compiler, they compile to nothing. This is the concrete pay-off of the [`contiguous_mapping`](graphs/mappings.md#reading-writing-prefetching) concept — and the reason a `std::map` length map is not merely inconvenient but measurably slower.

### Array lookups instead of hash lookups

An updatable heap needs to know where each element currently sits. The generic default is a hash map; `dijkstra_default_traits` instead uses `vertex_map_t<_Graph, std::size_t>`, so for melon's containers the lookup is an array index. The same substitution is available for [`disjoint_sets`](containers/data-structures.md#disjoint_sets) and any other structure taking an index map.

### You do not pay for what you do not ask for

Optional state is held in `[[no_unique_address]]` members that collapse to zero bytes when their traits flag is `false`, and the accessors that would read them are removed by `requires` clauses. A default `dijkstra` carries one status map and one heap — no predecessor map, no distance map.

The same applies to views: [`views::subgraph`](views/graphs.md#subgraph) with the default `true_map` filters forwards every accessor unchanged and adds no `filter_view` layer, because `true_map` is an empty type and the specialization is chosen with `if constexpr`.

### Algorithm-specific tricks

- [`breadth_first_search`](algorithms/traversals.md#breadth_first_search) selects a **branchless** implementation — flat preallocated queue, no bounds checks — when the graph knows its vertex count and nothing optional is stored.
- [`dinitz`](algorithms/flows-and-trees.md#dinitz) keeps a *consumable view* of each vertex's remaining arcs, so a saturated arc is never rescanned within a phase.
- DFS and Tarjan's SCC are iterative, so recursion depth is heap memory rather than stack frames.

### Not computing what you do not need

The largest constant factor is usually the work you avoid. Because algorithms are [ranges](algorithms/index.md), `break` is a full stop:

```cpp
for(auto && [v, dist] : dijkstra(graph, length_map, s)) {
    if(v == target) break;      // the rest of the graph is never touched
}
```

and views mean a restricted problem needs no copy of the graph:

```cpp
dijkstra(views::subgraph(graph, keep), length_map, s);   // no rebuild
```

## Getting it in practice

**Compile with optimizations and `NDEBUG`.** melon uses `assert` for preconditions, and the traversal loops contain several. A `-O0` build with assertions is not a measurement of anything.

**Pick the narrowest container.** [`static_forward_digraph`](containers/graphs.md#static_forward_digraph) stores one integer per arc against `static_digraph`'s three. If the traversal is forward-only, that is two thirds of the arc memory saved, and memory is what large graph traversals are bound by.

**Let the graph give you the maps.** `create_vertex_map<T>(g)` returns a contiguous map for melon's containers. A `std::map` or `std::unordered_map` of your own is [accepted](graphs/mappings.md#stdmap-does-not-satisfy-input_mapping) but is not contiguous, so it is never prefetched — and every lookup is a hash or a tree descent where the flat map is an array index.

**Try a 4-ary heap.** `dijkstra_default_traits` uses a binary heap; on large sparse graphs a 4-ary heap usually reduces the number of sift-down comparisons that miss cache. It is a one-character change in a traits struct.

**Do not enable `store_paths` or `store_distances` you will not read.** Each costs a map allocation and a write per settled vertex.

**Use `bidirectional_dijkstra` for point-to-point queries.** A one-sided search settles every vertex closer than the target; a two-sided one settles roughly the vertices within half the distance of each endpoint.

**Reuse the algorithm object.** `reset()` keeps the graph, the maps and the heap storage, so a loop over sources allocates once:

```cpp
dijkstra alg(graph, length_map);
for(auto && s : sources) {
    alg.reset().add_source(s);
    alg.run();
}
```

## Compile time

The price of a header-only, concept-heavy design is compilation. Two habits help: include the specific headers rather than `melon/all.hpp`, and instantiate an algorithm on a small number of concrete graph types rather than in a template that every caller re-instantiates. Concept diagnostics are cheaper to read than SFINAE failures, but they are not cheaper to *compile* — a wrong constraint still forces the compiler through the whole disjunction, which is why the [customization-point fallbacks](reference/customization-points.md) are worth understanding rather than fighting.
