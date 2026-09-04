# Header map

Every public header, and what it declares. Include what you use; `melon/all.hpp` pulls in everything and is meant for scratch programs.

Everything lives in `namespace melon`, with four sub-namespaces:

| Namespace | Holds |
| --- | --- |
| `melon::views` | **graph** views — `reverse`, `subgraph`, `induced_subgraph`, `undirect`, `complete_digraph`, `graph_all`, `undirected_graph_all` |
| `melon::maps` | **mapping** views — `function`, `mapping_all`, `constant` (aliases `true_map`, `false_map`), `identity`, `element`, `transform` |
| `melon::numeric` | the arithmetic value types — `rational`, `integer`, `make_rational`, `bounded_value`, `const_value` |
| `melon::experimental` | work in progress, no stability guarantee |

The split follows one rule: `views::` and `maps::` hold what you *spell at
call sites* — adaptors, factories, and factory-less leaf maps whose type name
is the interface — while a class *returned* by a factory lives in `melon`
itself, met through `auto` rather than by name. So the concepts and the
customization points stay in `melon`, as do the view classes
(`subgraph_view`, `reverse_view`, …), the ownership views `graph_ref_view` /
`graph_owning_view`, their mapping twins `mapping_ref_view` /
`mapping_owning_view`, and `transform_map_view`.

## Core

