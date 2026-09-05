#Changelog

Notable changes to melon. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[semantic versioning](https://semver.org) starting at 1.0.0.

## [Unreleased]

## [1.0.0] - 2026-09-05

First stable release. Every header outside `melon/detail/` and
`melon/experimental/` is frozen API for the 1.x series — see
[API stability](https://fhamonic.github.io/melon/getting-started/installation/#api-stability)
for the exact scope of the guarantee. All changes below are relative to
`1.0.0-alpha.1`, the last package published on Conan Center.

The `v1.0.0` tag was first cut on 2026-08-28 and re-pointed on 2026-09-05,
before any package was published from it, to a revision that also carries the
following renames. Code written against the earlier snapshot needs:
`add_arc({u, v}, …)` for `add_arc(u, v, …)` on a builder with properties;
`maps::identity`, `maps::element<I...>` and `maps::function` for
`maps::identity_map`, `maps::element_map<I...>` and `maps::map`;
`melon/maps/constant.hpp` and `melon/maps/element.hpp` for `maps::true_map` /
`maps::false_map` and `maps::element`; `melon/numeric/` for `semiring.hpp` and
`geometry.hpp`; a mapping rather than a callable as
`alias_method_sampler`'s probability map; and `graph_ref_view`,
`graph_owning_view`, `graph_view_base`, `views::graph_all`,
`views::graph_all_t` and `graph_for` for their `undirected_graph_*` twins,
which are gone along with `melon/views/undirected_graph_view.hpp`; and
`melon/graph.hpp` for `melon/undirected_graph.hpp` and
`melon/borrowed_graph.hpp`, whose contents now sit in the former.

### Breaking changes since 1.0.0-alpha.1

- The library namespace is `melon`, no longer `fhamonic::melon`, and the
  CMake target is `melon::melon`.
- The range-v3 dependency is gone: melon is dependency-free and builds on
  C++23 standard library ranges. The toolchain floor is GCC 14 / Clang 18
  with libstdc++ 14 (GCC 15 / C++26 recommended) and, on Windows, Visual
  Studio 2022 17.11 (toolset v14.41).
- Algorithms are move-only steppable ranges with a single lifecycle
  contract: copying an algorithm object no longer compiles, and
  preconditions are asserted in debug builds, never thrown.
- Mappings are read through const access — `mapping` requires
  const-readability, so `std::map` is not one — and algorithms store the
  graphs and maps they are given as views (`views::graph_all` /
  `maps::mapping_all`): lvalues by reference, rvalues by move. Both
  reject const rvalues (as `std::views::all` does) instead of silently
  deep-copying into an owning view over `const T` that models nothing.
- The graph protocol is a customization-point layer that synthesizes
  missing operations (`out_neighbors`, `arcs`, degrees, counts) from the
  primitives a graph type exposes, and it is stricter than the alpha
  concepts in two ways: `arcs_entries` elements must be tuple-likes of the
  shape `(arc, (source, target))`, and every range- or closure-returning
  CPO — the undirected `edges` and `incidence` included — rejects rvalue
  graphs at compile time; the result would dangle behind the temporary.
  Rvalue support remains available through the borrowed-graph promise
  (`enable_borrowed_graph` in `melon/graph.hpp`). Custom graph types
  written against the alpha concepts may need their protocol members
  reshaped.
- The undirected protocol follows suit: `nb_edges` is `num_edges`
  (`has_nb_edges` is `has_num_edges`); `incidence(g, v)` yields
  `(edge, other endpoint)` tuple-likes instead of bare edges, so a caller
  never re-derives the far endpoint from `edge_endpoints`; and a self-loop
  is incident at both ends — `incidence` lists it twice, each time with `v`
  as the other endpoint, and `degree` counts it twice. The last is part of
  the concepts, not of any one container: a member `degree` must agree with
  the incidence range it summarises. A custom undirected graph listing a
  loop once, or yielding edges alone, needs its `incidence` reshaped.
- `static_digraph`, `static_forward_digraph` and `mutable_digraph` are
  aliases of the default instantiation of `basic_static_digraph<V, A>`,
  `basic_static_forward_digraph<V, A>` and `basic_mutable_digraph<V, A>`:
  the three containers are now class templates over their vertex and arc
  handle types (`std::unsigned_integral`, both defaulting to
  `unsigned int`), `std::basic_string` / `std::string` style. A 64-bit arc
  type lifts the 2^32 ceiling; a 16-bit type halves every array and map of
  a small graph. The containers `assert` on a vertex or arc count the
  handle type cannot represent instead of wrapping. Every existing
  spelling of the names keeps compiling; a user-side forward declaration
  (`class static_digraph;`) does not, and never was covered — the API
  stability page now says so.
- `static_digraph_builder::add_arc` takes the endpoints as one
  `std::pair<vertex, vertex>` — `add_arc({u, v}, length)` instead of
  `add_arc(u, v, length)` — so a call shows where the topology stops and
  the properties begin; three positional integers did not. A builder
  without properties keeps the plain `add_arc(u, v)`. New `add_arcs`
  appends a range of endpoint pairs, or of `(pair, properties...)`
  tuple-likes such as `std::views::zip(endpoints, lengths)`, reserving up
  front for sized ranges.
- `melon/utility/semiring.hpp` moved to `melon/numeric/semiring.hpp`
  (pure-math substrate, not graph tooling; the declared names are
  unchanged), and `maps::true_map` / `maps::false_map` moved out of
  `melon/mapping.hpp` into `melon/maps/constant.hpp` as the `<true>` /
  `<false>` aliases of the new `maps::constant<V>`.
- `concurrent_dijkstras` is renamed `competing_dijkstras`, and its
  relaxation steps are private, like every algorithm's.
- `views::complete_digraph` requires unsigned vertex and arc types: signed
  handles broke its wraparound-empty incidence subranges.
- `shortest_path_semiring<T>::infty` (and `minimum_spanning_tree_semiring<T>`'s)
  is `std::numeric_limits<T>::infinity()` when `T` is IEEE floating point,
  `max()` as before otherwise: unlike `max()`, `inf` is exact under further
  arithmetic, so a "no path" value can no longer be improved by code
  combining it with lengths. Observable wherever an unreached vertex's
  `dist()` was compared against `max()`.
- `mutable_digraph`'s vertex and arc iterators are no longer
  cross-comparable: the handle types share one id representation, so the
  shared base's equality compared cursors from unrelated lists — and
  answered true on equal ids. Each iterator now defines its own equality.
- `priority_queue` probes `top()` and `empty()` on a `const Q`, and
  `semiring` probes `plus`/`less` on const values — matching how every
  algorithm actually calls them, so a heap with non-const reads or a
  mutable-lvalue-only `plus_t` is now rejected by the concept instead of
  hard-erroring inside the algorithm that admitted it.

### Other changes since 1.0.0-alpha.1

- New algorithms: `a_star` (heuristic consistency asserted at every
  examined arc in debug builds; deliberately no defaulted zero heuristic —
  that instance is dijkstra), `bellman_ford` and `bellman_ford_moore`
  (negative lengths, with opt-in negative-cycle detection and retrieval
  through the `detect_negative_cycles` traits flag),
  `biobjective_dijkstra`, `connected_components` (with
  `weakly_connected_components(g)`, which undirects a digraph and runs it),
  `network_voronoi`, `traversal_forest`, `network_simplex` (below), and
  `bentley_ottmann` promoted out of `melon/experimental/`.
- `network_simplex`: exact minimum-cost flow by the primal network simplex,
  in the implementation lineage of LEMON's — but with no renumbering, no
  problem copy, and no materialized artificial root: it runs in the
  graph's own id spaces, keeps every piece of state in the graph's own
  maps (the root is as implicit as its virtual arcs, marked by a vertex
  being its own parent in the basis tree), and reads capacities, costs and
  supplies live through the given mappings. Steppable one pivot at a time,
  reports its verdict through the `optimal()` / `infeasible()` /
  `unbounded()` predicates, and exposes the dual
  potentials alongside the flow (`flows_map()` / `potentials_map()`, with
  terminal move-out). Requires `num_vertices` / `num_arcs` and the map
  factories; neither vertex nor arc ids need be integral — any copyable,
  equality-comparable id type the factories and mappings accept runs, so a
  `mutable_digraph` with id holes from removals qualifies (the
  entering-arc search walks the graph's own `arcs()` range through a
  resumable cursor), and so does a graph whose handles are structs. Arc
  endpoints are delegated to `arc_source` / `arc_target` where the graph
  answers them, so an arc-list graph with map factories qualifies too. The
  entering-arc pivot rule is selectable through the traits
  (`pivot_rules::block_search` — the default — `first_eligible` and
  `best_eligible`, each carrying its own tuning constants; custom rules
  plug in through a small search-context interface), along with an
  `arc_mixing` flag providing LEMON's mixed sampling order as a strided
  scan over random-access arc ranges (off by default; see the traits
  documentation for when it pays).
- New utilities: `numeric/bounded_value`, `numeric/rational` and
  `numeric/geometry`, `alias_method_sampler` (its probability map is a
  mapping: a vector indexed by the item or a graph's vertex/arc map passes
  directly, callables need no wrapping, and the sampled value type deduces
  decayed), `make_static_digraph`, and `version.hpp` — the
  `MELON_VERSION` macros both build systems parse.
- New maps under `melon/maps/`: `maps::constant<V>` (an empty map
  answering the NTTP `V` for every key), `maps::element<I...>` (a
  `std::get` chain into the key), `maps::transform(m, f)` (a mapping
  viewed through a value projection `f(m[k])`, the base routed through
  `maps::mapping_all` — lvalue referenced, rvalue owned, view passed
  through; a reference-returning projection keeps the base's writability);
  `maps::identity` and `maps::function` stay in `melon/mapping.hpp`.
- Map roles: every map an algorithm creates is requested under a role
  (`dijkstra_roles::heap_index`, `dinitz_roles::flow`, … — one
  `<algorithm>_roles` struct per algorithm), carried as a defaulted second
  template parameter of `create_vertex_map` / `create_arc_map` /
  `create_edge_map`, of `vertex_map_t` / `arc_map_t` / `edge_map_t` and of
  `has_vertex_map` / `has_arc_map` / `has_edge_map`; `melon::default_role`
  is what a request naming no role carries. A factory with one template
  parameter answers every role with its standard map, so no container or
  user graph changes; a two-parameter `create_vertex_map<T, Role>` answers
  per role and is probed first. Every forwarding view (`graph_ref_view`,
  `graph_owning_view`, `reverse`, `subgraph`, `undirect`, the `with_*_maps`
  views) forwards the role it receives. Roles are extension
  points with a weaker stability guarantee than the main API.
- One wrapper for both protocols: `graph_ref_view`, `graph_owning_view` and
  `views::graph_all` accept directed and undirected graphs, and graphs
  modelling both protocols, and forward
  every protocol half the wrapped type models, as do the `with_*_maps`
  views and `views::reverse`; `graph_for` is the one constructor
  constraint. A type modelling both `graph` and `undirected_graph` — an
  undirected container reading each edge as two arcs — therefore stays
  both through the wrappers, and `dijkstra` and `kruskal` read the same
  object. `views::subgraph` keeps the directed half only, having no edge
  filter. The forwarding itself is public: `graph_view_interface<G, Stored>`
  (and the one-half `directed_graph_view_interface` /
  `undirected_graph_view_interface`) is the base a user adaptor derives from
  to forward everything and redeclare what it changes, `view_interface`
  style. `has_vertex_creation`, `has_is_valid_vertex` and
  `has_vertex_removal` are gated on `has_vertices`, no longer on `graph`.
- `views::as_directed` / `views::as_undirected`
  (`melon/views/graph_view.hpp`): restrict such a graph to one half — the
  identity on a graph with nothing to hide — for the caller who must pick,
  before an overload set split on `graph_view` / `undirected_graph_view`.
- `views::with_vertex_maps`, `views::with_arc_maps` and
  `views::with_edge_maps` (`melon/views/with_maps.hpp`): factory-enhancing
  views that answer the map factories from caller-supplied lambdas and
  forward everything else. A graph without factories runs every algorithm
  from a single lambda (`[]<typename T>(auto role, const auto & g) {
    … }`);
  a graph with them can have particular maps redirected into storage the
  caller already owns, Boost.Graph interior-property style. A lambda may
  declare the bare form, the filled form (taking the default value) or
  both, the missing one being derived; the first lambda serving a request
  owns it, so role-specific lambdas go before the generic one; a request no
  lambda serves — a generic lambda without an explicit `<typename T>`, a
  `mutable` lambda — falls through to the wrapped graph's factory.
  Pipe-composable; directed and undirected.
- `updatable_d_ary_heap::index_map_type` and the `heap_index_map_agrees`
  concept: each updatable-heap algorithm `static_assert`s that a traits
  heap publishing the alias is built on the very map the graph answers for
  the algorithm's heap-index role. The default traits of `dijkstra`,
  `a_star`, `bidirectional_dijkstra`, `competing_dijkstras` and
  `network_voronoi` spell their heap's index map through the role-aware
  alias; a custom traits struct hard-coding
  `vertex_map_t<static_digraph, std::size_t>` still compiles over a
  container, and fails the `static_assert` — rather than silently indexing
  a private copy — over a view answering the role with another map type.
- Bound adaptor closures (`g | views::subgraph(f)` and the `with_*_maps`
  ones) are constrained on the adaptor producing a view rather than on
  `melon::graph`, so undirected graphs pipe into them too.
- Experimental, with no stability guarantee:
  `experimental::views::add_virtual_vertices`
  (`melon/experimental/add_virtual_vertices.hpp`) augments a graph with
  `count` fresh vertices by extending the integral vertex id space past
  its largest id — dense ids not required, arcs untouched, empty
  incidence, the vertex-map factory covering them, stacked views minting
  past each other's virtual ids; `experimental::views::unify_sources`
  (`melon/experimental/unify_sources.hpp`) augments a dense-id graph with
  a virtual root vertex and one virtual arc per given source — the
  supersource construction of multi-source flow problems — by extending
  the integer id spaces, its map factories covering the virtual elements
  and the inward interface following the wrapped graph.
- MSVC support: Visual Studio 2022 17.11 (toolset v14.41) and newer build
  the library and pass the full test suite, exercised by an MSVC 17.11 /
  C++23 CI job on Windows. One documented MSVC front-end limitation
  remains for graphs that provide their map factories as free functions in
  translation units with a file-scope `using namespace melon` — see the
  installation page's MSVC note. `detail/prefetch.hpp` emits real prefetch
  hints under MSVC on x86-64.
- Semirings may promise arithmetic absorption through an optional
  `infty_is_absorbing` member, read via the `has_absorbing_infty` variable
  template; `bellman_ford` drops its per-arc unreached-source guard when
  the promise holds, making the floating-point arc sweep branchless.
- Concept honesty — probes answer `false` where they used to hard-error,
  and admit what the algorithms can actually use:
    - `graph<G>` answers `false` for a graph whose vertex or arc handles
      are move-only (the `arcs_entries` synthesizers constrain on
      `copy_constructible` handles).
    - The map-creation requirement (`has_vertex_map` / `has_arc_map`)
      lives on the algorithms' class-level requires-clauses and default
      traits, so `std::constructible_from` and CTAD probes on a graph
      without map factories answer `false`.
    - The containers' and `complete_digraph`'s map factories are
      constrained on what construction does with the value type
      (default-init, plus fill-assign for the default-value form), and the
      views' delegating factories on the wrapped graph's factory for the
      exact value type requested.
    - Factory-created maps need only model `output_mapping`: the
      algorithms fill and reset through an internal helper that uses a
      member `fill(value)` when the map offers one and writes per key
      otherwise, so a conforming map without `.fill` runs every algorithm.
    - `output_mapping` no longer constrains the type of the write
      expression and writes the probe's value from an rvalue with a
      forwarded key: const-assignable proxies in the C++23 `vector<bool>`
      style (`static_filter_map` is an output mapping, so a bit-packed
      subgraph filter keeps `disable_vertex` / `enable_vertex`), move-only
      value types and rvalue-only-key subscripts are admitted; computed
      maps stay rejected.
    - The `cartesian_point`, `cartesian_segment` and `cartesian_line`
      concepts are arity-exact (`std::tuple_size` of 2, 2 and 3) over
      `cartesian_coordinate` leaves — an `==`- and `<`-comparable scalar
      that is not itself tuple-like — so the three categories are pairwise
      disjoint and `point_on_line(lineA, lineB)` is a constraint failure
      rather than a meaningless predicate. The extent checks
      (`point_on_segment`, `segments_intersection` and the overlaps) take a
      `common_cartesian_segment`, whose endpoints share one point type, in
      the spirit of `std::ranges::common_range`; `segment_to_line` keeps
      accepting mixed endpoints.
- Correctness fixes from the audits preceding this release, notably:
  `complete_digraph` arc-id arithmetic past 2³² arcs, `dinitz` recursion
  overflow on million-vertex augmenting paths and equal-terminal looping,
  `biobjective_dijkstra` dominated-label yields, `knapsack_bnb` bound
  overflow, `rational` comparison categories, `disjoint_sets::merge` stale
  representatives, `static_filter_map`'s const-assignable bit proxy,
  `alias_method_sampler` boundary draws, `bidirectional_dijkstra` binding
  incidence ranges through `const auto &` (which fails for filter- and
  transform-shaped ranges that are not const-iterable), and
  `mutable_digraph` default-constructed iterators comparing unequal to the
  end sentinel (a value-initialized incidence or vertex range walked a
  null structure instead of being empty).
- Consistency sweeps: noexcept clauses measure what accessors actually
  return, the concept mirrors match the CPO detection they mirror
  (`priority_queue` asks `movable` + `default_initializable` instead of
  `semiregular`), and the synthesized `arcs()`/`arcs_entries()` join the
  same incidence direction on an equal-rank tie.

[1.0.0]: https://github.com/fhamonic/melon/compare/v1.0.0-alpha.1...v1.0.0
[Unreleased]: https://github.com/fhamonic/melon/compare/v1.0.0...HEAD
