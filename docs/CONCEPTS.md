# Concepts and design choices

## Graph

The concept `graph` is the root requirement for all the graph structures supported by the library.
Fundamentally, a graph is a way of representing relations between objects. 
These objects are called the *vertices* (or nodes) of the graph and their relations are called *edges* (or links) that connects group of vertices. Usually edges connect only pairs of vertices and, in order to keep it simple, the library sticks to this basic case, i.e., it does not handle [hypergraphs](https://en.wikipedia.org/wiki/Hypergraph). Although it may seem counterintuitive, we consider directed graphs to be more general than undirected ones. Indeed, in computer memory, data is fundamentally ordered and specifying that it must be considered unordered, like the end vertices of an undirected edge, requires more work. A *directed-edge* is often called an *arc* and represented by an arrow from its *source* vertex to its *target* vertex. A graph may contain many arcs between the same source and target vertices in which case it is called a [multigraph](graph,https://en.wikipedia.org/wiki/Multigraph). In a multigraph, arcs cannot be identified solely by their endpoints, and must be joined with an identifier, like a number. That comes handy since we often need to associate data to the arcs and this identifier can allow a more direct indexing than a pair of vertices.

All in all, the mathematical structure chosen to abstract all our graph concepts is the "directed multigraph" and is expressed by the following concept.
```cpp
template <typename _Tp>
concept graph = requires(const _Tp & __t) {
                    melon::vertices(__t);
                    melon::arcs(__t);
                    melon::arcs_entries(__t);
                };
```
Then, in our library, an instance `g` of a graph structure of type `_Tp` :
- `melon::vertices(g)` that returns a range of the graph vertices, which are of type `vertex_t<_Tp>` 
- `melon::arcs(g)` that returns a range of the graph arcs identifiers, which are of type `arc_t<G>`
- `melon::arcs_entries(__t)` that returns a range where each element is a pair `(a,(s,t))` where `a` is the arc identifier and `s` and `t` are the source and target vertices.

There are no requirements on the vertices and arcs types `vertex_t<G>` and `arc_t<G>` beyond identifying unambiguously vertices and arcs, i.e., no duplicates are present in the ranges `melon::vertices(g)` and `melon::arcs(g)`. The simplest implementation, and probably the most efficient one, is to use integers for these identifiers.

## Incidence graph

An arc is said to be *incident* to a vertex `v` if `v` is either its source or its target. A classical lookup operation on graphs is to iterate over the outgoing arcs of a given vertex. The following concept express the requirement of this functionality.
```cpp
template <typename _Tp>
concept has_out_arcs =
    graph<_Tp> &&
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        melon::out_arcs(__t, __v);
    } &&
    std::convertible_to<std::ranges::range_value_t<out_arcs_range_t<_Tp>>,
                        arc_t<_Tp>>;
```
Iterating over the outgoing arcs is not sufficient, for most algorithms we have to be able to retrieve the target vertex of an arc `a` with ``.  
```cpp
template <typename _Tp>
concept has_arc_target =
    graph<_Tp> && requires(const _Tp & __t, const arc_t<_Tp> & __a) {
                      melon::arc_target(__t, __a);
                  };
```
Here, the graph `g` must provide `melon::arc_target(g, a)` that returns the target vertex of the arc `a`.
Since they are very often required together, for example, in traversal algorithms, the `has_out_arcs` and `has_arc_target` concepts are regrouped under the `outward_incidence_graph` one.
At the opposite, the `has_in_arcs` and `has_arc_source` concepts are regrouped under the `inward_incidence_graph` one.
```cpp
template <typename _Tp>
concept outward_incidence_graph =
    graph<_Tp> && has_out_arcs<_Tp> && has_arc_target<_Tp>;
```
While the concepts `has_out_arcs` and `has_arc_target` are closely related, their independence is justified by the fact that some graph and algorithms implementations may require only one of them. For exemple, a graph satifying `outward_incidence_graph` may also satisfy `has_out_arcs` but not `has_in_arcs`.
 
## Adjacency graph

Two vertices `u` and `v` are said to be *adjacent* if they are connected by an arc. Another common lookup operation on graphs is iterating on the adjacent neighbors of a vertex.
```cpp
template <typename _Tp>
concept outward_adjacency_graph =
    graph<_Tp> && requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
                      melon::out_neighbors(__t, __v);
                  };
```
In this case, the graph `g` must provide `melon::out_neighbors(g, v)` that returns a range of the outgoing neigbors of the vertex `v`.
The concepts `outward_incidence_graph` and `outward_adjacency_list` may seem redundant, but it is not the case.
For example, a graph that is a `outward_incidence_graph` is necessarily an `outward_adjacency_graph` since we can transform the range of outgoing arcs by taking the target vertex of each arc.
However, a graph may provide a way to list the outgoing neighbors without satifying the concepts `has_out_arcs` and `has_arc_target`.

## Attaching data to vertices and arcs

It is up to each graph implementation to describe the way data can be attached to its vertices and arcs. Indeed, no assumptions are made on the underlying types of vertices and arcs that can range from integers (in most cases) to plain old structs. The concepts `has_vertex_map<G,T>` and `has_arc_map<G,T>` express the prerequisite for a graph of type `G` to allows users to create maps for attaching data of type `T` to vertices and arcs as follows.
```cpp
G g = ...;
vertex_map_t<G,T> vertex_map = create_vertex_map<T>(g);
arc_map_t<G,T> arc_map = create_arc_map<T>(g);
```
When creating such maps, the data attached is considered [default initialized](https://en.cppreference.com/w/cpp/language/default_initialization), i.e. in the case of primitive types, the value is indeterminate.
We can pass an aditional argument to initialize the values of the map.
```
vertex_map_t<G,int> vertex_map = create_vertex_map<T>(g, 0);
```

This design choice has many advantages:
- the graph implementation offers an interface that is totally agnostic of the types of its vertices and arcs
- graph algorithms requiring to attach data to vertices and arcs can express this requirement with concepts
- the actual types of `vertex_map_t<G,T>` and `vertex_arc_t<G,T>` are chosen by the graph implementation and fully customizable points, allowing, for example, to provide custom allocator
- for some implementations, the graph and its maps can hold mutual references in order to resize adequately upon the creation of new vertices and arcs or to give back the ownership of the allocated memory to the parent graph, when the destructor is called, in order to recycle it.

