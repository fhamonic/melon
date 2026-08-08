# Concepts index

Every concept in melon's public API, with its header and a one-line statement of what it requires. The narrative explanations are in [Graph concepts](../graphs/concepts.md), [Mappings](../graphs/mappings.md) and [Undirected graphs](../graphs/undirected-graphs.md).

## Graph — `melon/graph.hpp`

| Concept | Requires |
| --- | --- |
| `has_vertices<G>` | `melon::vertices(g)` |
| `has_num_vertices<G>` | `has_vertices` and `melon::num_vertices(g)` |
| `has_arcs<G>` | `melon::arcs(g)` |
| `has_num_arcs<G>` | `has_arcs` and `melon::num_arcs(g)` |
| `graph<G>` | `has_vertices`, `has_arcs`, `melon::arcs_entries(g)` |
| `has_arc_source<G>` | `graph` and `melon::arc_source(g, a)` |
| `has_arc_target<G>` | `graph` and `melon::arc_target(g, a)` |
| `has_out_arcs<G>` | `graph` and `melon::out_arcs(g, v)` yielding arcs |
| `has_in_arcs<G>` | `graph` and `melon::in_arcs(g, v)` yielding arcs |
| `has_out_degree<G>` | `graph` and `melon::out_degree(g, v)` |
| `has_in_degree<G>` | `graph` and `melon::in_degree(g, v)` |
| `outward_incidence_graph<G>` | `has_out_arcs` and `has_arc_target` |
| `inward_incidence_graph<G>` | `has_in_arcs` and `has_arc_source` |
| `outward_adjacency_graph<G>` | `graph` and `melon::out_neighbors(g, v)` |
| `inward_adjacency_graph<G>` | `graph` and `melon::in_neighbors(g, v)` |
| `has_arc_sources_map<G>` | `melon::arc_sources_map(g)` |
| `has_arc_targets_map<G>` | `melon::arc_targets_map(g)` |
| `has_vertex_map<G, T = std::size_t>` | `has_vertices` and both `create_vertex_map<T>` overloads |
| `has_arc_map<G, T = std::size_t>` | `has_arcs` and both `create_arc_map<T>` overloads |
| `has_vertex_creation<G>` | `melon::create_vertex(g)` returning `vertex_t<G>` |
| `has_vertex_removal<G>` | `melon::remove_vertex(g, v)` and `melon::is_valid_vertex(g, v)` |
| `has_is_valid_vertex<G>` | `graph` and `melon::is_valid_vertex(g, v)` — the validity question without the removal |
| `has_arc_creation<G>` | `melon::create_arc(g, u, v)` returning `arc_t<G>` |
| `has_arc_removal<G>` | `melon::remove_arc(g, a)` and `melon::is_valid_arc(g, a)` |
| `has_is_valid_arc<G>` | `graph` and `melon::is_valid_arc(g, a)` — likewise |
| `has_change_arc_source<G>` | `melon::change_arc_source(g, a, s)` |
| `has_change_arc_target<G>` | `melon::change_arc_target(g, a, t)` |

**Aliases.** `vertex_t<G>`, `arc_t<G>`, `vertices_range_t<G>`, `arcs_range_t<G>`, `out_arcs_range_t<G>`, `in_arcs_range_t<G>`, `out_arcs_iterator_t<G>`, `out_arcs_sentinel_t<G>`, `in_arcs_iterator_t<G>`, `in_arcs_sentinel_t<G>`, `out_neighbors_range_t<G>`, `in_neighbors_range_t<G>`, `vertex_map_t<G, T>`, `arc_map_t<G, T>`.

!!! note "The alias templates are the supported spelling"

    The member typedefs behind `vertex_t<G>` and `arc_t<G>` are private on
    every graph type; the alias templates work for every graph, view and user
    type, and are the only stable way to name a handle.

## Undirected graph — `melon/undirected_graph.hpp`

