# Melon library design review

*Date: 2026-07-30 — scope: all headers under `include/melon/` (60 files, ~14,200 lines).*
*Every load-bearing claim was verified by compilation with GCC 15.1 (`-std=c++23 -Wall`), and by execution under AddressSanitizer where the claim is about runtime behavior. Findings marked "executed" were reproduced in a running binary.*

## Fix status — updated 2026-07-30

The three proposals of [DESIGN_CHANGE.md](DESIGN_CHANGE.md) are **implemented** (constructor
honesty, relocation-rebase policy, lifecycle contract; 340/340 tests pass, new behaviors pinned in
`test/api_consistency.cpp` and `test/api_review.cpp`, relocation soundness ASan-verified). The
checklist below tracks every finding of this review; unchecked items remain open.

**Tier 1 — runtime bugs**
- [ ] 1. `same_component()` compares lowlinks (wrong answers)
- [x] 2. DFS move use-after-free — fixed by the rebase policy (Proposal 2); the refuted pin at
      `test/api_review.cpp` is replaced by a behavioral move+copy-mid-run test
- [ ] 3. `bentley_ottmann` comparator self-references on copy/move and `reset()` bricks the object
      — *partially*: the missing `not_self` guard is fixed (constructor no longer hijacks copies);
      the comparator rebind and `reset()` remain open
- [ ] 4. `static_filter_map::filter()` rvalue dangle
- [ ] 5. Moved-from `static_map`/digraphs stale `_size` over null buffer
- [ ] 6. Flow algorithms' zero-infinity infinite loop
- [ ] 7. `graphviz_printer` (format_to iterator, `_graph [` literal, temporary dangle)
- [x] 8. `competing_dijkstras::init()` trap — `init()` removed; blue-top is a class invariant
      maintained by `add_*_source`/`advance()` (Proposal 1); `advance()` gained its assert
- [ ] 9. Knapsack `set_budget` stale item filter

**Tier 2 — lying type traits**
- [x] 10. Unconstrained algorithm constructors — all 14 sites constrained on
      `graph_storable_as`/`mapping_storable_as`/`undirected_graph_storable_as` (Proposal 3).
      Follow-up ruling: stored members are always *views* — every algorithm class head now
      requires `graph_view`/`undirected_graph_view`/`mapping_view` (the transform_view precedent),
      the raw-storage fallback was removed, and value ownership is spelled
      `graph_owning_view`/`mapping_owning_view`; pinned in api_consistency.cpp §4
- [x] 11. BFS/topological_sort unconstrained hand-written copies — constrained on
      `copy_constructible`/`copyable`; same sweep applied to kruskal
- [x] 12. `static_digraph` forward-range constraint lie — dissolved from the static_map side
      (sized-forward-range constructor via `ranges::distance`)
- [x] 13. Non-view `Graph` parameters — `reverse_view`/`subgraph_view`/`undirect_view` heads now
      require `graph_view`; both owning views gained `is_object_v`
- [ ] 14. `reverse_view::arcs_entries` hard-errors on tuple-shaped entries
- [ ] 15. Experimental headers (planar_map ADL loop, dual.hpp, doubly_connected_digraph,
      scapegoat_tree)

**Tier 3 — API inconsistencies**
- [ ] 16. `views::subgraph` capability drops (`arcs_entries`, `num_arcs`, degrees, borrowedness)
- [ ] 17. `undirect_view::incidence()` double copy; missing `degree` forwarding; orphan
      `adjacency()`
- [ ] 18. Direct-call vs piped filter-map semantics divergence
- [ ] 19. `noexcept` honesty on `current()`/`consumable_iterator`/geometry comparator
- [x] 20. Algorithm-family drift — `store_paths` rename, strict `add_source` preconditions,
      `reset()` semantics documented in the `traversal_algorithm` concept,
      `bidirectional_dijkstra::run()` normalized (+ cached idempotent `dist()`), traits concepts
      for BFS/DFS/toposort, `biobjective_dijkstra` `relax()` privatized + `reached()`/
      `reached_map()` + missing asserts/nodiscard. *Still open from this item*: `set_*_length_map`
      on only two of four Dijkstras, dinitz/edmonds_karp indeterminate `_s`/`_t`, dinitz recursion
      depth, `constexpr` coverage drift
