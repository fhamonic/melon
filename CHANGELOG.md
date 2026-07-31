# Changelog

All notable changes to this project are documented in this file.
The project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html);
see the [API stability](README.md#api-stability) section of the README for the
exact scope of the guarantees.

## 1.0.0 — 2026-07-31

First stable release.

### Breaking changes

- **Algorithms are move-only.** Every algorithm's copy constructor and copy
  assignment are deleted, so `std::copyable<A>` is now `false` for every
  algorithm `A` over every graph, and `std::movable<A>` is `true` for every
  one of them. Three things motivated it. Nothing needed copying: no example,
  benchmark or documented use case copied an algorithm, and the only internal
  copy was `traversal_forest` copying its own `breadth_first_search` inside
  `traversal_forest`'s own copy constructor. Copying was never cheap — an
  algorithm carries every vertex map, the heap and the cached cursors — so
  passing one by value is now a compile error rather than a silent O(V+E)
  duplication. And the availability rule could not be stated: over one
  identical graph type (a subgraph of a ref to `static_digraph`), `dijkstra`,
  `breadth_first_search` and `topological_sort` were copyable while
  `depth_first_search` and `strongly_connected_components` were not, and
  `depth_first_search` flipped its answer depending on whether the container
  underneath handed out std-borrowed incidence ranges. Migration: construct a
  second algorithm, or `reset()` one to reuse the state it already allocated;
  `run()` returns `*this` by reference, so `auto a = alg.run();` becomes
  `alg.run();` with the result read through the accessors. Moving is
  unaffected and remains sound mid-traversal — the cursor rebasing in
  `detail/consumable_view.hpp` exists for the move, not the copy. This also
  retires a latent defect in `knapsack_bnb` / `unbounded_knapsack_bnb`, whose
  hand-written copies lacked the `requires`-clause that keeps `std::copyable`
  honest: for a move-only `ItemRange` the trait answered `true` and the copy
  hard-errored inside the mem-initializer.
- **`input_mapping` and `input_mapping_of` are collapsed into `mapping` and
  `mapping_of`.** The old two-layer split was redundant: the loose syntactic
  `mapping` supported no operation, and `input_mapping`'s real content —
  readability through a const access — was hidden inside `mapped_value_t`.
  `mapping` now states it directly, so `mapping<std::map<K, V>, K>` is false
  (its inserting `operator[]` cannot be const; wrapped maps still work through
  `maps::mapping_all`'s `at()` fallback), and `output_mapping` /
  `contiguous_mapping` / `mapping_view` refine `mapping`. Rename
  `input_mapping` → `mapping` and `input_mapping_of` → `mapping_of`; algorithm
  class heads spell their map parameter `mapping_view<Key> Map` instead of the
  former `input_mapping<Key> Map` + `requires mapping_view<Map, Key>` pair.
  The mapped_* aliases probe the exact expression `m[k]` with the same value
  categories as the concept, so a map whose `operator[]` takes the key by
  non-const lvalue reference no longer satisfies the concept while
  hard-erroring in the alias.
- **`competing_dijkstras::init()` is removed.** "The heap top, if any blue
  candidate remains, is blue" is now a class invariant that `add_blue_source`,
  `add_red_source` and `advance()` maintain, so a sourced object is ready to
  iterate like every other algorithm. Delete the `init()` call; nothing
  replaces it. Forgetting the old call silently yielded red-claimed vertices.
- **`bidirectional_dijkstra::run()` returns the algorithm, not the distance.**
  Like every other `run()` in the library, it drains and returns `*this`; the
  distance is read through the new `dist()` accessor — `alg.run().dist()`.
  `run()` is now idempotent (the second call used to return `infty`) and the
  result survives being ignored. Its traits flag `store_path` is renamed
  `store_paths`, matching `dijkstra`.
- **`biobjective_dijkstra::relax()` is private.** A public relaxation step let
  callers push arbitrary labels behind the traversal's back — the same hole as
  the removed `push_tarjan`. Insert labels through `add_source(v, blue, red)`,
  which has the same semantics. The class gains the family's
  `reached(v)` / `reached_map()` accessors.
- **`add_source` preconditions are strict family-wide.** `dijkstra` and
  `competing_dijkstras` used to assert only "not currently in the heap", which
  admitted re-adding a *settled* vertex and silently corrupted stored paths
  and distances; every `add_source` now asserts the vertex is untouched.
- **Stored members are always views, library-wide** (the
  `std::ranges::transform_view` precedent): `reverse_view`, `subgraph_view`
  and `undirect_view` require a `graph_view` template argument, and every
  algorithm's class head requires `graph_view` / `undirected_graph_view` for
  its graph and `mapping_view` for its stored maps. Constructors always route
  through `views::graph_all` / `maps::mapping_all`, so a non-view member type
  was a legal spelling whose constructor could never run — and a raw container
  member silently deep-copied its argument. Explicitly spelled raw-container
  members (`dijkstra<static_digraph, static_map<…>>`) are now ill-formed;
  value ownership is spelled `graph_owning_view` / `mapping_owning_view`,
  which store the same bytes. Code using CTAD or the `views::…` factories —
  which only ever produce view types — is unaffected.

- **`static_filter_map::filter()` returns a `std::ranges::subrange` of the
  new named `static_filter_map::filter_iterator`, and `melon::intrusive_view`
  is gone** (`detail/intrusive_view.hpp` deleted; `filter()` was its last
  user, the other containers having moved to `std::ranges::subrange` over
  hand-written intrusive iterators). The old return type embedded three
  lambdas: unnameable, non-default-constructible, unstorable, and its
  iterator was input-only. The new range is multipass (`forward_range`),
  borrowed, and storable; it stays sentinel-ended on purpose — making it
  common needs a clamp in the increment that measured 1.5x on dense scans —
  so pipe through `std::views::common` for an iterator pair. The bit-scan
  fast path also now engages for *any* common integral `iota_view`, not
  `iota_view<K, K>` exactly: `filter(std::views::iota(0, n))` against a map
  keyed by an unsigned type used to fall back silently to the generic
  per-key branch, 10-50x slower. Scan throughput is unchanged (re-measured
  at 1/10/50/90% density, 64K and 4M keys). Migration: code that spelled
  the range type re-spells it as
  `std::ranges::subrange<static_filter_map<K>::filter_iterator,
  std::default_sentinel_t>`; range-for and `auto` users are unaffected.

### Added

- **Stored maps are exposed as map views.** The `reached()`/`reached_map()`
  rule generalised: every per-key accessor over a stored map gains a
  pluralised `*s_map()` companion returning a read-only view
  (`maps::mapping_all`, valid while the algorithm lives and stays put).
  `dinitz`/`edmonds_karp` gain `flow(a)` and `flows_map()` — closing the
  documented per-arc flow gap; `dijkstra` gains `dists_map()`;
  `network_voronoi` gains `dists_map()` and `clusters_map()`;
  `breadth_first_search` gains `pred_vertices_map()`, `pred_arcs_map()` and
  `dists_map()`; `depth_first_search` gains `pred_vertices_map()`,
  `pred_arcs_map()` and `depths_map()` (each gated on the trait that stores
  the map, like the per-key accessor it accompanies). `dijkstra`'s pred maps
  stay unexposed on purpose: they store `std::optional<arc>`, not the `arc`
  that `pred_arc()` answers, and `path_to()` already covers that use. Pinned
  by `api_review.stored_maps_are_exposed_as_map_views`.
- **Result maps are extractable.** Every stored-map accessor
  (`flows_map()`, `dists_map()`, `reached_map()`, `component_ids_map()`, …)
  became a ref-qualified pair following `std::views::all`'s ref-or-owning
  split: lvalue algorithms hand out a `mapping_ref_view` as before, and an
  expiring algorithm — `std::move(alg).flows_map()` — moves the stored map
  into a `mapping_owning_view` that outlives it. Extraction is terminal,
  `std::move(alg).base()`'s contract: the member left behind is valid but
  empty. The rvalue path also closes a latent hazard — the old unqualified
  accessors bound to temporaries and returned a view into an expiring
  object. The computed `reached_map()`s (`dijkstra`, `network_voronoi`,
  `strongly_connected_components`, `biobjective_dijkstra` — derived from
  richer state, no stored bool map) extract by moving their backing map
  (status enums, component indices, Pareto fronts) into the returned lambda
  map, so extraction works uniformly across the family.
  `traversal_forest` delegates extraction to its inner BFS. Pinned by
  `api_review.expiring_map_accessors_extract_the_stored_map`.
- **std/STL alignment sweep** across the utility layer. `rational` replaces
  its six macro-generated relationals with hidden-friend `operator==` /
  `operator<=>` (`std::weak_ordering` — unnormalized representations make
  `1/2` and `2/4` equivalent, not equal), gains the compound assignments
  (`+=` did not compile before) and constrains its mixed-operand operators to
  number-like types (arithmetic or `bounded_value`), so `rational + string`
  is rejected at the constraint instead of erroring inside the body.
  `d_ary_heap` gains `std::priority_queue` parity: `emplace`, C++23
  `push_range`, member + ADL `swap` (the updatable heap swaps its id and
  index maps along), and a `priority_compare` typedef; the
  `updatable_priority_queue` concept's `priority()` loosens `same_as` to
  `convertible_to`, admitting the by-const-reference STL shape like `top()`
  already did. `mutable_digraph` gains `clear()`. View adaptors
  (`reverse_view`, `undirect_view`, `subgraph_view`, `induced_subgraph_view`)
  gain default constructors gated on `default_initializable`, and
  `views::reverse(views::reverse(g))` unwraps to the adapted graph like
  `std::views::reverse`. `erdos_renyi` constrains its generator with
  `std::uniform_random_bit_generator`; `alias_method_sampler` exposes
  `result_type`. The intrusive iterators (`mutable_digraph`, the
  dijkstra-family path iterators) now advertise `iterator_concept =
  forward_iterator_tag` with `iterator_category = input_iterator_tag`, the
  honest split for prvalue-returning iterators (`std::views::zip` precedent),
  and `prefetch_range` uses `std::ranges::data`, fixing a hard error for C
  arrays. `[[nodiscard]]` lands on every rational / `bounded_value` /
  geometry operation, the knapsack `solution_*()` accessors and
  `static_digraph_builder::build()`.
- **`melon::traversal_algorithm` and `melon::rooted_traversal_algorithm`**
  (`utility/algorithmic_generator.hpp`): the algorithm-object lifecycle as a
  named, tested contract — `reset()` restores the constructor's state, `run()`
  drains and returns the algorithm, `current()`/`advance()` require
  `!finished()`, no post-construction step. Every algorithm is statically
  asserted against it, and BFS/DFS/`topological_sort` gained the traits
  concepts (`breadth_first_search_traits`, …) the dijkstra family already had,
  so a misspelled traits flag fails the constraint instead of silently
  defaulting.
- **`graph_for`, `undirected_graph_for` and `mapping_for`**: every algorithm
  and view constructor is now constrained on what its member initializers
  actually do — wrapping through `graph_all` / `mapping_all` into the
  always-a-view member — so `std::is_constructible` answers honestly in both
  directions.
- **Relocation is sound and rebases.** Algorithms that cache incidence
  cursors (`depth_first_search`, `strongly_connected_components`,
  `connected_components`, `dinitz`, `traversal_forest`) re-aim every cursor at
  the *new* graph member when they are moved, using the vertex key stored
  beside each cursor and `consumable_input_view`'s consumed counter
  (new `rebase()` / `(range, consumed)` members). This fixes a use-after-free
  in the defaulted moves over a by-value subgraph — an algorithm over an
  owned, filtered subgraph now relocates soundly mid-run. For graphs that opt
  into `melon::enable_borrowed_graph`, the rebase compiles away and the move
  stays the defaulted memberwise one.
- **`melon::make_static_digraph`**
  (`melon/utility/make_static_digraph.hpp`): rebuilds any outward-incidence
  graph as a `static_digraph` whose vertices are renumbered `0..n-1` in the
  order a comparator induces, and translates given vertex and arc maps onto
  the new handles. Returns one flat, structured-binding-ready tuple:
  `auto [g, dist] = make_static_digraph(old_g, cmp, std::tie(dist));` — pass
  maps with `std::tie` to translate through references, `std::make_tuple` to
  hand over ownership.
- **`static_map` accepts sized forward ranges** (one `ranges::distance` pass,
  the way std containers accept forward iterators), which makes the
  `forward_range` constraint on `static_digraph` / `static_forward_digraph`
  constructors true instead of a lie. **`d_ary_heap`** forwards its base's
  single-comparator constructor (previously dead code), now `explicit`.

- **Removed the `fhamonic` umbrella namespace.** Everything now lives directly
  in `namespace melon`: replace `fhamonic::melon::` with `melon::`.
- **Mapping views moved from `melon::views` to `melon::maps`.** `views::map`,
  `views::mapping_all`, `views::mapping_all_t`, `views::true_map`,
  `views::false_map`, `views::identity_map` and `views::element_map` are now
  `maps::…`. `melon::views` holds graph views only — the two are different
  abstractions that happened to share the word "view", and `views::map` next to
  `views::reverse` read as though it transformed a graph. `mapping_ref_view`
  and `mapping_owning_view` stay in `melon`, symmetric with `graph_ref_view`
  and `graph_owning_view`; so do the mapping *concepts*.
- **`rational`, `integer`, `make_rational`, `bounded_value` and `const_value`
  moved to `melon::numeric`.** At namespace scope those names are generic
  enough to collide with a user's own, and `integer` was not even one — it is
  a `rational` with a unit denominator.
- **The traits concepts are plural.** `dijkstra_trait`,
  `bidirectional_dijkstra_trait`, `network_voronoi_trait`,
  `biobjective_dijkstra_trait`, `competing_dijkstras_trait` and
  `alias_method_sampler_trait` are now `…_traits`, matching
  `bentley_ottmann_traits`. `default_bentley_ottmann_traits` became
  `bentley_ottmann_default_traits`, matching every other
  `<algorithm>_default_traits`.
- **`vertex` / `arc` are private on every graph type.** `mutable_digraph` and
  `views::subgraph` published them while `static_digraph` and
  `complete_digraph` did not; `vertex_t<T>` / `arc_t<T>` are the supported
  spelling and work for all of them.
- **`disjoint_sets`' three maps are private**, and
  `strongly_connected_components::push_tarjan()` is now the private
  `_push_tarjan()` — pushing onto Tarjan's stack behind the traversal's back
  was never meant to be callable.
- **`melon::cpo::incident_edges_fn` is `melon::cpo::incidence_fn`**, the only
  CPO whose function-object name did not match the object it defines
  (`melon::incidence`).
- **`run()` returns `Algo &` on every algorithm** rather than `void` on ten of
  them and `Algo &` on four, matching `reset()`. Additive: callers discarding
  the result are unaffected. This now includes `bidirectional_dijkstra` — its
  `run()` used to return the computed distance and is covered in the breaking
  changes above; the answer is read through `dist()`.
- **`views::induced_subgraph::vertices()` returns a `ref_view`** over the
  stored range rather than a copy of it, so its type changed. This is what
  makes the view usable with an owned vertex range at all.
- **`noexcept` removed where it was a lie** (see *Fixed* below). Code that
  relied on those specifications — a `static_assert(noexcept(…))`, or a
  `noexcept` operation of your own that wrapped one — will need updating.
- **`subgraph::disable_arc` / `enable_arc` are non-`const`**, matching
  `disable_vertex` / `enable_vertex`. The filter is part of the view's value.
  Calls on a `const subgraph &` no longer compile — they only ever did for a
  `mapping_ref_view` filter; with a `mapping_owning_view` one the pair did not
  compile at all, which is now fixed.
- **Three single-argument constructors are `explicit`** —
  `static_filter_map(size_type)`, `static_digraph_builder(std::size_t)` and
  `graphviz_printer(const G &)` — matching `static_map`'s. `graphviz_printer
  p = g;` and `static_filter_map m = 10;` no longer compile.
- **`bounded_value`'s unary `operator-` is `= delete`d** where the negation of
  the *range* does not fit: unsigned `T`, or signed `T` with
  `Min == numeric_limits<T>::min()`. It used to produce
  `bounded_value<T, -Max, -Min, PS>` — bounds that bracket nothing for
  unsigned, ill-formed at the signed edge. Widen with `.bound<...>()`,
  subtract from a zero-valued `bounded_value`, or cast to `T`.
- **`current()` returns a read-only range** on
  `strongly_connected_components` (`std::span<const vertex>`) and
  `connected_components`. It is a window onto the algorithm's own buffer,
  which the next `advance()` rewrites.
- **Compiler baseline is GCC 14 / C++23 or Clang 18 / C++23** (GCC 15 / C++26
  is recommended; GCC 14/15, Clang 18 and MinGW GCC 15 are all tested in CI). When the standard library does not provide
  `std::ranges::concat_view` (`__cpp_lib_ranges_concat`), melon transparently
  falls back to a bundled implementation; the former `MELON_ENABLE_GCC14_SUPPORT`
  (aka `gcc14_compat`) CMake option was removed in favor of this automatic
  feature-test-macro detection. Earlier pre-releases required GCC 15 / C++26.
- **Dropped the range-v3 dependency.** melon is now dependency-free; the
  `melon/1.0.0-alpha.1` package on Conan Center still predates this change.
- **Graph views follow the `std::ranges` class/adaptor split, and adaptors
  support pipe syntax.** The classes are now `melon::reverse_view`,
  `melon::subgraph_view`, `melon::induced_subgraph_view` and
  `melon::undirect_view`; the old names in `melon::views` remain as the
  *adaptor objects*, so every call spelling (`views::reverse(g)`,
  `views::subgraph(g, vf, af)`, …) is unchanged. What breaks is naming the
  type: `views::reverse<G>` is now spelled `reverse_view<G>`. In exchange,
  `g | views::reverse`, `g | views::subgraph(vf)`,
  `g | views::graph_all` and closure composition
  (`views::reverse | views::subgraph()`) all work, and both spellings name
  exactly the same type, so the pipe is zero-cost by construction. Bound
  closures are self-contained like std's: `views::subgraph(vf)` copies the
  filter into the closure and each application copies (or, from an rvalue
  closure, moves) it into the view, so a closure is reusable and never
  dangles; the direct call keeps melon's reference semantics for lvalue
  filters. Custom adaptors derive `views::graph_adaptor_closure` — the
  melon analogue of `std::ranges::range_adaptor_closure`, which cannot be
  reused because its `operator|` requires a `std::ranges::range`.
- **`rational.hpp` and `bounded_value.hpp` moved from `melon/utility/` to
  `melon/numeric/`**, so the directory matches the `melon::numeric`
  namespace they already declare, the way `melon/views/` matches
  `melon::views`.
- **Algorithms are ranges, not `std::ranges` views.**
  `algorithm_view_interface` no longer derives from
  `std::ranges::view_interface`, so `std::ranges::view` and
  `std::ranges::enable_view` are now false for every algorithm. They used to
  be true, which made an lvalue algorithm piped into a standard adaptor
  (`alg | std::views::take(3)`) deep-copy the whole algorithm -- heap and
  vertex maps included -- and run on the copy, leaving the original
  unconsumed. Adaptors now wrap a `ref_view` around an lvalue and move an
  rvalue. The base bought nothing else: `empty()`, `front()` and
  `operator bool` all require `forward_range`, and algorithm ranges are
  input-only.
- **`d_ary_heap::top()` returns `const value_type &`**, the
  `std::priority_queue` shape, instead of a copy per call. The
  `priority_queue` concept accordingly asks
  `{ q.top() } -> std::convertible_to<value_type>` rather than
  `same_as<value_type>`, so heaps may return by value or by reference --
  the old spelling baked the copying shape into the concept and rejected
  every STL-shaped heap. The reference is invalidated by `push()`, `pop()`,
  `promote()` and `demote()`; copy first when the entry must survive one of
  those (the in-tree algorithms already do).
- **Single-pass iterators advertise `iterator_concept`, not
  `iterator_category`.** `algorithm_iterator`, `consumable_iterator`, the
  `intrusive_view` iterator and the bundled `concat_view` fallback's
  iterator declared `iterator_category = input_iterator_tag` while their
  post-increment returns `void` (P0541), so the Cpp17InputIterator
  operations the category promises (`*it++`) were ill-formed. They now
  expose `iterator_concept = input_iterator_tag` and no category:
  `std::input_iterator` is still satisfied, `std::iterator_traits` stays
  empty, and a pre-ranges algorithm rejects them at its constraint instead
  of failing mid-instantiation.
- **`connected_components::current()` returns `std::span<const vertex>`**,
  the type `breadth_first_search::traversal()` and
  `strongly_connected_components::current()` already settled on, instead of
  a `ref_view` over its internal queue.

### Fixed

- **Moved-from `static_map`, `static_filter_map` and the digraphs violated
  their invariants.** The defaulted moves nulled the buffer but kept `_size`
  (and `mutable_digraph`'s counts and list heads), so a moved-from map
  answered `size() == N` over a null buffer and copying it dereferenced
  null — a reachable state, since algorithms take their graph by value. The
  moves are hand-written (`std::exchange` to the empty state); a moved-from
  container is now a valid empty one, and both static digraphs inherit the
  fix through their `static_map` members.
- **`static_filter_map::filter()` dangled for rvalue non-iota key ranges.**
  The range was passed as an lvalue into `views::transform`, wrapping an
  rvalue container in a `ref_view` of a temporary dead at the semicolon.
  It is now forwarded, so the pipeline owns rvalues (`owning_view`) and
  still references lvalues.
- **`dinitz` over a filtered subgraph did not compile.** Its per-vertex
  cursor maps go through `create_vertex_map<cursor>`, and `static_map`
  default-constructs its slots — but a cursor over a filtered subgraph's
  incidence range holds a `filter_view` whose capturing-lambda predicate is
  not default-constructible, so the map type was unformable.
  `consumable_input_view`'s owning specialisation now stores its range in a
  `std::optional`: a default-constructed cursor is *disengaged* (assignment
  and destruction only — the contract the maps already live by, since
  `reset()` re-seeds every slot before use), and copying or moving one yields
  another disengaged cursor. The borrowed specialisation every melon
  container lands on is unchanged.
- **`dinitz` and `edmonds_karp` hung forever for capacity types without a
  `numeric_limits` specialization.** The primary template's `max()` returns
  `T{}` — a zero infinity — so a conforming custom capacity type compiled
  cleanly and looped forever. Both class heads now require
  `std::numeric_limits<value_t>::is_specialized`, turning the hang into a
  concept-level rejection.
- **`dinitz` and `edmonds_karp` BFS faulted on graphs without
  `num_vertices`.** Both walked `_bfs_queue` with a vector iterator while
  `push_back`-ing into it — safe only under the `reserve()` that
  `has_num_vertices` gates, and a use-after-free once the queue reallocated
  (caught by ASan on a conforming graph whose vertex range is a filter view,
  so the sized-range `num_vertices` fallback is unavailable). The walk now
  mirrors `breadth_first_search`'s cursor split — iterator when the reserve
  is guaranteed, index otherwise — measured at a 2–4% cost on the BFS phase
  for the index arm only, and none for the reserved one. Pinned by the new
  `test/unsized_digraph.hpp` fixture in both algorithms' tests.
- **`graphviz_printer` was broken three ways.** `print` discarded every
  iterator `std::format_to` returned, so positional output iterators (a
  `char *`) rewound and overwrote on each call — it now takes the iterator
  by value, threads it, returns it, and is constrained
  `std::output_iterator<const char &>`. A member rename had hit the
  `"graph ["` string literal, emitting a spurious `_graph` node and losing
  the graph attributes. And the constructor silently bound a temporary
  graph into its `reference_wrapper`; a deleted `const G &&` overload now
  refuses it (the `mapping_ref_view` precedent).
- **Knapsack `set_budget` left a stale item filter, and the unbounded twin
  crashed on an empty feasible set.** Raising the budget after construction
  silently kept newly-affordable items excluded, returning a wrong
  "optimal" value; `set_budget` now re-derives through `reset()` in both
  twins (the setter costs a re-sort — the O(1) version answered wrongly).
  Testing it exposed a second defect: `unbounded_knapsack_bnb`'s solvers
  lacked the `it == end` guard the bounded twin always had, so `run()` with
  no feasible item jumped `goto begin` into the loop body and dereferenced
  `end` — hang or garbage; both solver functions are now guarded.
- **`bentley_ottmann`'s defaulted move compared through the moved-from
  object, and `reset()` bricked it.** The sweep trees' comparator holds a
  `std::cref` to the current event point, and `std::set` carries its
  comparator with it on move — so a moved algorithm's trees kept ordering
  against the *moved-from* object's member, a use-after-free once the source
  died (ASan-confirmed; the copy half of the same defect was already retired
  with copy itself). Both sweep points now live behind a single
  `unique_ptr`, so their address survives the move and the defaulted moves
  are sound; the comparison hot path is unchanged — the `reference_wrapper`
  already was one indirection, it just lands on a heap anchor now.
  `reset()` used to clear the event queue with no way to refill it (the
  constructor consumed the id range and dropped it), leaving the object
  permanently `finished()`; the range is now stored — the class head takes a
  `std::ranges::view` id-range parameter captured through `views::all`, like
  every algorithm stores what it runs over — and `reset()` re-seeds and
  replays the sweep, which also requires the range to be a `forward_range`.
  CTAD spellings are unaffected; only explicit specializations name the
  range type where they named the id type. The tag-dispatch constructor is
  also constrained on the delegate it forwards to, matching `dijkstra`.
  Pinned in `test/bentley_ottmann.cpp` (`mid_run_move`,
  `reset_replays_the_sweep`).
- **`strongly_connected_components::same_component()` returned wrong
  answers.** It compared Tarjan *lowlinks*, which are not uniform within a
  finished component — for the single-component graph `0→1, 0→2, 1→0, 2→1`
  it answered `same_component(0,1)` but not `same_component(0,2)` — and two
  unreached vertices compared "same" through the sentinel. `same_component()`
  is removed. The algorithm instead takes a traits parameter
  (`strongly_connected_components_traits`, the `topological_sort` pattern)
  with a `store_component_ids` flag: when set, a dense component id —
  emission order, so reverse topological order of the condensation — is
  written per member as the component is popped, and the flag-gated
  `component_id(u)` and `component_ids_map()` expose the ids; the
  same-component query is spelled `component_id(u) == component_id(v)`.
  Without the flag none of these exist and the id map is
  `[[no_unique_address]]` air, so the default configuration pays nothing
  beyond the counter behind the new `num_components()` — not gated, since
  the count of components yielded so far is meaningful without the ids.

#### Third API-review pass

- **`graph_ref_view` and `undirected_graph_ref_view` accepted a temporary
  whose conversion materialises the graph.** `convertible_to<T, G &>` alone
  let a handle type with both `operator G&()` and `operator G()` bind, and
  the view then pointed at an object dying at the end of the
  full-expression. Both constructors now carry the `std::ranges::ref_view`
  bindable-test that `mapping_ref_view` already had, plus the conditional
  `noexcept` measuring the user conversion.
- **`static_digraph::in_arcs()` yielded arc ids in descending order** while
  `out_arcs()` is ascending: the constructor's counting sort filled each
  bucket backwards over ascending ids. It now walks the ids in reverse, so
  both incidence ranges come out ascending and arc maps indexed inside an
  `in_arcs` loop are read with a forward stride.
- **`dinitz::flow_value()` / `minimum_cut()` and `edmonds_karp`'s twins are
  `[[nodiscard]]`** -- they were the only value-producing members in the
  library without it, and a discarded `flow_value()` reads as a no-op
  statement. `dinitz`'s deduction guides also name their map parameter
  `CapacityMap` instead of the copy-pasted `LengthMap`.

#### Second API-review pass

- **Three algorithms kept a cursor as an iterator into their own buffer and
  defaulted their copy.** `kruskal::_cursor` points into `_sorted_edges`, and
  `knapsack_bnb::_best_sol` / `unbounded_knapsack_bnb::_best_sol` hold
  iterators into `_value_cost_pairs`. A memberwise copy handed the new object
  iterators into the *source*, so `kruskal::finished()` — comparing against the
  copy's own `end()` — never came true and `advance()` walked off the end
  (ASan: heap-buffer-overflow), while a knapsack copy outliving its source read
  freed memory. All three first learned to copy-then-rebase; the defect class
  was then closed outright when algorithm copies were deleted (see the
  breaking changes) — a move transfers the vector's buffer, so the cursors
  stay valid without rebasing.
- **Cursors over a non-borrowed range aliased the range they were copied or
  moved from.** `consumable_input_view`'s owning specialisation keeps an
  iterator that may refer *back* into the range it holds -- a
  `std::ranges::filter_view` iterator, which is what every `views::subgraph`
  incidence range yields, holds a pointer to its parent view. The defaulted
  copy and move therefore aimed the new cursor at the old range. This bit
  ordinary use, not just copies: these cursors live inside
  `depth_first_search::_stack`, so a plain vector reallocation moved them and a
  DFS over a filtered subgraph read freed memory with no copy anywhere in the
  program. The owning specialisation now tracks how far it has walked and
  re-derives the iterator in a user-provided copy and move; its copy members
  are constrained, so a cursor over a move-only range is honestly
  not-copy-constructible rather than declared-but-ill-formed. **The borrowed
  specialisation -- what every melon container lands on -- is untouched: 16
  bytes, no counter, no extra work in `advance()`.** Only the `filter_view`
  path pays, and it already carried 32 bytes of view: 48 becomes 56.
- **`views::induced_subgraph` over an owned vertex range was not a graph.**
  `vertices()` returned by value, and `std::views::all_t` of a temporary
  container is a move-only `std::ranges::owning_view`, so the member was
  ill-formed and `graph<induced_subgraph>` silently came back false while
  construction still compiled. It now hands out a `ref_view`.
- **`traversal_forest` could not take an owned graph.** It stored the graph
  view twice — once itself, once inside its `breadth_first_search` — and built
  the second from the first as an lvalue, which needs a copy
  `graph_owning_view` does not have. It now stores it once, reached through
  the new `breadth_first_search::base()`.
- **`consumable_input_view::operator=` could not bind the expression it exists
  for.** `_remaining_out_arcs[u] = out_arcs(_graph, u)` — the re-seeding idiom
  the header documents — is a prvalue, which never bound to `operator=(R &)`.
  It only compiled at all because the borrowed specialisation happened to have
  a non-`explicit` converting constructor where the primary one was `explicit`.
  Both are `explicit` now and both take the range by forwarding reference.
- **`views::subgraph` was missed entirely by the 1.0.0 `noexcept` sweep.**
  Every member was unconditionally `noexcept`, including `create_vertex_map` /
  `create_arc_map`, which allocate, and the six members that build a
  `filter_view`. Forwarding members now carry a conditional specification, the
  rest carry none — the same rule the other views follow.
- **`num_vertices()` carried no `noexcept` specification in any view**, while
  its `num_arcs()` / `num_edges()` sibling carried a conditional one, silently
  dropping a guarantee `static_digraph` does give. Fixed in `graph_ref_view`,
  `graph_owning_view`, `views::reverse`, `views::undirect` and both undirected
  views, which now also spell their guards as the existing `has_num_vertices` /
  `has_num_arcs` / `has_num_edges` concepts.
- **`views::graph_all` and `views::undirected_graph_all` claimed `noexcept`
  unconditionally** for the ref-view branch, although the converting
  constructor performs a `static_cast` a user conversion operator can make
  throwing. Now measured, as `maps::mapping_all` already did.
- **`bidirectional_dijkstra::length_type` was keyed on the vertex handle**
  rather than the arc, disagreeing with the class's own default `Traits`; it
  worked only because every melon container spells both handles `unsigned int`.
  The same class combined the two frontiers with a raw `+` at four sites
  instead of `Traits::semiring::plus`, silently wrong for any semiring whose
  `plus` is not addition, and built its status map through a member call rather
  than the CPO, rejecting graphs whose maps come from ADL.
- **`dinitz::minimum_cut()` tested `out_arcs` and then built its view over
  `in_arcs`**, so a graph with a viewable out-incidence and a non-viewable
  in-incidence failed to compile. `flow_value()` and `minimum_cut()` are now
  `const` on both flow algorithms.
- **`competing_dijkstras` and `biobjective_dijkstra` still defaulted `Traits`
  on their deduction guides**, computing it over the deduced (reference) graph
  type while the class computes it over `views::graph_all_t` — so
  `decltype(competing_dijkstras(g, b, r))` and
  `competing_dijkstras<G, BLM, RLM>` were different types. Same fix the other
  four algorithms got in 1.0.0. Both classes also now require
  `has_vertex_map<Graph>`, which they had always used.
- **`topological_sort` required `has_num_vertices` without declaring it** — it
  reserves `num_vertices(_graph)` and keeps an iterator cursor whose stability
  depends on that reserve — so a graph without it got a hard error inside the
  constructor instead of a constraint failure.
- **`alias_method_sampler` wrote `mutable` distributions through a `const`
  `operator()`**, so two threads sampling from one `const` sampler raced — the
  same defect `erdos_renyi`'s function-local statics had, in a per-object form.
  Both distributions are locals now, and an empty item range is asserted rather
  than handed to a distribution with an inverted range.
- **`bentley_ottmann::segment_entry` declared assignment operators the compiler
  had already deleted**: three `const` data members. Same trap that made
  `induced_subgraph` fail `std::movable`.
- **Ten missing standard includes** that only compiled transitively: `<span>`
  (`strongly_connected_components`, `breadth_first_search`), `<stdexcept>`
  (`static_map`, `static_filter_map` — both `throw std::out_of_range`),
  `<cassert>` (`biobjective_dijkstra`, `competing_dijkstras`, `subgraph`),
  `<ranges>` (`biobjective_dijkstra`, `intrusive_view`, `graphviz_printer`) and
  `<optional>` (`dijkstra`, `topological_sort`).
- `melon::detail::intrusive_view` inherited `std::ranges::view_base`
  *privately*; `has_arc_removal` was the one `has_*` concept calling its CPO
  unqualified; `mapped_reference_t` / `mapped_const_reference_t` probed an
  rvalue where the `mapping` concept probes an lvalue.
- `static_digraph_builder` copied each arc property **four times** per
  `add_arc` — into `add_arc`'s by-value parameter, into `push_arc`'s, into a
  `make_tuple`, and into `push_back` as an lvalue. Only the first remains.

#### First API-review pass

- **Two constructors read ranges they had just forwarded away.**
  `static_forward_digraph` did `std::move(targets)` on a *forwarding
  reference* — stealing from an lvalue the caller still owned — and then read
  `targets` in its asserts; `static_digraph` forwarded both endpoint ranges
  and then read them in its asserts and in both degree-counting loops. Both
  now read the members they were forwarded into. It only ever worked because
  `static_map`'s range constructor copies.
- **`complete_digraph::num_arcs()` wrapped silently past 65536 vertices**: it
  returned `std::size_t` but computed `n * (n - 1)` after casting both operands
  to `arc`, so 70000 vertices reported 604'962'704 arcs instead of
  4'899'930'000. It is computed in `std::size_t`, and asserts when the count no
  longer fits in an arc handle.
- **`erdos_renyi` was unseedable and raced.** The engine *and* the distribution
  were function-local `static`s, so no call could be reproduced and concurrent
  calls shared both. There is now an `erdos_renyi<G>(n, density, generator &)`
  overload; the two-argument one seeds a local engine.
- **`weakly_connected_components` was constrained on the wrong concepts** — the
  adjacency ones, where `views::undirect` needs the incidence ones — so a graph
  with adjacency but no incidence passed the constraint and then hard-errored
  inside `undirect`, and one with incidence but no adjacency was rejected
  although it works.
- `bounded_value`'s CRTP downcast went through `reinterpret_cast`, which is
  undefined behaviour for a derived-to-base cast; it is a `static_cast` now.
  `const_value`'s value-discarding constructor gained the `assert(v == V)` that
  was the only thing standing between a caller and an ignored argument.
- `topological_sort` built its in-degree map with `create_vertex_map<long
  unsigned int>` against a member declared `vertex_map_t<Graph, std::size_t>`:
  the same type on LP64, three different ones on MinGW-w64 and MSVC, where it
  did not compile.
- `DEFINE_RATIONAL_OPERATOR` is `#undef`'d, so `rational.hpp` — and therefore
  `all.hpp` — no longer leaks an unprefixed function-like macro into every TU.
  `rational`'s converting `operator rational<ON, OD>()` is `const`, so a const
  rational can be converted at all.
- `operator*` on the path iterators of `dijkstra`, `topological_sort` and
  `bidirectional_dijkstra`, and on `complete_digraph`'s arc iterator, returned
  a `const` prvalue: it inhibits moves and disagrees with the `reference`
  typedef beside it.
- `static_digraph_builder` includes `<cassert>` for the `assert` it uses, and
  lost a stray `;`. `breadth_first_search` is forward-declared with the tag it
  is defined with (`-Wmismatched-tags` on Clang and MSVC).
- **`noexcept` no longer promises what the code cannot keep.** Every algorithm
  constructor, `reset()`, `add_source()`, `advance()` and `run()` asserted
  `noexcept` while allocating a heap, a queue and one map per vertex, and while
  calling the user's length map, semiring, comparator and graph. So did
  `static_digraph`'s three-argument constructor, every container's
  `create_vertex_map` / `create_arc_map`, `mutable_digraph::create_vertex` /
  `create_arc`, `disjoint_sets::push` / `find` / `merge`,
  `d_ary_heap`'s sift helpers and `static_digraph_builder`. A throw out of any
  of them called `std::terminate` with no diagnostic. The forwarding views
  (`graph_ref_view`, `graph_owning_view`, `views::reverse`, `views::undirect`,
  `undirected_graph_ref_view`, `undirected_graph_owning_view`) keep a
  *conditional* specification instead, so wrapping a graph neither invents a
  guarantee it does not give nor discards one it does.
- **The undirected-graph CPOs computed their `noexcept` from the wrong
  overload.** Every `is_noexcept` helper in `undirected_graph.hpp` was called
  as `is_noexcept<T &>()`, which — since `const T &` collapses to `T &` when
  `T` is itself a reference — made the helper select and measure a *non-const*
  member while `edges`, `num_edges`, `edge_endpoints`, `incidence` and `degree`
  all take `const T &`. For a type with distinct const and non-const overloads
  the specification described the overload that is never called.
  `experimental/planar_map.hpp` had the same shape and the same fix.
- **`finished()` and `current()` are `const` on every generator.** They were
  not on `connected_components`, `strongly_connected_components` and
  `traversal_forest`, so a `const` reference to one of those could not even be
  asked whether it was done.
- **Wrapping a graph in a view no longer loses its endpoint maps.**
  `graph_ref_view`, `graph_owning_view` and `views::reverse` spelled them
  `sources_map()` / `targets_map()`, names no CPO ever looked for, so
  `arc_sources_map(view)` fell back to a synthesised per-arc lambda instead of
  the container's flat array — once per settled vertex in `dijkstra::advance`.
  They are `arc_sources_map()` / `arc_targets_map()` now, matching `subgraph`,
  and are guarded by the new `has_arc_sources_map` / `has_arc_targets_map`
  concepts.
- **Copying a view or an algorithm works.** `views::reverse r2(r);`,
  `breadth_first_search b2(b);` and the like used to pick the converting
  constructor instead of the copy constructor whenever the operand was a
  mutable lvalue, which failed to compile. Copies of `topological_sort`,
  `connected_components` and `strongly_connected_components` are now
  independent and usable as well; they previously shared a cursor with the
  original and walked off the end of it.
- `views::induced_subgraph` is a proper view: it is assignable, and passing one
  along by value no longer wraps it in an extra owning layer.
- `melon/all.hpp` compiles again (it referenced two non-existent headers) and
  now includes the entire public API; a dedicated test translation unit keeps
  it from rotting.
- All headers use `#pragma once`, fixing a duplicated include guard that made
  `algorithm/unbounded_knapsack_bnb.hpp` a silent no-op when included after
  `algorithm/knapsack_bnb.hpp`, as well as collision-prone unprefixed guards
  in `utility/rational.hpp` and `utility/geometry.hpp`.
- `mapping_owning_view` (and therefore every algorithm taking a mapping by
  value) is now `std::movable` even when it owns a non-assignable functor such
  as a capturing lambda, using a `std::ranges`-style movable-box.
- Two GCC-only constructs that Clang rightfully rejects: an ill-formed
  deduction guide in the `concat_view` fallback (redundant, removed) and
  missing `template` keywords on dependent names in `bentley_ottmann.hpp`.
  melon now compiles with Clang 18.

### Added

- `melon::enable_borrowed_graph<G>` / `melon::borrowed_graph<G>` — an opt-in
  trait mirroring `std::ranges::enable_borrowed_range`: true when the ranges a
  graph hands out stay valid independently of the graph *object*. Specialised
  for `graph_ref_view`, `undirected_graph_ref_view` and
  `views::complete_digraph`, and propagated through `views::reverse` and
  `views::undirect`.
- `breadth_first_search::base()` — the graph view the traversal was built over,
  so a composing algorithm can keep one copy of it instead of its own
  alongside.

- `static_map::resize(n)` — reallocates and keeps the elements that still fit,
  where `reset(n)` keeps nothing. Neither initialises: after a growing
  `resize` the new tail is indeterminate, unlike `std::vector::resize`.
- **`static_digraph_builder::build()` is ref-qualified**: `build() &` copies
  the property vectors as before, `build() &&` moves them out. `add_arc` is
  ref-qualified to match, so a chain keeps its value category and
  `static_digraph_builder<G, P>(n).add_arc(...).add_arc(...).build()` reaches
  the moving overload with no `std::move`. Previously `add_arc` returned
  `builder &` unconditionally, which would have sent
  `std::move(b).add_arc(...).build()` back to the copying overload.
- `has_arc_sources_map<G>` / `has_arc_targets_map<G>` concepts.
- `[[nodiscard]]` was dropped from every constructor — including the defaulted
  copy/move ones, where it is pure noise — and from the void-returning
  `remove_vertex`, `remove_arc`, `change_arc_source` and `change_arc_target`
  CPOs, where it means nothing. It stays on everything that returns a value the
  caller has to use.
- The docs now state which algorithms are ranges and which are not, and record
  melon's `noexcept` policy: see
  [Algorithms are ranges](docs/algorithms/index.md).
- **`Traits` now has a default on every algorithm that takes one.**
  `dijkstra<G, LM>`, `bidirectional_dijkstra<G, LM>`, `network_voronoi<G, LM>`
  and `alias_method_sampler<R, P>` previously required all three arguments, so
  the type CTAD produced could not be written down. The default moved from the
  deduction guides onto the class templates, which is also what makes the two
  spellings name the same type.
- `melon/version.hpp` with `MELON_VERSION_MAJOR` / `MELON_VERSION_MINOR` /
  `MELON_VERSION_PATCH` and a combined `MELON_VERSION` macro for feature
  testing. It is the single source of truth for the version number, parsed by
  both `CMakeLists.txt` and `conanfile.py`.
- CMake install and export rules: `cmake --install` now ships the headers
  together with `melonConfig.cmake` / `melonConfigVersion.cmake`
  (`SameMajorVersion`, architecture-independent), so non-Conan consumers can
  use `find_package(melon CONFIG)`. The library target is also exported under
  the conventional namespaced alias `melon::melon`, and carries the
  `cxx_std_23` compile feature.
- `MELON_SANITIZE` CMake option (e.g. `-DMELON_SANITIZE=address,undefined`)
  to build the test suite with sanitizers.

### Internal restructuring

No behaviour change; verified by the suite and, for the Dijkstra hot loop, by
benchmark (300k vertices / 2.4M arcs, minimum over eight alternating runs:
**-2.5 %** against the previous release state, i.e. within noise and not a
regression).

- **The graph views' forwarding members exist once each.** `graph_ref_view`,
  `graph_owning_view` and `views::reverse` each spelled out the same sixteen
  accessors; `undirected_graph_ref_view` and `undirected_graph_owning_view` the
  same ten. They now come from `detail::graph_forwarding_interface` and
  `detail::undirected_graph_forwarding_interface`, to which a view supplies only
  `_forwarding_base()`. `views::reverse` redeclares just the eight accessors
  whose meaning genuinely crosses over, and name hiding does the rest — which
  also gets availability right for free, since `reverse::out_arcs` is
  constrained on `has_in_arcs<Graph>`. Forty-eight member definitions became
  twenty-six. This duplication is *why* the 1.0.0 `noexcept` sweep could touch
  67 members across four files and still leave `num_vertices` without a
  specification in every one of them.
- **Twenty-three of the twenty-six CPOs lost their private `is_noexcept()`.**
  Each one re-ran the same `if constexpr` its `operator()` ran, in a different
  place — which is exactly how `undirected_graph.hpp` came to measure an
  overload the operator never called. They are now one constrained overload per
  protocol, with the `noexcept` beside the expression it measures, so the two
  cannot drift. The three left (`arcs`, `arcs_entries`, `out_neighbors` /
  `in_neighbors`, and the three `create_*_map` factories) keep the `if
  constexpr` form: their branches encode a ranking policy over several possible
  fallbacks, and splitting them would be less readable, not more. Fixed on the
  way: `arc_sources_map` / `arc_targets_map` measured `melon::arc_source` for a
  branch that actually builds a lambda-backed view.
- **`detail::intrusive_view` uses `detail::movable_box`.** It solved the
  "a capturing lambda is copy-constructible but not assignable" problem with
  three `std::optional`s and two hand-written assignment operators, written out
  once for the view and again for its iterator — while `mapping_owning_view`
  already used `movable_box` for the same problem. `movable_box` moved to
  `melon/detail/movable_box.hpp` and gained an in-place constructor; the four
  assignment operators are now `= default`, and every iterator sheds three
  optional discriminants.
- **`run()` lives in `algorithm_view_interface`.** `while(!finished())
  advance();` was retyped in eleven algorithms and missing from `kruskal`, which
  inherits the interface. The three whose iteration is internal keep their own
  body with the same shape (drain, return the algorithm, results through
  accessors): `dinitz` and `edmonds_karp` are not generators, and
  `bidirectional_dijkstra` is a point query whose answer `dist()` reads after
  `run()`.
- **`prefetch_keys_and_values`** replaces the identical three- and four-line
  prefetch preamble in `dijkstra`, `bidirectional_dijkstra`, `network_voronoi`,
  `competing_dijkstras`, `biobjective_dijkstra` and `edmonds_karp`.

The `advance()` bodies of the four Dijkstra variants were **not** merged. They
share a shape, not an implementation — the state and the relaxation step differ
in every one — so a common skeleton would have meant three or four CRTP hooks
through the library's hottest loop in exchange for about forty lines.

### Build system

- `MELON_BUILD_TESTS` now defaults to `PROJECT_IS_TOP_LEVEL`: consuming melon
  via `add_subdirectory` no longer builds the test suite nor downloads GTest.
- Warning flags are applied privately to the test executable instead of being
  attached to the exported `melon` target; consumers no longer inherit
  `-Wall -Wextra …`.
- AddressSanitizer is no longer silently enabled whenever `libasan` is found;
  use `MELON_SANITIZE` instead. The GCC-specific
  `-fconcepts-diagnostics-depth` flag is now gated on the compiler, unblocking
  MSVC/MinGW configuration.
- `cmake_minimum_required` unified to 3.24 (was an inconsistent 3.12/3.13).
- Conan: `package_type = "header-library"`, and the recipe now configures the
  top-level `CMakeLists.txt` (like a regular consumer) instead of a duplicate
  library target defined under `test/`.

### `experimental/`

- Everything under `melon/experimental/` moved from `namespace melon` to
  **`namespace melon::experimental`**, so the code that carries no stability
  guarantee can no longer be reached from the stable namespace by accident.
- `planar_map.hpp` and `dual.hpp` were repaired (two missing semicolons, a
  stale `#include "melon/planar_map.hpp"` path, and the `has_arc_twin` /
  `has_arc_face` concepts that `dual.hpp` used but nobody had written) and
  are now covered by `test/experimental.cpp`, which keeps them compiling.
- `scapegoat_tree.hpp` and `doubly_connected_digraph.hpp` are still
  unfinished — they do not compile once instantiated — so they are **no
  longer shipped**: both the CMake install rules and the Conan package now
  exclude them. They remain in the repository, each with a file-level note
  describing what is missing.

### CI

- New jobs: Clang 18, ASan + UBSan (via `MELON_SANITIZE`, through the pure
  CMake + FetchContent path), and a `clang-format --dry-run -Werror` check
  pinned to clang-format 18 — the whole tree has been reformatted with it.
- `actions/checkout` bumped from the deprecated v3 to v7; Conan profiles moved
  from `.github/workflows/` to `.github/conan-profiles/`.
