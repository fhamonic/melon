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

There are no requirements on the vertices and arcs types `vertex_t<G>` and `arc_t<G>` beyond identifying unambiguously vertices and arcs, i.e., no duplicates are present in the ranges `.vertices()` and `.arcs()`.


## Incidence list

An arc is said to be *incident* to a vertex `v` if `v` is either its source or its target. A classical lookup operation on graphs is to iterate over the outgoing arcs of a given vertex. The following concept express the requirement of this functionality.

```cpp
template <typename G>
concept outward_incidence_list = graph<G> &&
requires(G g, vertex_t<G> u) {
    { g.out_arcs(u) } -> input_range_of<arc_t<G>>;
};
```
Iterating over the outgoing arcs is not sufficient, for most algorithms we have to be able to retrieve the target vertex of arcs.  
```cpp
template <typename G>
concept has_arc_target = graph<G> && requires(G g, arc_t<G> a) {
    { g.target(a) } -> std::same_as<vertex_t<G>>;
    { g.targets_map() } -> input_map_of<arc_t<G>, vertex_t<G>>;
};
```
Here, the graph must provide of function `.target(a)` that returns the target vertex a given arc `a` and a function `.targets_map()` that return an entity that maps any arc to its target vertex. This entity can either be a constant reference to a plain container or a wrapper around the `.target` function and his usefulness will be explained later.
<!-- TODO : link to map_view and prefetching -->
Since they are very often required together, for example, in traversal algorithm, the `outward_incidence_list` and `has_arc_target` concepts are regrouped under the `forward_incidence_list` one.
At the opposite, the `inward_incidence_list` and `has_arc_source` concepts are regrouped under the `reverse_incidence_graph` one.
```cpp
template <typename G>
concept forward_incidence_graph = outward_incidence_list<G> && has_arc_target<G>;
```
We found that the independence of these concepts can be justified by the fact that some graph implementations can satisfy `has_arc_source` without satisfying `inward_incidence_list` or satisfy `inward_incidence_list` without `has_arc_source`, and that preserving the symmetry between out arcs and in arcs is useful.
 
## Adjacency list

Two vertices `u` and `v` are said to be *adjacent* if they are connected by an arc.

```cpp
template <typename G>
concept outward_adjacency_list = outward_incidence_list<G> && has_arc_target<G> || requires(G g, vertex_t<G> u) {
    { g.out_neighbors(u) } -> input_range_of<vertex_t<G>>;
};
```


We can define the arcs to be the iterators of the inner list of vertices. Indeed, this iterator is uniquely defining an out_ward arc

The concepts `forward_incidence_graph` and `adjacency_list` may seem redundant, but it is not the case.
For example, a graph that is a `forward_incidence_list` may also be a `inward_incidence_list` but not a `inward_adjacency_list` because it is not capable of retrieving the source vertex of a given `arc`.

## Attaching data to vertices and arcs

It is up to each graph implementation to describe the way data can be attached to its vertices and arcs. Indeed, no assumptions are made on the underlying types of vertices and arcs that can range from integers (in most cases) to plain old structs. The concepts `has_vertex_map<G,T>` and `has_arc_map<G,T>` express the prerequisite for a graph of type `G` to allows users to create maps for attaching data of type `T` to vertices and arcs as follows.
```cpp
G g = ...;
vertex_map_t<G,T> vertex_map = g.template create_vertex_map<T>();
arc_map_t<G,T> arc_map = g.template create_arc_map<T>();
```
When creating such maps, the data attached is considered [default initialized](https://en.cppreference.com/w/cpp/language/default_initialization), but a default value can be passed in argument.

This design choice has many advantages:
- the graph implemention to be the only one needing to know the actual types of its vertices and arcs
- graph algorithms requiring to attach data to vertices and arcs can express this requirement with concepts
- the actual types of `vertex_map_t<G,T>` and `vertex_arc_t<G,T>` is chosen by the graph implementation and a fully customizable point, allowing, for example, to provide custom allocator
- for some implementations, the graph and its maps can hold mutual references in order to resize adequately upon the creation of new vertices and arcs or to give back the ownership of the allocated memory to the parent graph, when the destructor is called, in order to recycle it.