| Header | Declares |
| --- | --- |
| `melon/borrowed_graph.hpp` | `enable_borrowed_graph`, the trait to specialise for a view of your own whose ranges survive the view being relocated, and the `borrowed_graph` concept — see [Ownership](../views/ownership.md#relocating-an-algorithm-move-only-always-sound) |
| `melon/graph.hpp` | the [graph concepts](../graphs/concepts.md) and every directed [customization point](customization-points.md); `vertex_t`, `arc_t`, `vertex_map_t`, `arc_map_t` |
| `melon/mapping.hpp` | the [mapping concepts](../graphs/mappings.md) — `mapping`, `mapping_of`, `mapping_view` and friends, plus `mapping_for` — `mapping_ref_view` / `mapping_owning_view` / `maps::mapping_all`, `maps::function`, `maps::identity` |
| `melon/undirected_graph.hpp` | the [undirected concepts](../graphs/undirected-graphs.md) and CPOs; `edge_t`, `edge_map_t` |
| `melon/version.hpp` | `MELON_VERSION_MAJOR` / `MINOR` / `PATCH`, `MELON_VERSION` |
| `melon/all.hpp` | everything below |

## Containers — `melon/container/`

| Header | Declares |
| --- | --- |
| `static_digraph.hpp` | [`basic_static_digraph<V, A>`, `static_digraph`](../containers/graphs.md#static_digraph) |
| `static_forward_digraph.hpp` | [`basic_static_forward_digraph<V, A>`, `static_forward_digraph`](../containers/graphs.md#static_forward_digraph) |
| `mutable_digraph.hpp` | [`basic_mutable_digraph<V, A>`, `mutable_digraph`](../containers/graphs.md#mutable_digraph) |
| `static_map.hpp` | [`static_map<K, V>`](../containers/data-structures.md#static_map) |
| `static_filter_map.hpp` | [`static_filter_map<K>`](../containers/data-structures.md#static_filter_map) |
| `d_ary_heap.hpp` | [`d_ary_heap`, `updatable_d_ary_heap`](../containers/data-structures.md#heaps) |
| `disjoint_sets.hpp` | [`disjoint_sets`](../containers/data-structures.md#disjoint_sets) |

## Views — `melon/views/`

| Header | Declares |
| --- | --- |
| `graph_view.hpp` | `graph_view_base`, the `graph_view` concept and `enable_graph_view`, `graph_ref_view`, `graph_owning_view`, `graph_for`, `views::graph_all`, [`views::graph_adaptor_closure`](../views/graphs.md#pipe-syntax) |
| `undirected_graph_view.hpp` | the undirected counterparts, `undirected_graph_for` included |
| `reverse.hpp` | `reverse_view`, the [`views::reverse`](../views/graphs.md#reverse) adaptor |
| `subgraph.hpp` | `subgraph_view`, `induced_subgraph_view`, the [`views::subgraph`, `views::induced_subgraph`](../views/graphs.md#subgraph) adaptors |
| `undirect.hpp` | `undirect_view`, the [`views::undirect`](../views/graphs.md#undirect) adaptor |
| `complete_digraph.hpp` | [`views::complete_digraph`](../views/graphs.md#complete_digraph) |
| `with_maps.hpp` | `with_vertex_maps_view`, `with_arc_maps_view`, `with_edge_maps_view`, the [`views::with_vertex_maps`, `views::with_arc_maps`, `views::with_edge_maps`](../views/graphs.md#with_vertex_maps-with_arc_maps-with_edge_maps) adaptors |

## Maps — `melon/maps/`

The ready-made [mapping views](../graphs/mappings.md#the-ready-made-maps) beyond what `melon/mapping.hpp` itself carries.

| Header | Declares |
| --- | --- |
| `constant.hpp` | `maps::constant<V>` and its `maps::true_map` / `maps::false_map` aliases |
| `element.hpp` | `maps::element<I...>` |
| `transform.hpp` | `transform_map_view`, the `maps::transform` factory |

## Algorithms — `melon/algorithm/`

| Header | Declares |
| --- | --- |
| `breadth_first_search.hpp` | [`breadth_first_search`](../algorithms/traversals.md#breadth_first_search) |
| `depth_first_search.hpp` | [`depth_first_search`](../algorithms/traversals.md#depth_first_search) |
| `topological_sort.hpp` | [`topological_sort`](../algorithms/traversals.md#topological_sort) |
| `strongly_connected_components.hpp` | [`strongly_connected_components`](../algorithms/traversals.md#strongly_connected_components) |
| `connected_components.hpp` | [`connected_components`, `weakly_connected_components`](../algorithms/traversals.md#connected-components) |
| `traversal_forest.hpp` | [`traversal_forest`](../algorithms/traversals.md#traversal_forest) |
| `dijkstra.hpp` | [`dijkstra`, `dijkstra_default_traits`, `dijkstra_traits`](../algorithms/shortest-paths.md#dijkstra) |
| `a_star.hpp` | [`a_star`, `a_star_default_traits`, `a_star_traits`](../algorithms/shortest-paths.md#a_star) |
| `bidirectional_dijkstra.hpp` | [`bidirectional_dijkstra`](../algorithms/shortest-paths.md#bidirectional_dijkstra) |
| `biobjective_dijkstra.hpp` | [`biobjective_dijkstra`](../algorithms/shortest-paths.md#biobjective_dijkstra) |
| `competing_dijkstras.hpp` | [`competing_dijkstras`](../algorithms/shortest-paths.md#competing_dijkstras) |
| `network_voronoi.hpp` | [`network_voronoi`](../algorithms/shortest-paths.md#network_voronoi) |
| `bellman_ford.hpp` | [`bellman_ford`, `bellman_ford_default_traits`, `bellman_ford_traits`](../algorithms/shortest-paths.md#bellman_ford) |
| `bellman_ford_moore.hpp` | [`bellman_ford_moore`, `bellman_ford_moore_default_traits`, `bellman_ford_moore_traits`](../algorithms/shortest-paths.md#bellman_ford_moore) |
| `edmonds_karp.hpp` | [`edmonds_karp`](../algorithms/flows-and-trees.md#edmonds_karp) |
| `dinitz.hpp` | [`dinitz`](../algorithms/flows-and-trees.md#dinitz) |
| `network_simplex.hpp` | [`network_simplex`, `network_simplex_default_traits`, `network_simplex_traits`, `mcf_status`](../algorithms/flows-and-trees.md#network_simplex) |
| `kruskal.hpp` | [`kruskal`](../algorithms/flows-and-trees.md#kruskal) |
| `knapsack_bnb.hpp` | [`knapsack_bnb`](../algorithms/others.md#knapsack) |
| `unbounded_knapsack_bnb.hpp` | [`unbounded_knapsack_bnb`](../algorithms/others.md#knapsack) |
| `bentley_ottmann.hpp` | [`bentley_ottmann`](../algorithms/others.md#bentley_ottmann) |

## Utilities — `melon/utility/`

| Header | Declares |
| --- | --- |
| `static_digraph_builder.hpp` | [`static_digraph_builder`](../containers/graphs.md#the-builder) |
| `make_static_digraph.hpp` | [`make_static_digraph`](../containers/graphs.md#rebuilding-as-a-static_digraph) |
| `algorithmic_generator.hpp` | [`algorithmic_generator`](../algorithms/index.md), `traversal_algorithm`, `rooted_traversal_algorithm`, `algorithm_iterator`, `algorithm_view_interface`, `traversal_entry_t` |
| `priority_queue.hpp` | `priority_queue`, `updatable_priority_queue` |
| `graphviz_printer.hpp` | [`graphviz_printer`](../containers/graphs.md#printing-a-graph) |
| `erdos_renyi.hpp` | [`erdos_renyi<G>(n, p)`](../containers/graphs.md#generating-a-graph) |
| `alias_method_sampler.hpp` | [`alias_method_sampler`](../algorithms/others.md#sampling) |

## Numerics — `melon/numeric/`

The value types live in `namespace melon::numeric`; the directory matches the namespace, the way `melon/views/` matches `melon::views`. The semiring and geometry vocabulary is the exception: those headers hold the pure-math substrate of the algorithms, but their names stay in `melon` itself.

| Header | Declares |
| --- | --- |
| `rational.hpp` | `numeric::rational<NumT, DenT>`, `numeric::integer<T>`, `numeric::make_rational` |
| `bounded_value.hpp` | `numeric::bounded_value`, `numeric::const_value` and the widening-conversion helpers |
| `semiring.hpp` | [`semiring`](../algorithms/shortest-paths.md#semirings) and the four provided ones |
| `geometry.hpp` | `cartesian_coordinate`, `cartesian_point`, `cartesian_segment`, `common_cartesian_segment`, `cartesian_line`, `cartesian` |

## Not public API

**`melon/detail/`** — implementation details. No stability guarantee, and nothing here should appear in your code: `concat_view.hpp` (the `std::ranges::concat_view` fallback for standard libraries that lack it), `consumable_view.hpp`, `fill.hpp` (the fill-or-write-per-key helper behind every algorithm reset), `intrusive_iterator_base.hpp`, `map_if.hpp` (the `[[no_unique_address]]` conditional maps), `movable_box.hpp` (the `std::ranges`-style box that keeps a view owning a capturing lambda assignable), `not_self.hpp` (the guard that stops a single-argument constructor template from swallowing an object of its own type instead of letting the copy or move constructor be chosen), `prefetch.hpp`, `specialization_of.hpp`, `stdlib_check.hpp` (the libstdc++ version diagnostic).

The same applies to anything under `melon/detail/` or in a `detail` namespace — including `melon::cpo`, where the CPO implementation types are defined. The customization point *objects* themselves (`vertices`, `out_arcs`, …) live in an inline namespace inside `melon`, are spelled `melon::vertices` and so on, and are stable API.

**`melon/experimental/`** — work in progress in `namespace melon::experimental`, with no stability guarantee:

| Header | Status |
| --- | --- |
| `planar_map.hpp` | compiles, covered by `test/experimental.cpp` |
| `add_virtual_vertices.hpp` | augments a graph with fresh vertex ids; covered by `test/add_virtual_vertices.cpp` |
| `unify_sources.hpp` | the supersource construction (virtual root + per-source arcs); covered by `test/unify_sources.cpp` |
| `dual.hpp` | compiles, covered by `test/experimental.cpp` |
| `scapegoat_tree.hpp` | **unfinished, does not compile — not shipped** |
| `doubly_connected_digraph.hpp` | **unfinished, does not compile — not shipped** |

The last two remain in the repository but are excluded from both the CMake install rules and the Conan package.

## Include-what-you-use

The dependency edges worth knowing:

- `melon/graph.hpp` includes `melon/mapping.hpp` and, at the end, `melon/views/graph_view.hpp` — so having a graph gives you the mapping concepts and `views::graph_all`.
- the algorithm headers include `melon/graph.hpp` or `melon/undirected_graph.hpp` as needed, so `#include "melon/algorithm/dijkstra.hpp"` alone gives you `vertices`, `create_vertex_map`, `maps::function` and the concepts. The pure-mapping ones — both knapsacks and `bentley_ottmann` — include only `melon/mapping.hpp`.
- **container headers do not include `melon/graph.hpp`** — they only need `melon/mapping.hpp`. Including `container/mutable_digraph.hpp` on its own gives you the class but not `create_vertex`, `vertices` or `num_vertices`. Add `melon/graph.hpp` when a container is all you include.
- no algorithm or view header includes a *graph container* — only `utility/erdos_renyi.hpp`, `utility/make_static_digraph.hpp` and `melon/all.hpp` do — so you must include `melon/container/static_digraph.hpp` yourself to have a graph to run on. (The heap-based algorithms — `dijkstra`, `a_star`, the other Dijkstra variants, `network_voronoi` and `bentley_ottmann` — do pull in `container/d_ary_heap.hpp` for their default traits, and `kruskal` pulls in `container/disjoint_sets.hpp`.)
