# Algorithms are ranges

Most graph libraries express "do something at each step of a traversal" with a visitor class, a callback, or a set of named parameters. melon inverts the control flow: an algorithm is an object you *step*, and the loop body is the visitor.

## The generator protocol

```cpp
template <typename A>
concept algorithmic_generator = requires(A alg) {
    { alg.finished() } -> std::convertible_to<bool>;
    alg.current();
    alg.advance();
};
```

Three members: is there anything left, what is the current result, move on. `algorithm_view_interface` turns that into a `std::ranges` input range by wrapping the object in an `algorithm_iterator` whose `operator*` is `current()`, `operator++` is `advance()`, and whose sentinel comparison is `finished()`.

The consequences are the point:

```cpp
// 1. consume it as a range
for(auto && [v, dist] : dijkstra(graph, length_map, s)) { ... }

// 2. stop where you like — no exception, no visitor return code
for(auto && [v, dist] : dijkstra(graph, length_map, s)) {
    if(dist > radius) break;
}

// 3. compose with std::views
auto far = dijkstra(graph, length_map, s)
         | std::views::drop_while([](auto && e) { return e.second < 10; })
         | std::views::take(5);

// 4. drive two searches in lockstep
dijkstra forward(graph, length_map, s);
dijkstra backward(graph | views::reverse, length_map, t);
while(!forward.finished() && !backward.finished()) {
    forward.advance();
    backward.advance();
}
```

!!! note "Pipelines see the algorithm, not a copy"

    In the third example the pipeline holds a *reference* to the temporary
    algorithm, so iterating `far` advances that search. An algorithm is a
    range but deliberately **not** a `std::ranges::view` — it carries the
    whole search state, which is too heavy to copy silently. If you are
    unsure what gets copied and what gets referenced when composing melon
    objects, [Ownership and mapping views](../views/ownership.md) is the
    chapter that spells out the rules once, for everything.

