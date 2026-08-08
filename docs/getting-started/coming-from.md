# Coming from Boost.Graph or LEMON

If you have written graph code in C++ before, most of melon will be familiar under different names. This page is the translation table, followed by the four places where the model genuinely differs.

## Vocabulary

| | melon | Boost.Graph | LEMON |
| --- | --- | --- | --- |
| Immutable graph | `static_digraph` | `compressed_sparse_row_graph` | `StaticDigraph` |
| Mutable graph | `mutable_digraph` | `adjacency_list` | `ListDigraph` |
| Vertex type | `vertex_t<G>` | `graph_traits<G>::vertex_descriptor` | `Digraph::Node` |
| Arc type | `arc_t<G>` | `graph_traits<G>::edge_descriptor` | `Digraph::Arc` |
| Add a vertex | `create_vertex(g)` | `add_vertex(g)` | `g.addNode()` |
| Add an arc | `create_arc(g, u, v)` | `add_edge(u, v, g)` | `g.addArc(u, v)` |
| All vertices | `vertices(g)` | `vertices(g)` (iterator pair) | `Digraph::NodeIt` |
| All arcs | `arcs(g)` | `edges(g)` (iterator pair) | `Digraph::ArcIt` |
| Out-arcs of `v` | `out_arcs(g, v)` | `out_edges(v, g)` | `Digraph::OutArcIt` |
| Out-neighbors of `v` | `out_neighbors(g, v)` | `adjacent_vertices(v, g)` | — |
| Endpoints | `arc_source(g, a)` / `arc_target(g, a)` | `source(e, g)` / `target(e, g)` | `g.source(a)` / `g.target(a)` |
| Counts | `num_vertices(g)` / `num_arcs(g)` | `num_vertices(g)` / `num_edges(g)` | `countNodes(g)` / `countArcs(g)` |
| Degree | `out_degree(g, v)` | `out_degree(v, g)` | `countOutArcs(g, v)` |
| Vertex data | `create_vertex_map<T>(g)` | interior or exterior property map | `Digraph::NodeMap<T>` |
| Arc data | `create_arc_map<T>(g)` | interior or exterior property map | `Digraph::ArcMap<T>` |
| Reverse view | `views::reverse(g)` | `reverse_graph<G>` | `reverseDigraph(g)` |
| Filtered view | `views::subgraph(g, vf, af)` | `filtered_graph<G, EF, VF>` | `SubDigraph<G, VF, AF>` |
| Undirected view | `views::undirect(g)` | — | `Undirector<G>` |
| Dijkstra | `dijkstra(g, len, s)` | `dijkstra_shortest_paths(g, s, ...)` | `Dijkstra<G, LM>` |
| BFS | `breadth_first_search(g, s)` | `breadth_first_search(g, s, visitor(v))` | `Bfs<G>` |
| Max flow | `dinitz(g, cap, s, t)` | `boykov_kolmogorov_max_flow(...)` | `Preflow<G, CM>` |
| Min spanning tree | `kruskal(ug, cost)` | `kruskal_minimum_spanning_tree(...)` | `kruskal(g, cost, out)` |

Note the vocabulary choice: melon says **arc** for a directed edge and reserves **edge** for [undirected graphs](../graphs/undirected-graphs.md), following LEMON rather than Boost.Graph's `edge`-for-everything.

## The same program, three ways

=== "melon"

    ```cpp
    static_digraph_builder<static_digraph, double> builder(n);
    builder.add_arc(0, 1, 7.0).add_arc(1, 2, 9.0);
    auto [g, length] = builder.build();

    for(auto && [v, dist] : dijkstra(g, length, 0u))
        std::println("{} {}", v, dist);
    ```

=== "Boost.Graph"

    ```cpp
    using G = adjacency_list<vecS, vecS, directedS, no_property,
                             property<edge_weight_t, double>>;
    G g(n);
    add_edge(0, 1, 7.0, g);
    add_edge(1, 2, 9.0, g);

    std::vector<double> dist(num_vertices(g));
    dijkstra_shortest_paths(g, 0, distance_map(&dist[0]));
    for(auto v : make_iterator_range(vertices(g)))
        std::println("{} {}", v, dist[v]);
    ```