| Concept | Requires |
| --- | --- |
| `undirected_graph<G>` | `melon::vertices(g)`, `melon::edges(g)`, `melon::edge_endpoints(g, e)` |
| `has_num_edges<G>` | `undirected_graph` and `melon::num_edges(g)` |
| `has_incidence<G>` | `undirected_graph` and `melon::incidence(g, v)` yielding `(edge, vertex)` pairs |
| `has_degree<G>` | `undirected_graph` and `melon::degree(g, v)` |
| `has_edge_map<G, T = std::size_t>` | `undirected_graph` and both `create_edge_map<T>` overloads |

**Aliases.** `edge_t<G>`, `edges_range_t<G>`, `incidence_range_t<G>`, `incidence_iterator_t<G>`, `incidence_sentinel_t<G>`, `edge_map_t<G, T>`.

## Mapping — `melon/mapping.hpp`

| Concept | Requires |
| --- | --- |
| `mapping<M, K>` | `m[k]` and a non-`void` value through a **const** access |
| `output_mapping<M, K>` | `mapping` and `m[k] = v` |
| `contiguous_mapping<M, K>` | `mapping`, integral `K`, and `m.data()` |
| `mapping_of<M, K, V>` | `mapping` and `mapped_value_t<M, K>` is exactly `V` |
| `output_mapping_of<M, K, V>` | `output_mapping` and the value is exactly `V` |
| `contiguous_mapping_of<M, K, V>` | `contiguous_mapping` and the value is exactly `V` |
| `mapping_view<M, K>` | `mapping`, `std::movable`, and `enable_mapping_view<M>` — the variable template is `std::derived_from<M, mapping_view_base>`, mirroring `enable_graph_view` |
| `mapping_for<M, Map>` | `Map` is constructible from `maps::mapping_all_t<M>` — the constructor constraint that wraps an argument through `mapping_all` into the member |

**Aliases.** `mapped_reference_t<M, K>`, `mapped_const_reference_t<M, K>`, `mapped_value_t<M, K>`, `maps::mapping_all_t<M>`.

## Views — `melon/views/graph_view.hpp`, `melon/views/undirected_graph_view.hpp`, `melon/borrowed_graph.hpp`

| Concept / variable | Meaning |
| --- | --- |
| `enable_graph_view<T>` | `std::derived_from<T, graph_view_base>` |
| `graph_view<T>` | `graph`, `std::movable`, `enable_graph_view` |
| `enable_undirected_graph_view<T>` | `std::derived_from<T, undirected_graph_view_base>` |
| `undirected_graph_view<T>` | `undirected_graph`, `std::movable`, and the above |
| `enable_borrowed_graph<T>` | Opt-in, `false` by default: ranges obtained from `T` stay valid when the `T` *object* is relocated |
| `borrowed_graph<T>` | `enable_borrowed_graph<std::remove_cvref_t<T>>` |
| `graph_for<G, Graph>` | `Graph` is constructible from `views::graph_all_t<G>` — the constructor constraint that wraps an argument through `graph_all` into the member |
| `undirected_graph_for<UG, UGraph>` | the undirected counterpart, through `undirected_graph_all_t` |

**Aliases.** `views::graph_all_t<G>`, `views::undirected_graph_all_t<G>`.

