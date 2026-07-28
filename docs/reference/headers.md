# Header map

Every public header, and what it declares. Include what you use; `melon/all.hpp` pulls in everything and is meant for scratch programs.

Everything lives in `namespace melon`, with views in `melon::views` and work in progress in `melon::experimental`.

## Core

| Header | Declares |
| --- | --- |
| `melon/graph.hpp` | the [graph concepts](../graphs/concepts.md) and every directed [customization point](customization-points.md); `vertex_t`, `arc_t`, `vertex_map_t`, `arc_map_t` |
| `melon/mapping.hpp` | the [mapping concepts](../graphs/mappings.md), `mapping_ref_view` / `mapping_owning_view` / `views::mapping_all`, `views::map`, `views::true_map`, `views::false_map`, `views::identity_map`, `views::element_map` |
| `melon/undirected_graph.hpp` | the [undirected concepts](../graphs/undirected-graphs.md) and CPOs; `edge_t`, `edge_map_t` |
| `melon/version.hpp` | `MELON_VERSION_MAJOR` / `MINOR` / `PATCH`, `MELON_VERSION` |
| `melon/all.hpp` | everything below |

## Containers — `melon/container/`

| Header | Declares |
| --- | --- |
| `static_digraph.hpp` | [`static_digraph`](../containers/graphs.md#static_digraph) |
| `static_forward_digraph.hpp` | [`static_forward_digraph`](../containers/graphs.md#static_forward_digraph) |
| `mutable_digraph.hpp` | [`mutable_digraph`](../containers/graphs.md#mutable_digraph) |
| `static_map.hpp` | [`static_map<K, V>`](../containers/data-structures.md#static_map) |
| `static_filter_map.hpp` | [`static_filter_map<K>`](../containers/data-structures.md#static_filter_map) |
| `d_ary_heap.hpp` | [`d_ary_heap`, `updatable_d_ary_heap`](../containers/data-structures.md#heaps) |
| `disjoint_sets.hpp` | [`disjoint_sets`](../containers/data-structures.md#disjoint_sets) |

## Views — `melon/views/`

| Header | Declares |
| --- | --- |
| `graph_view.hpp` | `graph_view_base`, `graph_ref_view`, `graph_owning_view`, `views::graph_all` |
| `undirected_graph_view.hpp` | the undirected counterparts |
| `reverse.hpp` | [`views::reverse`](../views/graphs.md#reverse) |
| `subgraph.hpp` | [`views::subgraph`, `views::induced_subgraph`](../views/graphs.md#subgraph) |
| `undirect.hpp` | [`views::undirect`](../views/graphs.md#undirect) |
| `complete_digraph.hpp` | [`views::complete_digraph`](../views/graphs.md#complete_digraph) |

## Algorithms — `melon/algorithm/`

| Header | Declares |
| --- | --- |
| `breadth_first_search.hpp` | [`breadth_first_search`](../algorithms/traversals.md#breadth_first_search) |
| `depth_first_search.hpp` | [`depth_first_search`](../algorithms/traversals.md#depth_first_search) |
| `topological_sort.hpp` | [`topological_sort`](../algorithms/traversals.md#topological_sort) |
| `strongly_connected_components.hpp` | [`strongly_connected_components`](../algorithms/traversals.md#strongly_connected_components) |
| `connected_components.hpp` | [`connected_components`, `weakly_connected_components`](../algorithms/traversals.md#connected-components) |
| `traversal_forest.hpp` | [`traversal_forest`](../algorithms/traversals.md#traversal_forest) |
| `dijkstra.hpp` | [`dijkstra`, `dijkstra_default_traits`, `dijkstra_trait`](../algorithms/shortest-paths.md#dijkstra) |
| `bidirectional_dijkstra.hpp` | [`bidirectional_dijkstra`](../algorithms/shortest-paths.md#bidirectional_dijkstra) |
| `biobjective_dijkstra.hpp` | [`biobjective_dijkstra`](../algorithms/shortest-paths.md#biobjective_dijkstra) |
| `competing_dijkstras.hpp` | [`competing_dijkstras`](../algorithms/shortest-paths.md#competing_dijkstras) |
| `network_voronoi.hpp` | [`network_voronoi`](../algorithms/shortest-paths.md#network_voronoi) |
| `edmonds_karp.hpp` | [`edmonds_karp`](../algorithms/flows-and-trees.md#edmonds_karp) |
| `dinitz.hpp` | [`dinitz`](../algorithms/flows-and-trees.md#dinitz) |
| `kruskal.hpp` | [`kruskal`](../algorithms/flows-and-trees.md#kruskal) |
| `knapsack_bnb.hpp` | [`knapsack_bnb`](../algorithms/others.md#knapsack) |
| `unbounded_knapsack_bnb.hpp` | [`unbounded_knapsack_bnb`](../algorithms/others.md#knapsack) |
| `bentley_ottmann.hpp` | [`bentley_ottmann`](../algorithms/others.md#bentley_ottmann) |

## Utilities — `melon/utility/`

| Header | Declares |
| --- | --- |
| `static_digraph_builder.hpp` | [`static_digraph_builder`](../containers/graphs.md#the-builder) |
| `algorithmic_generator.hpp` | [`algorithmic_generator`](../algorithms/index.md), `algorithm_iterator`, `algorithm_view_interface`, `traversal_entry_t` |
| `priority_queue.hpp` | `priority_queue`, `updatable_priority_queue` |
| `semiring.hpp` | [`semiring`](../algorithms/shortest-paths.md#semirings) and the four provided ones |
| `graphviz_printer.hpp` | [`graphviz_printer`](../containers/graphs.md#printing-a-graph) |
| `erdos_renyi.hpp` | [`erdos_renyi<G>(n, p)`](../containers/graphs.md#generating-a-graph) |
| `alias_method_sampler.hpp` | [`alias_method_sampler`](../algorithms/others.md#sampling) |
| `rational.hpp` | `rational<NumT, DenT>`, `integer<T>`, `make_rational` |
| `geometry.hpp` | `cartesian_point`, `cartesian_segment`, `cartesian_line`, `cartesian` |
| `bounded_value.hpp` | `bounded_value` and the widening-conversion helpers |

## Not public API

**`melon/detail/`** — implementation details. No stability guarantee, and nothing here should appear in your code: `concat_view.hpp` (the `std::ranges::concat_view` fallback for standard libraries that lack it), `consumable_view.hpp`, `intrusive_view.hpp`, `intrusive_iterator_base.hpp`, `map_if.hpp` (the `[[no_unique_address]]` conditional maps), `prefetch.hpp`, `specialization_of.hpp`.

The same applies to anything in a `detail` or `__detail` namespace, or prefixed with `__` — including `melon::__cust_access`, where the CPO function objects are defined.

**`melon/experimental/`** — work in progress in `namespace melon::experimental`, with no stability guarantee:

| Header | Status |
| --- | --- |
| `planar_map.hpp` | compiles, covered by `test/experimental.cpp` |
| `dual.hpp` | compiles, covered by `test/experimental.cpp` |
| `scapegoat_tree.hpp` | **unfinished, does not compile when instantiated — not shipped** |
| `doubly_connected_digraph.hpp` | **unfinished, does not compile when instantiated — not shipped** |

The last two remain in the repository but are excluded from both the CMake install rules and the Conan package.

## Include-what-you-use

The dependency edges worth knowing:

- `melon/graph.hpp` includes `melon/mapping.hpp` and, at the end, `melon/views/graph_view.hpp` — so having a graph gives you the mapping concepts and `views::graph_all`.
- every algorithm header includes `melon/graph.hpp`, so `#include "melon/algorithm/dijkstra.hpp"` alone gives you `vertices`, `create_vertex_map`, `views::map` and the concepts.
- **container headers do not include `melon/graph.hpp`** — they only need `melon/mapping.hpp`. Including `container/mutable_digraph.hpp` on its own gives you the class but not `create_vertex`, `vertices` or `num_vertices`. Add `melon/graph.hpp` when a container is all you include.
- no algorithm or view header includes a *graph container* — only `utility/erdos_renyi.hpp` and `melon/all.hpp` do — so you must include `melon/container/static_digraph.hpp` yourself to have a graph to run on. (`algorithm/dijkstra.hpp` does pull in `container/d_ary_heap.hpp`, which its default traits need.)
