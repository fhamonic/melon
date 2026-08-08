# Why melon

Graph code in C++ has long forced a choice between two unpleasant options. [Boost.Graph](https://www.boost.org/doc/libs/release/libs/graph/) is generic, but its genericity predates concepts: it is expressed with traits classes and tag dispatch, its external property maps are verbose, and a mistake yields template errors nobody wants to read. [LEMON](https://lemon.cs.elte.hu/trac/lemon) is fast and comfortable, but it is unmaintained and does not compile with C++20 and above. Most people end up writing `std::vector<std::vector<int>>` and a hand-rolled Dijkstra for the third time this year.

melon is an attempt to get the genericity of the first and the speed and ergonomics of the second, using the tools C++20 and C++23 finally provide: concepts, ranges, and customization point objects. Its architecture follows from a handful of decisions, described below.

## The interface is a set of concepts, not a base class

melon has no `Graph` class that algorithms take. It has a hierarchy of [concepts](../graphs/concepts.md), rooted at `graph`, that describe what a graph type must *do*:

```cpp
template <typename T>
concept graph = has_vertices<T> && has_arcs<T> &&
                requires(const T & t) { melon::arcs_entries(t); };
```

and refined by capability concepts — `outward_incidence_graph`, `inward_adjacency_graph`, `has_arc_source`, `has_vertex_map<G, T>`, `has_arc_creation`, and so on. Every algorithm states exactly the capabilities it needs in its template signature:

```cpp
template <graph_view Graph, mapping_view<arc_t<Graph>> LengthMap,
          dijkstra_traits Traits>
    requires outward_incidence_graph<Graph> && has_vertex_map<Graph>
class dijkstra;
```

The template-parameter constraints (`graph_view`, `mapping_view`) say what the stored members *are* — stored members are always views, a rule [Ownership and mapping views](../views/ownership.md) spells out — while the capabilities the algorithm needs sit in the requires-clause.

Two things follow. First, instantiating an algorithm with a structure that cannot support it fails at *compile time*, at the call site, with a diagnostic naming the missing requirement — not with a wall of errors from three headers down. Second, and more importantly, nothing about that signature mentions a melon type. Any structure of yours that satisfies `outward_incidence_graph` runs melon's Dijkstra, with no adapter, no wrapper, and no copy of your data into a "real" graph first. [Bringing your own graph](../graphs/custom-graphs.md) shows how few functions that takes.

## Customization points that fill in the gaps

The free functions the concepts are written against — `melon::vertices`, `melon::out_arcs`, `melon::arc_target`, … — are not plain functions but [customization point objects](../reference/customization-points.md). Each one accepts a member function (`g.out_arcs(v)`) or a free function found by ADL (`out_arcs(g, v)`), so you can adapt a type you do not own.

More usefully, several of them *synthesize* what you did not provide. Give melon `vertices`, `out_arcs` and `arc_target`, and you automatically get:

- `melon::out_neighbors(g, v)` — the out-arcs, transformed by their target;
- `melon::arcs(g)` — the out-arcs of every vertex, joined;
- `melon::arcs_entries(g)` — the same, paired with endpoints;
- `melon::out_degree(g, v)` and `melon::num_vertices(g)` when the corresponding ranges are sized.

So a structure only has to expose what it stores. The fallbacks are not blind: when a graph offers several routes to the same range, the CPO picks the one whose range category is strongest — `arcs_entries` will list `arcs` directly rather than joining incidences if the `arcs` range is at least as good, and joins out-arcs rather than in-arcs when the out-arcs range is the better of the two. A graph that stores an explicit arc list keeps its random-access iteration; one that stores adjacency lists gets a correct, if forward-only, arc range for free.

## The graph owns its data maps

Attaching data to vertices and arcs is not done with external property maps handed in from the outside. The graph creates them:

```cpp
G g = ...;
vertex_map_t<G, double> potential = create_vertex_map<double>(g);
arc_map_t<G, int> flow = create_arc_map<int>(g, 0);  // 0-initialized
```

This inverts the usual dependency, and buys a lot:

- the graph interface stays agnostic of what vertices and arcs actually *are* — integers, pointers, or plain structs;
- an algorithm requiring scratch space states it as a constraint, `has_vertex_map<G, int>`, instead of asking the caller for storage;
- the storage type is the graph implementation's choice and a full customization point — `static_digraph` hands back a flat `static_map` indexed in O(1), a graph with non-integral vertices can hand back a hash map, and either can use a custom allocator;
- a mutable graph can hold a back-reference to its maps and resize them when vertices are created, or reclaim their memory on destruction.

The [mapping concepts](../graphs/mappings.md) (`mapping`, `output_mapping`, `contiguous_mapping` and their `..._of` refinements) describe those maps in the same style: `std::vector`, `std::vector<bool>`, melon's own containers and your `operator[]`-providing type all qualify (`std::map` too, once wrapped by `maps::mapping_all` — which the algorithms apply themselves), and an algorithm asks for exactly the capability it uses. `contiguous_mapping` in particular is what lets the shortest-path algorithms issue explicit prefetches.

## Algorithms are ranges you can step

An algorithm in melon is not a function that runs to completion and returns a result. It is an object that satisfies `algorithmic_generator` — `finished()`, `current()`, `advance()` — and therefore [behaves as an input range](../algorithms/index.md), through `algorithm_view_interface`. The full lifecycle is the named `traversal_algorithm` / `rooted_traversal_algorithm` contract: `reset()` restores the constructor's state, `run()` drains and returns the algorithm, and `add_source` requires the vertex to be untouched — see [the lifecycle contract](../algorithms/index.md#the-lifecycle-contract).

```cpp
// consume it as a range, in the order the algorithm settles vertices
for(auto && [v, dist] : dijkstra(graph, length_map, s)) { ... }

// or drive it by hand, and stop when you have what you need
dijkstra alg(graph, length_map, s);
while(!alg.finished()) {
    auto [v, dist] = alg.current();
    if(v == target) break;
    alg.advance();
}
```

The held object is move-only: a copy is a compile error, and starting over is `reset()` or a second construction. Early exit costs nothing and needs no visitor, no exception thrown through the algorithm, and no callback. Two searches can be advanced in lockstep — which is precisely how `bidirectional_dijkstra` and `competing_dijkstras` are built. And because the object is a range, the rest of the standard library applies: `std::views::filter`, `std::views::take`, `std::ranges::find_if`.

Where a result genuinely needs the full run, the algorithm still exposes `run()` and dedicated accessors — `dinitz::minimum_cut()`, `dijkstra::path_to(t)`, `biobjective_dijkstra::pareto_front(v)`.

## You do not pay for what you do not use

Each algorithm takes a *traits* type that selects its data structures and what it records:

```cpp
struct my_traits {
    using semiring = shortest_path_semiring<double>;
    using heap = updatable_d_ary_heap<4, ...>;   // 4-ary instead of binary
    static constexpr bool store_distances = true;
    static constexpr bool store_paths = true;
};

dijkstra alg(my_traits{}, graph, length_map, s);
double d = alg.dist(t);
for(auto && a : alg.path_to(t)) { ... }
```

The predecessor and distance maps are declared `[[no_unique_address]]` and collapse to nothing when the corresponding flag is `false`, so the default Dijkstra allocates one status map and one heap — and `dist()` and `path_to()` are then *removed from the overload set* by a `requires` clause rather than failing at runtime. Swapping the [semiring](../algorithms/shortest-paths.md#semirings) turns the same Dijkstra into a most-reliable-path or a maximum-capacity-path search without touching the traversal code.

The same principle drives the [views](../views/graphs.md): `views::reverse(g)`, `views::subgraph(g, vertex_filter, arc_filter)` and `views::undirect(g)` are lazy adaptors holding a reference, not rebuilt graphs, and they are themselves graphs — so they compose (`g | views::subgraph(keep) | views::reverse`, as in `std::ranges`), and any algorithm accepts them.

## Is melon right for you?

**melon is a good fit if** you write graph or network-optimization code in modern C++ and want algorithms that work directly on *your* data structure; if you need to run the same algorithm over a graph, its reverse, and a filtered subgraph without duplicating memory; or if you are looking for a maintained replacement for LEMON that compiles under C++23.

**Know the limits.** melon is a young, single-maintainer library. Its API is frozen for the 1.x series as of 1.0.0, but the [roadmap](https://github.com/fhamonic/melon#roadmap) — network simplex, Laplacian solvers, planar map intersection, JSON serialization, tree and bipartite graph concepts — is a list of things that do not exist yet, not of things being polished. Everything under `melon/experimental/` carries no stability guarantee at all. There is no MSVC support; on Windows the supported toolchain is MinGW-w64. And the price of concept-based genericity is C++23 fluency: ranges, concepts, CTAD, and the diagnostics that come with them.

**Stay with the incumbents if** you need the breadth of Boost.Graph's algorithm catalogue (planarity testing, matching, isomorphism, min-cost flow — melon has none of these yet), if you are pinned to an older standard, or if you build with MSVC.

## Next steps

Head to [Installation](installation.md), then walk through [A first graph](first-graph.md).

If you already know Boost.Graph or LEMON, [Coming from Boost.Graph or LEMON](coming-from.md) maps your vocabulary onto this one in a single table.