- [ ] 21. Container drift — *partially*: `d_ary_heap` single-comparator constructor forwarded and
      `explicit` (dead code revived). Everything else in item 21 remains open
      (`static_filter_map` sweep, iterator↔const_iterator, `updatable_d_ary_heap` map
      constructors, `create_arc` asserts, `exclusive_scan` int accumulator, unbounded-knapsack
      constraint, `bounded_value`, integer geometry truncation, `handle_event` copies, sampler
      constraints, voronoi accessors)

**Tier 4 — std::ranges/STL alignment**
- [ ] `iterator_concept` on `intrusive_iterator_base`/`static_filter_map::iterator`
- [ ] `std::ranges::data` in `prefetch.hpp`
- [ ] `rational` modernization (`<=>`, compound assignment, `common_type`, hidden friends)
- [ ] `[[nodiscard]]` sweep — *partially*: `biobjective_dijkstra::is_dominated()`/`pareto_front()`
      done; numeric/geometry/knapsack/builder/run() sites remain
- [ ] `d_ary_heap` `emplace`/`push_range`/`swap`; `priority()` `same_as` → `convertible_to`
- [ ] Adaptor gaps (default ctors, double-reverse unwrap, `complete_digraph` naming, container
      `operator==`, `mutable_digraph::clear()`)
- [ ] std random idioms (`uniform_random_bit_generator`, `floating_point` Prob, weight
      normalization)
