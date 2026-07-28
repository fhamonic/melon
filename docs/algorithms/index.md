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
auto backward = dijkstra(views::reverse(graph), length_map, t);
while(!forward.finished() && !backward.finished()) {
    forward.advance();
    backward.advance();
}
```

!!! note

    The second declaration is written `auto backward = dijkstra(...)` rather
    than `dijkstra backward(...)` on purpose: with a class-template argument
    like `views::reverse(graph)` in the initializer, the direct-initialization
    form is parsed as a function declaration. The range-`for` and assignment
    forms are unambiguous.

The fourth is not hypothetical: it is exactly how [`bidirectional_dijkstra`](shortest-paths.md#bidirectional_dijkstra) and [`competing_dijkstras`](shortest-paths.md#competing_dijkstras) are built.

!!! note "Input range, single pass"

    The iterator is an `std::input_iterator`: the algorithm has one state, and
    iterating consumes it. Two iterators over the same algorithm object advance
    the same search, and there is no way to restart short of `reset()`.

## Which algorithms are ranges

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

Even the range-shaped ones offer `run()` — `while(!finished()) advance();` — for when you want the side effects and the accessors but not the values.

## Construction and deduction

Every algorithm deduces its template parameters from its constructor arguments. You name the class, not its parameters:

```cpp
dijkstra alg(graph, length_map, source);
edmonds_karp flow(graph, capacity_map, source, target);
kruskal tree(ugraph, cost_map);
```

The graph and the mappings go through [`views::graph_all` and `views::mapping_all`](../views/ownership.md), so an lvalue is referenced and an rvalue is owned. That is why the deduction guides are written in terms of `views::graph_all_t<_Graph>` and why passing a temporary graph is safe.

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

**Unavailable accessors do not exist.** `dist()` and `path_to()` carry a `requires(_Traits::store_distances)` / `requires(_Traits::store_paths)` clause. Calling them on a default-configured algorithm is a compile error naming the flag, not an assertion at runtime.

The flags available per algorithm are listed on each algorithm's page. The data-structure slots — the heap type, the semiring, the index map — are described under [Shortest paths](shortest-paths.md#traits).

## A note on `assert`

melon's algorithms use `assert` for their preconditions: `current()` on a finished generator, `dist(v)` on an unvisited vertex, `promote` in the wrong direction. These vanish under `NDEBUG`, which is the default in a Release build. Run your test suite without `NDEBUG` at least once — the test suite of melon itself starts every file with `#undef NDEBUG` for exactly that reason.