`enable_borrowed_graph` and the `borrowed_graph` concept are the two names from
`melon/borrowed_graph.hpp`; the trait mirrors `std::ranges::enable_borrowed_range` and is what
decides whether an algorithm caching incidence ranges must rebase those cursors
when it is moved, or can let the compiler default the move. It is
`true` for `graph_ref_view`, `undirected_graph_ref_view` and
`views::complete_digraph`, propagates through `views::reverse` and
`views::undirect`, and is `false` for `views::subgraph` and `graph_owning_view`,
whose ranges point back at the view. Specialise it for a view of your own whose
ranges do not; see [Ownership](../views/ownership.md#relocating-an-algorithm-move-only-always-sound).

## Algorithms and utilities

| Concept | Header | Requires |
| --- | --- | --- |
| `algorithmic_generator<A>` | `utility/algorithmic_generator.hpp` | `finished()`, `current()`, `advance()` |
| `traversal_algorithm<A>` | `utility/algorithmic_generator.hpp` | the [lifecycle contract](../algorithms/index.md#the-lifecycle-contract): a movable generator range with chaining `reset()` and `run()` |
| `rooted_traversal_algorithm<A, S>` | `utility/algorithmic_generator.hpp` | `traversal_algorithm` plus chaining `add_source(s)` |
| `priority_queue<Q>` | `utility/priority_queue.hpp` | `std::semiregular`, `push`, `top`, `pop`, `size`, `empty`, `clear` |
| `updatable_priority_queue<Q>` | `utility/priority_queue.hpp` | `priority_queue` plus `contains`, `priority`, `promote`, `demote` |
| `semiring<S>` | `utility/semiring.hpp` | `value_type`, `plus_t`, `less_t`, `zero`, `infty`, `plus`, `less` |
| `breadth_first_search_traits<T>` | `algorithm/breadth_first_search.hpp` | the four BFS flags: `store_pred_vertices`, `store_pred_arcs`, `store_distances`, `store_traversal_range` |
| `depth_first_search_traits<T>` | `algorithm/depth_first_search.hpp` | `store_pred_vertices`, `store_pred_arcs`, `store_depth` |
| `topological_sort_traits<T>` | `algorithm/topological_sort.hpp` | `store_ranks`, `store_critical_paths` |
| `strongly_connected_components_traits<T>` | `algorithm/strongly_connected_components.hpp` | `store_component_ids` |
| `dijkstra_traits<T>` | `algorithm/dijkstra.hpp` | a `semiring`, an `updatable_priority_queue`, `store_distances`, `store_paths` |
| `bidirectional_dijkstra_traits<T>` | `algorithm/bidirectional_dijkstra.hpp` | a `semiring`, an `updatable_priority_queue`, `store_paths` |
| `network_voronoi_traits<T>` | `algorithm/network_voronoi.hpp` | a `semiring`, an `updatable_priority_queue`, and three flags: `store_distances`, `store_clusters`, `store_cluster_adjacency` |
| `biobjective_dijkstra_traits<T>` | `algorithm/biobjective_dijkstra.hpp` | the two-objective label and heap types |
| `competing_dijkstras_traits<T>` | `algorithm/competing_dijkstras.hpp` | a `semiring`, an `updatable_priority_queue`, a `(value, is_blue)`-shaped `entry`, and a strict-weak-order `entry_cmp` over it |
| `alias_method_sampler_traits<T>` | `utility/alias_method_sampler.hpp` | `heuristic_preprocessing` |
| `bentley_ottmann_traits<T>` | `algorithm/bentley_ottmann.hpp` | `Traits::report_endpoints` convertible to `bool` — the geometric kernel types are members of `bentley_ottmann_default_traits`, not concept requirements |
| `cartesian_point<T>` | `utility/geometry.hpp` | `std::get<0>` and `std::get<1>` of the point |
| `cartesian_segment<T>` | `utility/geometry.hpp` | two `cartesian_point`s |
| `cartesian_line<T>` | `utility/geometry.hpp` | the line coefficients |

**Aliases.** `traversal_entry_t<A>`.

## Using them

Constrain templates on the *least* you need — the diagnostic then names the missing capability at the call site:

```cpp
template <outward_incidence_graph G, mapping<arc_t<G>> LengthMap>
    requires has_vertex_map<G, double>
auto my_search(const G & g, const LengthMap & length);
```

And assert them next to your own types, where a regression shows up as one line rather than as a template avalanche:

```cpp
static_assert(outward_incidence_graph<my_graph>);
static_assert(has_vertex_map<my_graph, double>);
```