The fourth is not hypothetical: it is exactly how [`bidirectional_dijkstra`](shortest-paths.md#bidirectional_dijkstra) and [`competing_dijkstras`](shortest-paths.md#competing_dijkstras) are built.

!!! note "Input range, single pass"

    The iterator is an `std::input_iterator`: the algorithm has one state, and
    iterating consumes it. Two iterators over the same algorithm object advance
    the same search, and there is no way to restart short of `reset()`.

## Which algorithms are ranges

An algorithm is a range exactly when it derives from `algorithm_view_interface`, which is what supplies `begin()` / `end()`. The twelve below do; the rest do not, and a range-`for` over one of them is a compile error, not a silent single-pass.

| Algorithm | `current()` yields |
| --- | --- |
| [`breadth_first_search`](traversals.md#breadth_first_search) | a vertex |
| [`depth_first_search`](traversals.md#depth_first_search) | a vertex |
| [`topological_sort`](traversals.md#topological_sort) | a vertex |
| [`strongly_connected_components`](traversals.md#strongly_connected_components) | a range of vertices |
| [`connected_components`](traversals.md#connected-components) | a range of vertices |
| [`traversal_forest`](traversals.md#traversal_forest) | a range of vertices |
| [`dijkstra`](shortest-paths.md#dijkstra) | `(vertex, distance)` |
| [`network_voronoi`](shortest-paths.md#network_voronoi) | `(vertex, (distance, kernel))` |
| [`biobjective_dijkstra`](shortest-paths.md#biobjective_dijkstra) | a heap label |
| [`competing_dijkstras`](shortest-paths.md#competing_dijkstras) | a heap label |
| [`kruskal`](flows-and-trees.md#kruskal) | an edge |
| [`bentley_ottmann`](others.md#bentley_ottmann) | `(point, range of segment ids)` |

The rest produce a single answer rather than a sequence, so they expose `run()` and dedicated accessors instead: [`bidirectional_dijkstra`](shortest-paths.md#bidirectional_dijkstra), [`edmonds_karp`](flows-and-trees.md#edmonds_karp), [`dinitz`](flows-and-trees.md#dinitz), [`knapsack_bnb`](others.md#knapsack) and [`unbounded_knapsack_bnb`](others.md#knapsack).

Even the range-shaped ones offer `run()` — `while(!finished()) advance();` — for when you want the side effects and the accessors but not the values. It returns the algorithm, like `reset()`, so a run and a query chain: `alg.run().dist(t)`. The exceptions are the three whose `run()` means something else: `dinitz` and `edmonds_karp` are not generators at all, and `bidirectional_dijkstra::run()` returns the distance it computed.

`finished()` and `current()` are `const` on every generator, so a `const` reference to an algorithm is enough to inspect where it stands; `advance()`, `run()` and `reset()` are the mutating half. Copying is available too, with one boundary: an algorithm that caches incidence ranges — `depth_first_search`, `strongly_connected_components`, `connected_components`, `dinitz` — is copyable only over a graph satisfying `melon::borrowed_graph`, which excludes `views::subgraph`. See [Ownership](../views/ownership.md#copying-an-algorithm-enable_borrowed_graph). Moving is always available. Where `current()` hands back a window onto the algorithm's own buffer — the component of `strongly_connected_components` or `connected_components`, the tree of `traversal_forest` — that window is read-only, since the next `advance()` rewrites it. Where it hands back a single handle it hands back a *value*, never a reference into that buffer.

## `base()` and `reached_map()`

Every algorithm that stores a graph exposes `base()`, and every algorithm that answers `reached(v)` also answers `reached_map()`.

`base()` follows the `std::ranges` shape of whatever the type is. An algorithm *owns* its graph view, so its `base()` is the `std::ranges::owning_view` shape — four ref-qualified overloads returning references:

```cpp
auto alg = breadth_first_search(graph, 0u);
const auto & g = alg.base();          // the graph view it runs over
auto owned = std::move(alg).base();   // move it out of a finished algorithm
```

That is how `traversal_forest` reaches its sources without storing a second copy of the graph, and it is why `base()` here does not return a copy the way a *view*'s does — see [Ownership](../views/ownership.md).

`reached_map()` hands back a mapping over the same information `reached(v)` answers one vertex at a time, for passing to anything that takes a map. Some algorithms store that map (`breadth_first_search`, `depth_first_search`, `topological_sort`, `connected_components`) and some compute it from a status map (`dijkstra`, `network_voronoi`, `strongly_connected_components`); either way the result is a *view into the algorithm*, valid while it lives and stays put, exactly like every other melon map view.

## Construction and deduction

Every algorithm deduces its template parameters from its constructor arguments. You name the class, not its parameters:

```cpp
dijkstra alg(graph, length_map, source);
edmonds_karp flow(graph, capacity_map, source, target);
kruskal tree(ugraph, cost_map);
```

The graph and the mappings go through [`views::graph_all` and `maps::mapping_all`](../views/ownership.md), so an lvalue is referenced and an rvalue is owned. That is why the deduction guides are written in terms of `views::graph_all_t<Graph>` and why passing a temporary graph is safe.

Sources are usually optional constructor arguments, and can always be added afterwards:

```cpp
dijkstra alg(graph, length_map);
alg.add_source(s1);
alg.add_source(s2, 10.0);   // start s2 at a nonzero distance
alg.run();
```

`reset()` returns the object to its initial state, keeping the graph and the maps, so a loop over many sources allocates once:

```cpp
dijkstra alg(graph, length_map);
for(auto && s : terminals) {
    alg.reset().add_source(s);
    for(auto && [v, d] : alg) { ... }
}
```

## Traits

The second (or, for the multi-map algorithms, last) template parameter of most algorithms is a **traits** type that selects the data structures and what gets recorded. It is passed as a *first constructor argument*, which is what makes the deduction work:

```cpp
struct my_traits : dijkstra_default_traits<static_digraph, double> {
    static constexpr bool store_distances = true;
    static constexpr bool store_paths = true;
};

dijkstra alg(my_traits{}, graph, length_map, source);
```

Two things follow from the design.

**Unused storage costs nothing.** The optional maps are declared with `[[no_unique_address]]` and become empty types when their flag is `false`, so the default Dijkstra carries exactly one status map and one heap.

**Unavailable accessors do not exist.** `dist()` and `path_to()` carry a `requires(Traits::store_distances)` / `requires(Traits::store_paths)` clause. Calling them on a default-configured algorithm is a compile error naming the flag, not an assertion at runtime.

The flags available per algorithm are listed on each algorithm's page. The data-structure slots — the heap type, the semiring, the index map — are described under [Shortest paths](shortest-paths.md#traits).

## A note on `noexcept`

melon marks a function `noexcept` only when it can keep the promise. An algorithm's constructor, `reset()`, `add_source()`, `advance()` and `run()` are **not** `noexcept`: they allocate (the heap, the queue, the vertex maps) and they run your code — your length map, your semiring, your comparator, your graph's `out_arcs()`. A `noexcept` there would not prevent the throw, it would turn it into `std::terminate` with no diagnostic.

The observers are `noexcept` when their body allows it. Where a view forwards to the wrapped graph — `graph_ref_view::vertices()`, `reverse::arc_targets_map()`, every `create_*_map()` — the specification is *conditional*, `noexcept(noexcept(melon::vertices(*_graph)))`, so wrapping a graph in a view neither invents a guarantee the graph does not give nor throws one away that it does.

## A note on `assert`

melon's algorithms use `assert` for their preconditions: `current()` on a finished generator, `dist(v)` on an unvisited vertex, `promote` in the wrong direction. These vanish under `NDEBUG`, which is the default in a Release build. Run your test suite without `NDEBUG` at least once — the test suite of melon itself starts every file with `#undef NDEBUG` for exactly that reason.