- [ ] `graphviz_printer::print` iterator threading + `output_iterator` constraint (overlaps
      Tier 1 #7)

**New finding (discovered during implementation)**: `dinitz` over a graph whose incidence ranges
are lambda-predicated views (any *filtered* subgraph) is unconstructible for a pre-existing
reason unrelated to the relocation work — its per-vertex cursor maps go through
`create_vertex_map<cursor>`, and `static_map`'s element default-initialization requires
default-constructible elements, which a `filter_view` holding a capturing lambda is not.
Constructibility of cursor maps deserves its own ruling (e.g. an `optional`-like slot or a
`movable_box`-backed cursor).

---

Overall: the core (`graph.hpp` CPOs, `mapping.hpp`, the main containers and views) is unusually
disciplined — the `views::all` ref/owning precedent, one-overload-per-protocol CPOs with honest
`noexcept`, and `not_self` guards are applied consciously and are pinned by tests. The defects found
are almost all places where a stated convention was *not carried through* to a sibling. They cluster
into five themes: wrong-answer bugs (2), lifetime/relocation unsoundness (4), lying type traits
(constraint says yes, body explodes), `noexcept`/`iterator_category` honesty gaps, and
`views::subgraph` silently dropping capabilities. One finding **refutes an existing test pin**,
flagged at the end.

## Tier 1 — Wrong results or UB at runtime (all compile+run verified)

1. **`same_component()` returns wrong answers** —
   `include/melon/algorithm/strongly_connected_components.hpp:283-287` compares Tarjan *lowlinks*,
   which are not uniform within a finished SCC. For the single-SCC graph `0→1, 0→2, 1→0, 2→1`,
   `same_component(0,1)` is true but `same_component(0,2)` is false (executed). Also, two unreached
   vertices compare "same" via `INVALID_COMPONENT`. Fix: write a real component id per member when
   the component is popped (the loop at lines 269-274 already visits each member).

2. **DFS move is a use-after-free over a non-borrowed graph — refutes the pin at
   `test/api_review.cpp:214`.** `include/melon/algorithm/depth_first_search.hpp:94-102` constrains
   *copy* on `borrowed_graph` but defaults *move*, claiming `consumable_input_view` re-derivation
   makes it sound. It only rebases the iterator-into-own-range half; ranges cached in `_stack` from
   a by-value `subgraph_view` capture the old object's `_graph` member. ASan: heap-use-after-free
   after move + destroy + advance. The same mechanism threatens `traversal_forest`'s defaulted move.
   Recommended fix: hand-write the move to re-obtain each frame's range from the new `_graph` using
   the `_consumed` counter that already exists for exactly this idiom.

3. **`bentley_ottmann` defaulted copy/move dangle through the tree comparators** —
   `include/melon/algorithm/bentley_ottmann.hpp:187-191`: `segment_cmp` holds
   `std::cref(_current_event_point)`; `std::set` copies the comparator verbatim, so the copy's trees
   compare against the *source's* members (ASan-confirmed use-after-free once the source dies). Its
   range constructor also lacks the `not_self` guard every sibling has (line 157), so copying a
   non-const lvalue selects the wrong constructor; and `reset()` (lines 193-197) bricks the object
   because the consumed id range is never stored.

4. **`static_filter_map::filter()` dangles for rvalue non-iota ranges** —
   `include/melon/container/static_filter_map.hpp:421-426` passes `r` as an lvalue into
   `views::transform`, wrapping an rvalue `std::vector` in a `ref_view` of a dead temporary
   (ASan-confirmed). Fix: `std::forward<R>(r)`.

5. **Moved-from `static_map`/`static_filter_map`/digraphs violate their invariant** —
   `include/melon/container/static_map.hpp:67`,
   `include/melon/container/static_filter_map.hpp:258`, inherited by both static digraphs, and
   analogously `include/melon/container/mutable_digraph.hpp:120`: defaulted moves null `_data` but
   keep `_size`, so a moved-from map reports `size()==4` over a null buffer; copying it SEGVs
   (executed). `traversal_forest(std::move(g))` is a supported idiom, so this state is reachable.
   Fix: `std::exchange(_size, 0)` in hand-written moves.

6. **Flow algorithms hang forever for value types without `numeric_limits`** —
   `include/melon/algorithm/edmonds_karp.hpp:126`, `include/melon/algorithm/dinitz.hpp:186`: the
   primary `numeric_limits<T>::max()` returns `T{}` = zero-infinity; a conforming custom capacity
   type compiles cleanly and loops forever (executed, killed at 120 s). Require
   `numeric_limits<T>::is_specialized` or take the sentinel as a constructor parameter.

7. **`graphviz_printer` is broken three ways** — discards the iterator every `std::format_to`
   returns, so any positional output iterator (e.g. `char*`) produces interleaved garbage
   (`include/melon/utility/graphviz_printer.hpp:184-277`); emits `_graph [pad=...]` instead of
   `graph [...]` — a member rename hit the string literal, creating a spurious dot node and losing
   the graph attributes (lines 224-226); and stores `reference_wrapper<const G>` that silently binds
   a temporary graph (ASan-confirmed dangle) where `mapping_ref_view` already shows the
   deleted-rvalue-binding antidote (line 48).

8. **`competing_dijkstras`' range interface contradicts its documented contract unless the caller
   remembers `init()`** — no other algorithm has a mandatory post-source step; iterating without it
   yields red-claimed vertices (executed: yields a RED vertex where the correct output is empty).
   `init()` is also UB on an empty heap, and `advance()` lacks the family's `assert(!finished())`.
   Fold the skip-to-first-blue into `add_*_source` or the `finished()`/`current()` pair, and delete
   `init()`.

9. **Knapsack `set_budget` leaves a stale item filter** —
   `include/melon/algorithm/knapsack_bnb.hpp:208-211` (and the unbounded twin): raising the budget
   after a `reset()` silently excludes newly-feasible items → wrong "optimal" value. Have
   `set_budget` re-derive via `reset()`.

## Tier 2 — Lying type traits: the concept says yes, instantiation hard-errors

All compile-checked; these are the worst kind for a generic library because the failure escapes the
immediate context.

10. **Algorithm constructors are unconstrained forwarding templates** —
    `include/melon/algorithm/dijkstra.hpp:90-99` and siblings (bidirectional, biobjective,
    competing): `is_constructible_v<dijkstra<ref_view, static_map<…>>, G&, RawMap&>` answers true,
    construction hard-errors in the mem-init list. Constrain on
    `constructible_from<Graph, views::graph_all_t<G>> &&
    constructible_from<LengthMap, maps::mapping_all_t<M>>`.

11. **BFS/topological_sort hand-written copies are unconstrained** —
    `include/melon/algorithm/breadth_first_search.hpp:114-137` (and 346-381),
    `include/melon/algorithm/topological_sort.hpp:140-167`: `std::copyable` answers true for a
    move-only `graph_owning_view` graph, copy hard-errors. DFS already shows the correct constrained
    pattern; `traversal_forest` inherits the lie transitively.

12. **`static_digraph`/`static_forward_digraph` constructors accept `forward_range` but require
    `random_access_range` to compile** — `include/melon/container/static_digraph.hpp:130-137`,
    `include/melon/container/static_forward_digraph.hpp:24-36`: `constructible_from` with
    `forward_list` is true, construction explodes inside `static_map`. Make the constraint and the
    body agree.

13. **`reverse_view`/`subgraph_view`/`undirect_view` accept non-view `Graph` arguments** —
    `include/melon/views/reverse.hpp:17`, `include/melon/views/subgraph.hpp:14`,
    `include/melon/views/undirect.hpp:14`: `constructible_from<reverse_view<static_digraph>,
    static_digraph&>` is true, construction hard-errors. std pins the precedent (`transform_view`
    requires `view<V>`); constrain the class on `graph_view`. Same genre: `graph_owning_view<G&>`
    is a legal type that can't construct (`include/melon/views/graph_view.hpp:269`,
    `include/melon/views/undirected_graph_view.hpp:167`).

