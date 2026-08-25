# Changelog

Notable changes to melon. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[semantic versioning](https://semver.org) starting at 1.0.0.

## [Unreleased]

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

[Unreleased]: https://github.com/fhamonic/melon/compare/v1.0.0-alpha.1...HEAD
