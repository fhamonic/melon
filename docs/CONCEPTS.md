# Concepts and design choices

## Graphs

The concept `graph` is the root concept of the description of a graph structure within the library.
Thus, we wanted the most general but reasonable requirement possible for describing it.
Fundamentally, a graph is a way of representing relations between objects. In such case the objects are the *vertices* (or nodes) of the graph and the relations are the *edges* (or links) that connects group of vertices. We focus on the case where edges connect only pairs of vertices, i.e., we do not handle [hypergraphs](https://en.wikipedia.org/wiki/Hypergraph). Then, although it may seem counterintuitive, we will consider directed graphs to be more general than undirected graph. Indeed, in computer memory, data is fundamentally ordered and specifying that it must be considered unordered requires more work. A *directed-edge* is often called an *arc* and represented by an arrow from its *source* vertex to its *target* vertex. Finally, a graph may contain many arcs between the same source and target vertices in which case it is called a [multigraph](https://en.wikipedia.org/wiki/Multigraph). In such case arcs cannot be identified solely by their endpoints, and we must add to them an identifier such as a number. That comes handy since we often also need to associate data to the arcs and this identifier allows a more direct indexing than a pair of vertices.

All in all, the mathematical structure chosen to abstract all our graph concepts is the "directed multi-graph" and correspond to the 'graph' concept expressed below.

```cpp
template <typename G>
concept graph = std::copyable<G> &&
requires(G g) {
    { g.vertices() } -> std::ranges::input_range;
    { g.arcs() } -> std::ranges::input_range;
    { g.arc_entries() } -> 
        input_range_of<std::pair<arc_t<G>,std::pair<vertex_t<G>, vertex_t<G>>>>;
};
```

In our library, a graph structure of type `G` must then provide :
- `.vertices()` that returns a range that describes the graph vertices, whose type can be retrieved as `vertex_t<G>`
- `.arcs()` that returns a range that describes the graph arcs, whose type can be retrieved as `arc_t<G>`
- `.arc_entries()` that returns a range where each element is a pair `(i,(s,t))` where `i` is the arc identifier and `s` and `t` are the source and target vertices.


## Incidence list

An arc is said to be *incident* to a vertex `v` if `v` is either its source or its target.

## Adjacency list

Two vertices `u` and `v` are said to be *adjacent* if they are connected by an arc.