14. **`reverse_view::arcs_entries` hard-errors instead of answering false for tuple-shaped
    entries** — `include/melon/views/reverse.hpp:127-136` assumes `.first/.second`; a mere
    `graph<reverse_view<…>>` check on a tuple-entries graph is a compile error because the `auto`
    return deduction instantiates the lambda body. Use `std::get<0>/get<1>` or constrain the member
    away cleanly.

15. **Experimental headers regress fixed patterns**:
    - `include/melon/experimental/planar_map.hpp:476-496` reintroduces the ADL-self-dependency hard
      error graph.hpp's variable-template CPOs were specifically built to kill ("satisfaction of
      atomic constraint depends on itself" for any type in `melon::experimental`).
    - `planar_map.hpp:12-14` probes a two-argument `vertex_coordinates(v, v)` member the CPO never
      calls (unary members unrecognized; binary members pass the concept then hard-error), and
      `vertex_coordinates_t` (lines 52-54) invokes the CPO without the graph argument, so both arc
      endpoint-coordinate protocols are dead code.
    - `include/melon/experimental/dual.hpp:58-64` calls `std::transform` where
      `std::views::transform` is meant (hard error on first use of `out_arcs`); nearly every member
      is an unconditional-`noexcept` terminate-bomb; the `create_*_map` constraints check the
      default `std::size_t` ValueType instead of `T`.
    - `include/melon/experimental/doubly_connected_digraph.hpp` does not compile, and its
      incidence iterators' sentinel test (`_cursor == _first`) is true *before the first element*.
    - `include/melon/experimental/scapegoat_tree.hpp` has a parse error (`cosnt`), missing
      `<cmath>`, `erase` paths that return nothing (UB), a double-destruction in `delete_node`, and
      an inverted balancing criterion (`log(n)*log(ALPHA)`, negative for meaningful α).

## Tier 3 — API inconsistencies within the library's own conventions

