# Changelog

Notable changes to melon. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[semantic versioning](https://semver.org) starting at 1.0.0.

## [Unreleased]

### Added

- `network_simplex`: exact minimum-cost flow by the primal network simplex,
  in the implementation lineage of LEMON's — but with no renumbering, no
  problem copy, and no materialized artificial root: it runs in the
  graph's own id spaces, keeps every piece of state in the graph's own
  maps (the root is as implicit as its virtual arcs, marked by a vertex
  being its own parent in the basis tree), and reads capacities, costs and
  supplies live through the given mappings. Steppable one pivot at a time,
  reports `optimal` / `infeasible` / `unbounded`, and exposes the dual
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
- `experimental::views::add_virtual_vertices`
  (`melon/experimental/add_virtual_vertices.hpp`): augments a graph with
  `count` fresh vertices by extending the integral vertex id space past
  its largest id — dense ids not required, arcs untouched. Virtual
  vertices have empty incidence, the vertex-map factory covers them, and
  stacked views mint past each other's virtual ids. Experimental: no
  stability guarantee.
- `experimental::views::unify_sources`
  (`melon/experimental/unify_sources.hpp`): augments a dense-id graph with
  a virtual root vertex and one virtual arc per given source — the
  supersource construction of multi-source flow problems — by extending
  the integer id spaces; its map factories cover the virtual elements, and
  the inward interface follows the wrapped graph. Experimental: no
  stability guarantee.

- MSVC support: Visual Studio 2022 17.11 (toolset v14.41) and newer build
  the library and pass the full test suite, exercised by a new
  MSVC 17.11 / C++23 CI job on Windows. One documented MSVC front-end
  limitation remains for graphs that provide their map factories as free
  functions in translation units with a file-scope `using namespace melon`
  — see the installation page's MSVC note.
- `detail/prefetch.hpp` emits real prefetch hints under MSVC on x86-64;
  they were previously GCC/Clang-only.
- `maps::constant<V>` (`melon/maps/constant.hpp`): an empty map
  answering the NTTP `V` for every key; `maps::true_map` and
  `maps::false_map` are now its `<true>` / `<false>` aliases.
- `maps::transform(m, f)` (`melon/maps/transform.hpp`): views a
  mapping through a value projection — `f(m[k])` — with the base routed
  through `maps::mapping_all` (lvalue referenced, rvalue owned, view
  passed through). A reference-returning projection keeps the base's
  writability.

### Changed

- `maps::map` is renamed `maps::function`; it stays in
  `melon/mapping.hpp`.
- `maps::true_map`, `maps::false_map` and `maps::element` moved out of
  `melon/mapping.hpp` into their own headers under `melon/maps/`
  (`constant.hpp`, `element.hpp`); `maps::identity` stays in
  `melon/mapping.hpp`.
- `melon/utility/semiring.hpp` and `melon/utility/geometry.hpp` moved to
  `melon/numeric/`: they are pure-math substrate, not graph tooling. The
  declared names are unchanged.
- The `cartesian_point`, `cartesian_segment` and `cartesian_line` concepts
  are now arity-exact (`std::tuple_size` of 2, 2 and 3) over
  `cartesian_coordinate` leaves — a new public concept: an `==`- and
  `<`-comparable scalar that is not itself tuple-like. This makes the three
  categories pairwise disjoint, so passing a segment or line where a point
  belongs is a constraint failure instead of compiling into a meaningless
  predicate (`point_on_line(lineA, lineB)` used to compile). The extent
  checks (`point_on_segment`, `segments_intersection` and the overlaps) now
  take a `common_cartesian_segment` — a `cartesian_segment` whose endpoints share one
  point type, in the spirit of `std::ranges::common_range` — turning the
  `std::minmax` hard error they produced on mixed-endpoint segments into a
  constraint failure; `segment_to_line` keeps accepting them. Types that
  model the documented contract — tuples of numbers of the right shape —
  are unaffected.

### Fixed

- `mutable_digraph`: default-constructed iterators now compare equal to
  the end sentinel, so a value-initialized incidence or vertex range is
  empty instead of walking a null structure — which graph views rely on to
  stand in an empty range (fixes `experimental::views::unify_sources` root
  incidence over a hole-free `mutable_digraph`).

## [1.0.0] - 2026-08-28

First stable release. Every header outside `melon/detail/` and
`melon/experimental/` is frozen API for the 1.x series — see
[API stability](https://fhamonic.github.io/melon/getting-started/installation/#api-stability)
for the exact scope of the guarantee. All changes below are relative to
`1.0.0-alpha.1`, the last package published on Conan Center.

### Breaking changes since 1.0.0-alpha.1

- The library namespace is `melon`, no longer `fhamonic::melon`, and the
  CMake target is `melon::melon`.
- The range-v3 dependency is gone: melon is dependency-free and builds on
  C++23 standard library ranges. The toolchain floor is GCC 14 / Clang 18
  with libstdc++ 14 (GCC 15 / C++26 recommended); MSVC remains unsupported.
- Algorithms are move-only steppable ranges with a single lifecycle
  contract: copying an algorithm object no longer compiles, and
  preconditions are asserted in debug builds, never thrown.
- Mappings are read through const access — `mapping` requires
  const-readability, so `std::map` is not one — and algorithms store the
  graphs and maps they are given as views (`views::graph_all` /
  `maps::mapping_all`): lvalues by reference, rvalues by move.
- The graph protocol is a customization-point layer that synthesizes
  missing operations (`out_neighbors`, `arcs`, degrees, counts) from the
  primitives a graph type exposes, and it is stricter than the alpha
  concepts in two ways: `arcs_entries` elements must be tuple-likes of the
  shape `(arc, (source, target))`, and every range- or closure-returning
  CPO rejects rvalue graphs at compile time — the result would dangle
  behind the temporary. Rvalue support remains available through the
  borrowed-graph promise (`melon/borrowed_graph.hpp`); `views::graph_all`
  rejects const rvalues instead of silently deep-copying. Custom graph
  types written against the alpha concepts may need their protocol members
  reshaped.
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

### Other changes since 1.0.0-alpha.1

- New algorithms: `a_star` (heuristic consistency asserted at every
  examined arc in debug builds; deliberately no defaulted zero heuristic —
  that instance is dijkstra), `bellman_ford` and `bellman_ford_moore`
  (negative lengths, with opt-in negative-cycle detection and retrieval
  through the `detect_negative_cycles` traits flag),
  `biobjective_dijkstra`, `connected_components`, `network_voronoi`,
  `traversal_forest`, and `bentley_ottmann` promoted out of
  `melon/experimental/`.
- New utilities: `numeric/bounded_value` and `numeric/rational`,
  `alias_method_sampler`, `geometry`, `make_static_digraph`, and
  `version.hpp` — the `MELON_VERSION` macros both build systems parse.
- Semirings may promise arithmetic absorption through an optional
  `infty_is_absorbing` member, read via the `has_absorbing_infty` variable
  template; `bellman_ford` drops its per-arc unreached-source guard when
  the promise holds, making the floating-point arc sweep branchless.
- Correctness fixes from the audits preceding this release, notably:
  `complete_digraph` arc-id arithmetic past 2³² arcs, `dinitz` recursion
  overflow on million-vertex augmenting paths and equal-terminal looping,
  `biobjective_dijkstra` dominated-label yields, `knapsack_bnb` bound
  overflow, `rational` comparison categories, `disjoint_sets::merge` stale
  representatives, `static_filter_map`'s const-assignable bit proxy, and
  `alias_method_sampler` boundary draws.
- Consistency sweeps: noexcept clauses measure what accessors actually
  return, the concept mirrors match the CPO detection they mirror
  (`priority_queue` asks `movable` + `default_initializable` instead of
  `semiregular`), and the synthesized `arcs()`/`arcs_entries()` join the
  same incidence direction on an equal-rank tie.

[1.0.0]: https://github.com/fhamonic/melon/compare/v1.0.0-alpha.1...v1.0.0
[Unreleased]: https://github.com/fhamonic/melon/compare/v1.0.0...HEAD