=== "LEMON"

    ```cpp
    ListDigraph g;
    std::vector<ListDigraph::Node> node(n);
    for(int i = 0; i < n; ++i) node[i] = g.addNode();
    ListDigraph::ArcMap<double> length(g);
    length[g.addArc(node[0], node[1])] = 7.0;
    length[g.addArc(node[1], node[2])] = 9.0;

    Dijkstra<ListDigraph, ListDigraph::ArcMap<double>> alg(g, length);
    alg.run(node[0]);
    for(ListDigraph::NodeIt v(g); v != INVALID; ++v)
        std::println("{} {}", g.id(v), alg.dist(v));
    ```

## Four things that differ

### 1. Ranges, not iterator pairs or `It` classes

Everything that iterates returns a `std::ranges` range, usable directly in a `for` loop and composable with `std::views`:

```cpp
// the five nearest out-neighbors, by arc length
for(auto && a : out_arcs(g, v) | std::views::filter([&](auto a) {
                    return length[a] < 10.0; }) | std::views::take(5)) { ... }
```

There is no `tie(it, end) = vertices(g)`, no `INVALID` sentinel to compare against, and no `graph_traits` specialization to write.

The same idiom extends to whole graphs: [graph views](../views/graphs.md) adapt lazily and pipe, `std::ranges`-style — `g | views::reverse`, `g | views::subgraph(keep)` — where Boost.Graph has `reverse_graph<G>` and `filtered_graph<G, P>` wrappers you spell out by type.

### 2. Algorithms are lazy generators, not visitor-driven calls

Boost.Graph expresses "do something at each step" with a visitor class and named parameters; LEMON with `init()` / `addSource()` / `processNextNode()`. melon inverts the control flow: the algorithm is an [input range](../algorithms/index.md), so the *loop body* is the visitor, and `break` is the early exit.

```cpp
// Boost: a visitor struct, an exception thrown to stop early
// melon: this
for(auto && [v, dist] : dijkstra(g, length, s)) {
    if(dist > radius) break;
    reachable.push_back(v);
}
```

LEMON's stepwise interface is the closest analogue, and melon keeps it: `finished()`, `current()`, `advance()`, plus `run()` when you want the whole thing. The whole lifecycle is the named `traversal_algorithm` contract: `reset()` restores the constructor's state, `run()` drains and returns the algorithm, and there is no post-construction step — LEMON's `init()` has no melon counterpart, and the one algorithm that had one, `competing_dijkstras::init()`, was removed in 1.0.

One more difference matters if you are used to copying algorithm objects around: melon's are move-only. A copy is a compile error — `reset()` or construct a second object instead — and `auto a = alg.run();` does not compile, because `run()` returns a reference. See [Ownership](../views/ownership.md#relocating-an-algorithm-move-only-always-sound).

### 3. Data maps are created by the graph, not handed to it

Boost.Graph's property maps are read and written through `get(pm, k)` and `put(pm, k, v)`, and come in interior and exterior flavours with `make_iterator_property_map` in between. LEMON's `NodeMap` is closer: it is constructed from the graph and uses `operator[]`.

melon follows LEMON, through a free function:

```cpp
auto dist = create_vertex_map<double>(g, 0.0);
dist[v] = 3.0;
```

The storage type is the graph's choice, and anything with a const-readable `operator[]` qualifies as a map — `std::vector`, `std::vector<bool>`, your own type, or a lambda wrapped in `maps::map`. (`std::map` is the exception: its inserting `operator[]` has no const form, so it fails `mapping` bare and works only wrapped through `maps::mapping_all` — of a const map.) See [Mappings](../graphs/mappings.md).

### 4. Concepts replace traits classes — and your type can be the graph

To make a type usable by Boost.Graph you specialize `graph_traits` and provide the free functions with the exact expected signatures. In melon you provide member functions or ADL free functions, and a concept checks them; there is no traits class to specialize, and [what you do not provide is often synthesized](../reference/customization-points.md) from what you do.

An algorithm's requirements are visible in its signature and enforced at the call site:

```cpp
template <graph_view Graph, mapping_view<arc_t<Graph>> LengthMap,
          dijkstra_traits Traits>
    requires outward_incidence_graph<Graph> && has_vertex_map<Graph>
class dijkstra;
```

Pass a graph without out-arcs and the error names `outward_incidence_graph`, not a missing member three instantiations deep.

## What melon does not have

Boost.Graph's catalogue is far larger. melon has no planarity testing, no graph isomorphism, no matching, no min-cost flow, no A\*, no Bellman–Ford, no graph I/O formats beyond a [Graphviz printer](../containers/graphs.md#printing-a-graph). LEMON's LP/MIP interfaces have no counterpart either — that is a [separate library](https://github.com/fhamonic/mippp) by the same author. If you need one of those today, melon is a complement rather than a replacement.