16. **`views::subgraph` silently drops base-graph capabilities** (all compile-checked):
    - No `arcs_entries` forwarding — an entries-only graph wrapped by a filterless subgraph stops
      modeling `graph` at all (the same bug fixed for `graph_ref_view`, pinned at
      `test/api_review.cpp:319-362`), and a graph with its own `arcs_entries` gets the synthesized
      fallback instead.
    - No `num_arcs`, `out_degree`, `in_degree` forwarding (`graph_forwarding_interface` forwards
      all three; `subgraph_view` derives `graph_view_base` directly).
    - No `enable_borrowed_graph` specialization for the filterless case, so
      `depth_first_search(views::subgraph(g))` loses the copyability `depth_first_search(g)` has.
    Related: `views::complete_digraph` has no O(1) `out_degree`/`in_degree` members although the
    answer is the constant n−1, and its non-sized `in_arcs` kills the fallback
    (`include/melon/views/complete_digraph.hpp:135-149`).

17. **`undirect_view::incidence()` deep-copies a copyable non-borrowed graph into both lambdas** —
    2 filter-map copies per call (executed count), and the borrowedness the copy would buy is false
    anyway: `_capture()` branches on `copy_constructible` alone while the trait requires
    `borrowed && copyable` (`include/melon/views/undirect.hpp:30-37` vs 172-174). Also
    `undirected_graph_forwarding_interface` forwards everything except `degree`
    (`include/melon/views/undirected_graph_view.hpp:29-108`), and `undirect_view::adjacency()`
    (undirect.hpp:126-131) is an orphan member with no CPO behind it.

18. **`views::subgraph(g, filter)` vs `g | views::subgraph(filter)` have different semantics** for
    an lvalue filter map: the direct call stores a `mapping_ref_view` (writes via `disable_vertex`
    hit the caller's map), the piped form decay-copies (`graph_view.hpp:403-447`, deliberate
    self-containment). std avoids the divergence by always decay-copying. Owner's call — one
    behavior, or a loud doc.

19. **`noexcept` honesty regressions** (the library's own pinned rule):
    - `current()` in BFS/DFS/topological_sort never measures the vertex copy the by-value return
      performs — with a throwing-copy vertex type `noexcept(dfs.current())` is true → terminate
      (`breadth_first_search.hpp:218`/420, `depth_first_search.hpp:143`,
      `topological_sort.hpp:201`). `competing_dijkstras`/`biobjective_dijkstra` spell it correctly;
      `dijkstra::current()` carries none at all — three spellings in one family.
    - `consumable_iterator`'s increment/compare are unconditionally `noexcept` while wrapping
      arbitrary iterators (`include/melon/detail/consumable_view.hpp:42-60`).
    - Geometry's point comparator is unconditionally `noexcept` over user comparison operators
      (`include/melon/utility/geometry.hpp:29-35`).

20. **Family drift in the algorithm objects**:
    - `store_path` vs `store_paths` traits flag (`bidirectional_dijkstra.hpp:29` vs
      `dijkstra.hpp:31`).
    - `add_source` precondition strength differs: `!= IN_HEAP` in dijkstra/competing permits
      re-adding a settled vertex (silently corrupts stored paths/distances) vs strict
      `PRE_HEAP`/`!reached` elsewhere.
    - `set_*_length_map` setters exist on two of the four Dijkstras only.
    - `reset()` means "blank" in six algorithms and "re-seeded/re-run" in `topological_sort` and
      `traversal_forest`, undocumented.
    - BFS/DFS/topological_sort take an unconstrained `typename Traits`; the Dijkstras have traits
      concepts.
    - `bidirectional_dijkstra::run()` is the family's only value-returning `run()`, is not
      `[[nodiscard]]`, is non-idempotent (second call returns `infty` — executed), and the result is
      irrecoverable (no `dist()` accessor).
    - `biobjective_dijkstra`: public `relax()` breaks encapsulation (same genre as the removed
      `push_tarjan`, pinned at `test/api_consistency.cpp:183-196`); no convenience source
      constructor; no `reached()`; the only non-`constexpr` main constructor; `advance()` misses
      `assert(!finished())`.
    - `dinitz`/`edmonds_karp`: `_s`/`_t` indeterminate after the 2-arg constructor;
      `minimum_cut()`/`flow_value()` before `run()` read indeterminate state; dinitz's DFS recurses
      O(V) deep (stack overflow on path graphs) where edmonds_karp is iterative. Otherwise the two
      flow APIs are cleanly interchangeable — except dinitz's copy requires `borrowed_graph` while
      edmonds_karp's doesn't, worth one doc sentence.

21. **Container drift**:
    - `static_filter_map` has zero `constexpr` and missing `[[nodiscard]]` where `static_map` is
      fully annotated (its own comments claim parity); `iterator` is not convertible to
      `const_iterator` (mixed comparison ill-formed — every std container provides this); missing
      `key_type`/`mapped_type` typedefs, subscripts by `size_type` instead of `key_type`, no
      content-preserving `resize()`.
    - `d_ary_heap` does not forward its base's single-comparator constructor (dead code,
      compile-checked; and that base constructor should be `explicit` per the pinned rule).
    - `updatable_d_ary_heap` can never receive a stateful priority/id map — the commented-out test
      at `test/d_ary_heap.cpp:220-226` is the corpse of this.
    - `mutable_digraph::create_arc` is the only mutator without validity asserts
      (`mutable_digraph.hpp:222-224`).
    - `std::exclusive_scan(..., 0)` accumulates arc offsets in `int` (signed-overflow UB past
      `INT_MAX` arcs) in both static digraph constructors (`static_digraph.hpp:154-159`,
      `static_forward_digraph.hpp:43-45`); pass `arc{0}`.
    - `unbounded_knapsack_bnb` requires random-access items in `is_dominated` but its constraint
      accepts any `range` (`unbounded_knapsack_bnb.hpp:144`; hard error with `std::list`).
    - `bounded_value` arithmetic hard-errors hostilely (`void` compound literal) when no signed
      hierarchy type fits, even when the *range* trivially fits
      (`include/melon/numeric/bounded_value.hpp:229-236`); its explicit widening conversion operator
      is non-const and fully shadowed by the converting constructor (lines 321-325).
    - Default `cartesian` geometry silently truncates intersections (and divides by zero on
      vertical lines) for integer coordinates (`geometry.hpp:65-74`, 92-94; executed: (1, 1/3)
      comes back (1, 0)); constrain on exact division or divide through `make_rational` as the
      commented-out originals did.
    - `bentley_ottmann::handle_event` takes `const std::pair<intersection_type, events>&` while the
      map's `value_type` has a const key — every call materializes a temporary copying the point
      *and* the events vector (`bentley_ottmann.hpp:242`; executed). Its `endpoint_type` uses a
      comma inside `decltype`, taking only the second endpoint's type (lines 57-59).
    - `alias_method_sampler`: unconstrained `Prob` (an `int` prob map dies inside libstdc++
      `<random>` instead of at a melon concept), unconstrained `Generator` and `ItemRange`, and
      unnormalized weights silently build garbage tables where `std::discrete_distribution`
      normalizes.
    - `network_voronoi`: `store_cluster_adjacency` is inert — **pinned** as such
      (`test/network_voronoi.cpp:108-128`), no action until implemented; `set_kernels` narrows a
      64-bit vertex id in braced init (compile error); no per-vertex `dist()`/`cluster()` accessor
      after a run.

## Tier 4 — std::ranges / STL alignment opportunities

- **`iterator_concept`, not `iterator_category`, on proxy/prvalue iterators** —
  `include/melon/detail/intrusive_iterator_base.hpp:11` claims Cpp17 `forward_iterator_tag` while
  `operator*` returns a prvalue (verified; affects dijkstra/bidirectional/topological_sort path
  iterators, `mutable_digraph`, `network_voronoi`), and `static_filter_map::iterator` claims
  `random_access_iterator_tag` with a proxy reference. The std spelling (what `zip_view`/`flat_map`
  do): `iterator_concept = forward/random_access_iterator_tag` + `iterator_category =
  input_iterator_tag`. This is the one iterator-honesty sweep the library's own convention missed —
  `consumable_iterator`, `intrusive_view::iterator`, `concat_view::iterator` and
  `algorithm_iterator` all already document and apply it.
- **`std::ranges::data` instead of member `.data()`** in `include/melon/detail/prefetch.hpp:15` —
  hard-errors today for a C array, which *is* a `contiguous_range` (verified).
- **`rational`**: replace the six macro-generated relationals with `operator<=>`
  (`std::weak_ordering`, given unnormalized representations) + `==`; drop the C++20-redundant
  `!=`; add compound assignments (`+=` does not compile today), `std::common_type` and
  `std::numeric_limits` specializations, `std::hash`; prefer hidden friends over namespace-scope
  operator templates (ADL hygiene); constrain the mixed-type operators to number-like `T`.
- **`[[nodiscard]]` sweep** where a discarded result mimics a no-op statement (all verified silent
  under `-Wall`): every rational/bounded_value/geometry operator, knapsack `solution_*()`,
  `static_digraph_builder::build()` (both overloads), `bidirectional_dijkstra::run()`,
  `biobjective_dijkstra::is_dominated()`/`pareto_front()`.
- **`d_ary_heap` vs `std::priority_queue`**: naming already matches (`push/pop/top/size/empty`;
  `clear()` is a pinned extension). Add `emplace(...)` (algorithms currently `make_pair`+`push` per
  relaxation), C++23 `push_range`, member + ADL `swap`, `value_compare`-style typedef. In the
  `updatable_priority_queue` concept, `priority()` demands `same_as` where `top()` deliberately
  uses `convertible_to` — rejecting the by-const-reference STL shape
  (`include/melon/utility/priority_queue.hpp:28`); loosen to `convertible_to`.
- **Adaptor gaps vs std::ranges**: default constructors on view adaptors gated on
  `default_initializable` (std adaptors have them; melon's don't); `views::reverse(views::reverse(g))`
  should unwrap like `std::views::reverse`; `complete_digraph` breaks the class-in-`melon::` /
  factory-in-`views::` naming split every other view follows (and its constructor silently truncates
  `n` where `num_arcs()` asserts); equality operators on containers (every std container is
  equality-comparable; none of melon's are); `static_map`'s range constructors could accept sized
  forward ranges via `ranges::distance` like std containers accept input iterators (this also
  dissolves finding 12 from the other side); `mutable_digraph` lacks `clear()`; a
  `reserve_hint`-style sized opt-in on algorithm ranges would help `std::ranges::to<vector>`.
- **std random idioms**: constrain `erdos_renyi`/`alias_method_sampler` generators with
  `std::uniform_random_bit_generator`, `Prob` with `std::floating_point`; normalize sampler weights
  like `std::discrete_distribution`; expose `result_type`.
- **`graphviz_printer::print`**: thread and return the output iterator (`std::format_to`'s own
  contract) and constrain `OS` with `std::output_iterator<char>`.
- **`semiring`**: the concept structurally forces `zero/infty/plus/less` to be static constexpr
  *variables* — fine, but undocumented; `shortest_path_semiring::plus(infty, x)` overflows (UB for
  signed types), and `bidirectional_dijkstra::run()` evaluates `plus` of two live keys near
  `infty/2` — consider a saturating `plus` or a documented range requirement.
- **Aligned and correctly pinned — no action**: algorithm ranges being `input_range` but
  deliberately not `view`; the `base()` accessor shapes (ref = 1, owning = 4, adaptors = value
  pair); `graph_all`/`mapping_all` mirroring `views::all`; `algorithm_iterator`'s P0541-correct
  post-increment and empty `iterator_traits`; `static_map`'s indeterminate-on-resize contract;
  builder `add_arc` vs CPO `create_arc` naming split (doc-pinned).

## Nits (abridged)

- Dead commented-out code blocks in eight headers (`subgraph.hpp:211`, `geometry.hpp:46-64`,
  `connected_components.hpp:199-203`, `strongly_connected_components.hpp:264-267`,
  `static_filter_map.hpp:33-57`, `dual.hpp:91-109`, `bentley_ottmann.hpp:202`,
  `algorithmic_generator.hpp:30,67`).
- Load-bearing typos: `substract_overflows` is concept-required so users must reproduce the
  misspelling (`bounded_value.hpp:56`); `computeUpperBound` lone camelCase (`knapsack_bnb.hpp:51`);
  `max_incomming_flow` (`dinitz.hpp:150`); copy/move parameters named `bin` on defaulted members
  (`edmonds_karp.hpp:54-55`); `cosnt` (`scapegoat_tree.hpp:179`).
- `const`-prvalue return at `bentley_ottmann.hpp:101` — the anti-pattern dijkstra.hpp:257-260's own
  comment warns about.
- Missing includes that work only transitively: `<limits>` in `edmonds_karp.hpp`/both knapsacks,
  `<cmath>` in `scapegoat_tree.hpp`; `undirected_graph_view.hpp` uses `graph_adaptor_closure`
  without including `graph_view.hpp`.
- Scalars passed as `const std::size_t &` (`static_digraph.hpp:134`,
  `static_forward_digraph.hpp:33`); `set_arc_label(const arc &, const std::string)` by-value const
  string (`graphviz_printer.hpp:137`).
- `graphviz` labels interpolated unescaped; `scale` divides by zero for degenerate bounding boxes
  (`graphviz_printer.hpp:197-198`); `stdlib_check.hpp` included twice; `d_ary_heap.hpp` included
  unused by `alias_method_sampler.hpp`.
- `if constexpr(std::numeric_limits<float>::is_iec559)` guards a *double* division
  (`knapsack_bnb.hpp:43`, `unbounded_knapsack_bnb.hpp:45`).
- `intrusive_view` specializes `enable_view` although it already inherits `view_base`; its deduction
  guide reorders `(Deref, Incr)` relative to the template parameter list `<I, Incr, Deref, Cond>`
  (documented, still a trap).
- `identity_map::operator[]` decay-copies where `std::identity` forwards (`mapping.hpp:277-283`) —
  possibly deliberate for the mapping contract, worth a comment.
- `mapping_ref_view`'s converting constructor is non-`explicit` while `graph_ref_view`'s is
  `explicit` (`mapping.hpp:133` vs `graph_view.hpp:243`).
- Add-source default-argument passing drifts (`const length_type &` in dijkstra vs by-value
  elsewhere); `constexpr` coverage drifts across the Dijkstra family; `finished()` noexcept spelling
  drifts in generic BFS.
- `disjoint_sets::merge` doesn't assert its arguments are roots; `find` has no const (no-compression)
  overload; `static_map(it, it)` doesn't assert `begin <= end`; `at()` exceptions carry no context.
- Stale test comment at `test/subgraph.cpp:253-255` (contradicts the fix pinned 60 lines above);
  commented-out tests in `test/d_ary_heap.cpp` instead of skipped ones.

---

## Pin conflict to rule on

`test/api_review.cpp:214` pins "move stays available and is sound" for DFS over a by-value
subgraph — the ASan trace shows it is not (Tier 1, finding 2). The pin's rationale
(`consumable_input_view` re-derivation) covers only one of the two aliasing paths, so it appears to
be based on an incomplete premise rather than a deliberate design decision.

## Verification toolchain

`/opt/gcc-15/bin/g++ (GCC) 15.1.0`, `-std=c++23 -Wall` (`-fsanitize=address` for runtime claims).
Reproduction TUs were written per finding (`containers_*`, `views_*`, `algoA_*`, `algoB_*`,
`util_*`, `detail_*.cpp`); they can be regenerated from the file/line references above — each is a
minimal TU demonstrating exactly the claimed defect. Nothing in the library was modified by this
review.
