# Design change proposals

*Date: 2026-07-30. Companion to [REVIEW.md](REVIEW.md).*

## Context and verdict

The full-library review (see REVIEW.md) supports one overall judgment: **the architecture is sound
and should be hardened, not redesigned.** The three-layer core — CPO + concept vocabulary
(`vertices`/`out_arcs`/`arc_target`, member → ADL → synthesized fallback), the mapping layer
mirroring `views::all` (`mapping_all`/`mapping_ref_view`/`mapping_owning_view`), and algorithms as
lazy input ranges — is the same shape `std::ranges` and P1709 (`std::graph`) converged on, and the
lazy-range algorithm model is the library's real differentiator: early exit at a target,
`take_while` over a BFS, composing a traversal into a pipeline. Function-style APIs
(`dijkstra(g, s, w) → dist_map`) cannot express any of that without visitor callbacks, which is the
BGL ergonomics this library exists to escape. The review's failure pattern confirms the judgment:
almost every defect is a sibling failing to carry a *stated* house convention through, not a defect
of the conventions themselves.

However, three places are **under-specified rather than under-enforced** — the contract itself is
ambiguous, so "hardening" them as-is would freeze an ambiguity. These must be settled first,
because hardening (constraints, `noexcept`, pinned tests) makes whatever is there permanent:

1. The algorithm-object lifecycle is a convention, not a contract.
2. There is no single relocation (copy/move) policy for algorithms that cache incidence ranges.
3. Constructors are unconstrained forwarding templates, so `std::is_constructible` lies.

What this document deliberately does **not** propose: moving to function-style algorithms, visitor
callbacks, or coroutine `std::generator`. The first two lose the lazy-range differentiator;
`std::generator` would cost cheap idempotent `begin()`, resumability, and the allocated-state reuse
that `reset()` exists for, and its second-`begin()`-is-UB contract is strictly worse than what
`algorithmic_generator` already provides (P0541-correct post-increment, honest `iterator_concept`,
deliberately not a `view` — all pinned).

---

## Proposal 1 — Lifecycle: make the family contract a named concept

### Problem

The eight algorithm objects share a lifecycle by imitation, not by contract, and they have drifted:

- `reset()` means "blank, re-add sources" in six algorithms and "re-seeded, immediately runnable"
  in `topological_sort` / `traversal_forest` — undocumented.
- `add_source` preconditions differ: `!reached(s)` / `PRE_HEAP` (strict) in BFS/DFS/bidirectional
  vs `!= IN_HEAP` in `dijkstra`/`competing_dijkstras`, which permits re-adding a *settled* vertex
  and silently corrupting stored paths/distances.
- `competing_dijkstras` requires a mandatory post-source `init()` no sibling has; forgetting it
  yields silently wrong iteration (red-claimed vertices), verified by execution.
- `bidirectional_dijkstra::run()` is the family's only value-returning `run()`, is not
  `[[nodiscard]]`, is non-idempotent (second call returns `infty`, verified), and the result is
  irrecoverable afterwards.
- Result accessors drift: `dist_map()` here, a return value there, absent elsewhere;
  `store_path` vs `store_paths`; BFS/DFS/toposort take an unconstrained `typename Traits` while the
  Dijkstras have traits concepts.

### Design

The unifying insight: the library already wrote the correct contract once, in a comment in
`strongly_connected_components.hpp` — *"reset() restores exactly the state the constructor leaves
behind."* That one sentence unifies all eight: for algorithms whose sources are added after
construction, reset-to-blank; for those whose constructor seeds (toposort, traversal_forest),
reset-to-seeded. The drift exists because the sentence lives in one comment instead of a concept.

```cpp
// algorithmic_generator.hpp (or a new melon/algorithm.hpp)
template <typename A>
concept traversal_algorithm =
    std::ranges::input_range<A> &&
    requires(A & a, const A & ca) {
        { a.reset() }     -> std::same_as<A &>;   // postcondition: state == freshly constructed
        { a.run() }       -> std::same_as<A &>;   // drain; idempotent; finished() holds after
        { ca.finished() } -> std::convertible_to<bool>;
        a.current();                               // precondition: !finished()
        a.advance();                               // precondition: !finished()
    };

template <typename A, typename S>
concept rooted_traversal_algorithm =               // the add_source half of the family
    traversal_algorithm<A> &&
    requires(A & a, const S & s) {
        { a.add_source(s) } -> std::same_as<A &>;  // precondition: !reached(s), asserted
    };
```

Specific rulings the concept forces:

1. **`competing_dijkstras::init()` is removed.** "The heap top, if any, is blue" becomes a class
   invariant, restored at the end of every mutating member — `add_blue_source`, `add_red_source`,
   and `advance` each end with the red-draining loop that `init()` currently is. Cost is paid only
   when a mutation actually leaves red on top; `begin()` needs no ceremony; the documented contract
   ("iterating yields only the vertices blue claims") becomes unconditionally true. `advance()`
   gains the family's `assert(!finished())`.

2. **`bidirectional_dijkstra` joins the family.** `run()` returns `*this`; the answer moves to
   traits-gated, cached accessors `[[nodiscard]] dist()` and `path()`. Caching the best distance in
   a member is what makes `run()` idempotent. As a point-query algorithm it models plain
   `traversal_algorithm` whose iteration is internal — the concept requires the lifecycle, not that
   iteration yields anything interesting.

3. **One `add_source` precondition, the strict one** (`!reached(s)` / `PRE_HEAP`), asserted
   family-wide. If warm restarts are ever wanted, that is a future `re_source()` with stated
   semantics, not a loosened assert.

4. **Accessor naming becomes a rule**: traits-gated results are `dist(v)` / `dist_map()`,
   `pred_arc(v)`, `path_to(v)`, `reached(v)` / `reached_map()`, with the pairing rule the tests
   already half-pin (`reached()` implies `reached_map()`) applied family-wide. This gives
   `biobjective_dijkstra` its missing `reached()` and removes its public `relax()` (same
   encapsulation ruling as the removed `push_tarjan`, pinned in `test/api_consistency.cpp:183-196`).

5. **Traits get concepts everywhere, and the flag is `store_paths`, plural, everywhere.**
   BFS/DFS/toposort's unconstrained `typename Traits` becomes `bfs_traits` / `dfs_traits` /
   `topological_sort_traits`, so a misspelled flag fails the concept instead of silently falling
   back to nothing.

### Enforcement

House style, cheap: `static_assert(traversal_algorithm<dijkstra<...>>)` for every algorithm in
`test/api_consistency.cpp`, plus behavioral pins for the reset-equals-constructed contract. The
ninth algorithm then cannot drift.

### Observable API breaks

Exactly two, both of which currently mislead: `init()` disappears (its call sites simply delete the
line), and `bidirectional_dijkstra::run()` stops returning the distance (call sites switch to
`.run().dist()`).

---

## Proposal 2 — Relocation: teach the cursors to rebase, once

### Problem

Algorithms cache incidence cursors — `consumable_input_view` frames in `depth_first_search::_stack`,
dinitz's two per-vertex maps — whose *ranges* may capture the address of the stored graph (a
`subgraph_view`'s filtered ranges capture `this`; see `detail/borrowed_graph.hpp`). Relocating the
algorithm therefore invalidates them unless the graph is `borrowed_graph`. Today each algorithm
hand-rolls a partial answer:

- DFS constrains **copy** on `borrowed_graph` but leaves **move** defaulted — and the move is a
  verified heap-use-after-free over a by-value subgraph (REVIEW.md Tier 1, finding 2; refutes the
  pin at `test/api_review.cpp:214`).
- BFS/toposort constrain neither (their hand-written copies additionally lie to `std::copyable`
  for move-only graphs).
- dinitz constrains copy only; `traversal_forest` inherits the problem transitively.

The current design makes every algorithm's special members a soundness puzzle solved per class.

### Design

Solve it in the layer that already half-solves it. `consumable_input_view`'s primary template
already carries `_consumed` — exactly the state needed to re-derive a cursor against a re-obtained
range (it exists because copies/relocations of the *cursor itself* already needed it). It is
missing one member:

```cpp
// consumable_view.hpp, primary template
template <typename Rng>
constexpr void rebase(Rng && r) {   // same logical range, new provenance:
    _range = std::views::all(std::forward<Rng>(r));   // keep _consumed, reseek
    _reseek();
}
// borrowed specialisation: rebase(...) is a no-op — its iterators do not
// point at the range object, which is the definition of borrowed.
```

A keyed cursor wrapper then makes rebasing self-describing — the cursor remembers *which* incidence
range it is, so it can re-ask the new graph:

```cpp
// detail/incidence_cursor.hpp
template <typename Graph>
class out_arcs_cursor {
    vertex_t<Graph> _v;
    consumable_input_view_t<out_arcs_range_t<Graph>> _cursor;
public:
    constexpr void rebase(const Graph & g) {
        if constexpr(!borrowed_graph<Graph>)
            _cursor.rebase(melon::out_arcs(g, _v));
    }
    // empty() / advance() / current() forward to _cursor
};
```

The uniform policy — which is *more* permissive than today:

- **Move: always available, always sound.** Defaulted when `borrowed_graph<Graph>` (rebase is a
  no-op; every melon container lands here at zero cost, unchanged). Otherwise hand-written as:
  default-move the members, then `for(auto & frame : _stack) frame.rebase(_graph);` — three
  mechanical lines instead of a per-class puzzle. This fixes the ASan finding and makes the
  currently-refuted test pin true.
- **Copy: constrained on `std::copy_constructible<Graph>` (plus the maps), not on
  `borrowed_graph`.** The copy rebases its cursors against its own graph copy. This is an API
  *improvement*: DFS over an owned subgraph becomes honestly copyable, where today it is forbidden.
  The `borrowed_graph` constraint on copy was compensating for the missing rebase, not expressing a
  real semantic limit.

  > **Superseded by Addendum 3**: copy is gone entirely. The move half of this proposal — the
  > rebase engine and the hand-written moves — is unaffected and is what the machinery was
  > always for. Read this bullet as the historical rationale, not the shipped policy.
- **Handed-out maps get one documented caveat**: `reached_map()` et al. reference the algorithm
  object; relocating the algorithm invalidates previously handed-out maps (the same contract
  `std::ranges` adaptors have over a moved container).

### Fallback considered and rejected as the default

Storing non-borrowed graphs behind a heap indirection (the C++26 `std::indirect` shape) makes the
graph object stable across moves, buying sound *defaulted* moves for free — but it leaves copy
restricted (the copy's cursors would still aim at the source's graph), adds an indirection to the
hot path, and duplicates a solution the `_consumed` machinery already provides. Keep it as a
per-algorithm escape hatch, not the policy. The rebase design wins because the library already
built the engine (`_consumed`, `_reseek`) and never connected it to the special members.

### Enforcement

Behavioral tests per algorithm: construct over a by-value subgraph, advance, move, destroy the
source, continue — the exact TU that today fails under ASan becomes the regression test with the
expectation inverted.

---

## Proposal 3 — Constructor honesty: name the storability relation, constrain everything with it

### Problem

The lie has one shape across the library: a class-head constraint admits any `input_mapping` /
`graph`, but the constructor unconditionally routes arguments through `graph_all` / `mapping_all`,
which only ever produce *view* types. So `std::is_constructible_v<dijkstra<graph_ref_view<G>,
static_map<...>>, G&, RawMap&>` answers **true** and construction hard-errors in the mem-initializer
— outside the immediate context, the worst failure mode for generic code (verified; REVIEW.md
Tier 2, finding 10). The same shape recurs in BFS/toposort's unconstrained hand-written copies
(finding 11), `static_digraph`'s forward-range constructor (finding 12), the view classes accepting
non-view `Graph` parameters (finding 13), and `graph_owning_view<G&>` (finding 13).

There is also a mirror-image defect: a user who *explicitly spells*
`dijkstra<G, static_map<unsigned, int>>` cannot construct it from an actual `static_map`, because
the constructor force-wraps everything through `mapping_all` — so the property pinned for CTAD
("the type the user would have spelled out") fails from the explicit-spelling side.

### Design

Name the actual requirement once, next to the machinery that defines it:

```cpp
// views/graph_view.hpp
template <typename G, typename Stored>
concept graph_storable_as =
    std::constructible_from<Stored, views::graph_all_t<G>>;

// mapping.hpp
template <typename M, typename Stored>
concept mapping_storable_as =
    std::constructible_from<Stored, maps::mapping_all_t<M>> ||
    std::constructible_from<Stored, M>;    // raw storage for explicitly-spelled types
```

and constrain every algorithm constructor with it:

```cpp
template <typename G, typename M>
    requires detail::not_self<G, dijkstra> &&
             graph_storable_as<G, Graph> &&
             mapping_storable_as<M, LengthMap>
constexpr dijkstra(G && g, M && m)
    : _graph(views::graph_all(std::forward<G>(g)))
    , _length_map(/* if constexpr: direct construction when
                     constructible_from<LengthMap, M>, else mapping_all */) { ... }
```

> As shipped, the concepts are named `graph_for` / `mapping_for` /
> `undirected_graph_for` and carry no raw-storage branch, and the *position* of each
> constraint turned out to be load-bearing rather than cosmetic. See the two addenda
> below; this block is the proposal as written.

Two deliberate choices:

1. `is_constructible` now tells the truth in **both directions** — false for the
   raw-map-into-view-slot case that hard-errors today, true (and working) for everything it admits.
   CTAD is untouched: the deduction guides already produce exactly the types the concepts accept.
2. The raw-storage branch of `mapping_storable_as` fixes the mirror-image defect: explicitly
   spelled member types become constructible from raw storage, restoring the pinned
   "type you would have spelled out" property from both sides.

The same ruling then sweeps the mirrored cases mechanically:

- BFS/toposort hand-written copy ctor/assignment gain `requires std::copyable<Graph>` (DFS already
  shows the constrained pattern — this proposal composes with Proposal 2, which then *widens* the
  condition to plain copyability).
- View class heads become `template <graph_view Graph>` (`reverse_view`, `subgraph_view`,
  `undirect_view`) — the `std::ranges::transform_view` precedent; no user is broken because the
  factories already produce only `graph_all_t` types.
- `graph_owning_view` / `undirected_graph_owning_view` add `std::is_object_v<G>`.
- `static_digraph` / `static_forward_digraph`: rather than narrowing the constraint to
  `random_access_range`, prefer the more std-aligned move — give `static_map` a sized-forward-range
  constructor via `std::ranges::distance`, making the existing `forward_range` constraint *true*
  (std containers accept input iterators; this is the same spirit).
- `d_ary_heap` forwards its base's single-comparator constructor (currently dead code) and both
  become `explicit` per the pinned single-argument rule.

### Enforcement

The both-directions test idiom from the review checklist, in `test/api_consistency.cpp`: for each
constructor, one `static_assert(std::is_constructible_v<...>)` per admitted shape, one negative
assert per rejected shape, and a live construction for every positive — so constraint and body can
never disagree again.

### Observable API breaks

None for correct existing code. This proposal only turns silent hard errors into honest constraint
failures and makes previously-impossible constructions (raw storage into explicitly spelled types)
work.

---

## Ordering and scope

Implement in this order:

1. **Proposal 3 first** — pure tightening, zero behavior change for correct code, and it hardens
   the surface the other two build on.
2. **Proposal 2 second** — the cursor machinery (`rebase` in `consumable_input_view`, the keyed
   cursor, per-algorithm special members). Highest-value proof of concept: the `rebase` path plus
   DFS's move converts the one execution-refuted pin into a passing regression test.
3. **Proposal 1 last** — the lifecycle concept, asserted against the already-hardened family, with
   its two deliberate breaks (`init()` removal, `bidirectional_dijkstra::run()` return value)
   landing as code + test + doc in one change.

Proposals 2 and 3 are invisible to correct existing code. Proposal 1 has exactly two observable
removals, both of which currently mislead users.

### Additive follow-up (post-hardening, out of scope here)

A thin convenience layer for the 80% case — e.g. `melon::shortest_path(g, s, t, lengths)` returning
`std::optional<std::pair<dist, path>>` built on the existing algorithm objects. Gives drive-by users
(and LEMON/BGL migrants) a one-liner without touching the architecture.

---

## Addendum 1 (2026-07-30, post-implementation): stored members are always views

Ratified as a follow-up to Proposal 3 after all three proposals landed. Proposal 3 as written
preserved two storage modes for algorithm members — view types (what CTAD deduces) and explicitly
spelled raw containers (`dijkstra<G, static_map<…>>`), reachable through a direct-construction
fallback in `mapping_storable_as`/`graph_storable_as`. The follow-up ruling removes the second
mode:

- **Every algorithm class head now requires view members**: `graph_view` /
  `undirected_graph_view` for the graph parameter, `mapping_view` for stored map parameters — the
  same `transform_view<V> requires view<V>` precedent Proposal 3 applied to the view adaptors.
- **The `storable_as` concepts and `store_*` helpers collapsed to the wrap**: the
  direct-construction fallback is gone; they now state exactly "wraps through
  `graph_all`/`mapping_all` into the member".
- **Value ownership is spelled** `graph_owning_view` / `mapping_owning_view`, which store the same
  bytes the raw member would have (`movable_box<M>` is the map plus nothing).

**Why the reversal.** The raw-storage mode carried a trap of the exact genre this review hunts:
`dijkstra<static_digraph, …>(lvalue_graph, …)` silently deep-copied the whole graph. It also kept
the relocation policy's hardest case populated (copyable raw containers with std-borrowed ranges,
which cannot rebase), and it split the storage domain in two where the rest of the library —
adaptors, `graph_all`, `mapping_all` — had committed to views. The STL precedent is genuinely
split (`std::priority_queue` stores a raw `Container`; ranges adaptors store views); melon is a
ranges-native library, so the ranges analogy wins. The `std::is_constructible` honesty in both
directions is unchanged; only the domain shrank.

Pinned in `test/api_consistency.cpp` §4: raw-container spellings are ill-formed (checked through
dependent requires-expressions), the owning-view spelling models `traversal_algorithm`, and the
lvalue/rvalue constructibility answers are asserted. CTAD users are unaffected throughout.

---

## Addendum 2 (2026-07-30, post-implementation): the guard's *position* is part of the contract

Ratified after the refactor that renamed Proposal 3's `storable_as` concepts to their final
spellings — `graph_for<G, Graph>`, `mapping_for<M, Map>`, `undirected_graph_for<UG, UGraph>`, each
stating exactly "wraps through `graph_all`/`mapping_all` into the member". The rename is cosmetic.
What is not cosmetic is where the constraints are allowed to sit.

**The rule.** `detail::not_self` must be the **first conjunct of the trailing `requires`-clause**.
No `*_for` helper may ride on a template parameter:

```cpp
// Wrong -- graph_for is checked first, whatever the trailing clause says.
template <graph_for<Graph> G, mapping_for<VertexFilter> VF = maps::true_map>
    requires detail::not_self<G, subgraph_view>

// Right.
template <typename G, typename VF = maps::true_map>
    requires detail::not_self<G, subgraph_view> && graph_for<G, Graph> &&
             mapping_for<VF, VertexFilter>
```

**Why.** A constrained template parameter's constraint is conjoined *ahead of* the trailing
`requires`-clause ([temp.constr.decl]), so the sugar silently reverses the intended order. For a
class that is itself a `graph_view` and whose constructor is callable with one argument — every
view adaptor, since the filters default — the constructor template competes with the copy
constructor, so `G` deduces to the class itself. Then `graph_for<G, Graph>` asks
`constructible_from<Graph, graph_all_t<G>>`, whose `pass_through` branch asks
`constructible_from<subgraph_view, subgraph_view>`: the very question under evaluation. This is not
an unsatisfied constraint but a **hard error** — GCC's "satisfaction of atomic constraint depends on
itself" — at the point of use, the same outside-the-immediate-context failure mode Proposal 3
exists to eliminate. `not_self` was written precisely to cut the recursion off before it starts, and
only the trailing position lets it.

The bug was live in `subgraph_view`: every algorithm holding one failed to compile the moment
anything asked `copy_constructible<Graph>`. It was latent in
BFS/DFS/SCC/toposort/`connected_components`/`traversal_forest`/`bentley_ottmann`, which are
single-argument-constructible but not themselves graphs, so `graph_for` short-circuited to a clean
`false` before reaching `pass_through`. All are fixed; the latent ones defensively.

**Where the sugar stays legal.** Two-argument constructors (`dijkstra`, `dinitz`, `edmonds_karp`,
`kruskal`, the paired Dijkstras, `network_voronoi`) never compete with a copy constructor, so the
class type is never deduced into `G` and no `not_self` is needed — `template <graph_for<Graph> G,
mapping_for<LengthMap> LM>` is correct there. Same for `set_*_length_map` setters, and for
constraints that cannot re-enter constructibility at all (`static_map`'s range concepts).

Pinned in `test/api_consistency.cpp` §5: `copy_constructible` over a `subgraph_view` terminates for
the view, the constructibility queries terminate for the algorithms that hold one, and composing a
subgraph of a subgraph still works (`not_self` is deliberately narrow — a *different*
specialization remains a legal argument). Some of those pins assert `false`, deliberately: every
algorithm is non-copyable after Addendum 3, and view composition passes the view through by value
rather than ref-viewing it. The point of the section is that these queries now *have* answers.

> Addendum 3 narrows what §5 can even ask of an algorithm: the query that used to detonate was an
> algorithm's copy constructor, constrained on `copy_constructible<Graph>`, which re-entered
> `subgraph_view`'s own constructibility. With copy deleted, the guard is reached through the
> constructor template's `graph_for` instead, so the section now pins `is_constructible_v<A, Sub&>`
> alongside the movability. `copy_constructible<Sub>` — the view itself, where the recursion
> actually lived — is unchanged and still the load-bearing pin.

---

## Addendum 3 (2026-07-30, post-implementation): algorithms are move-only

Ratified after Proposals 1-3 landed, on the strength of measurement rather than principle. Copy is
removed from every algorithm: `std::copyable<A>` is now `false` for every algorithm `A` over every
graph, `std::movable<A>` is `true` for every one of them, and the `traversal_algorithm` concept's
existing `std::movable` requirement becomes the *whole* relocation contract.

**Why.** Three findings, in increasing order of weight.

1. **Nothing used it.** No example, benchmark, documented use case or test used a copy for a
   purpose — every test that copied an algorithm was testing copying. The single library-internal
   copy was `traversal_forest` copying its own `_bfs`, inside `traversal_forest`'s own copy
   constructor: circular, existing only so that a `traversal_forest` could itself be copied, which
   nothing needed either. The one plausible use case, forking a branch-and-bound search, is one the
   library had already designed away from — `knapsack_bnb` is deliberately iterative.

2. **It was never cheap.** An algorithm carries every vertex map, the heap and the cached cursors.
   Copying is O(V+E), and the syntax that triggers it — passing by value, `auto x = alg.run();` —
   suggests otherwise. Deleting it turns a silent cost into a compile error. The library had
   already reached this conclusion once from the other side: `algorithmic_generator.hpp` records
   that deriving from `view_interface` made a copyable algorithm model `std::ranges::view`, so
   `alg | std::views::take(3)` deep-copied the whole search state and ran on the copy.

3. **The rule could not be stated — the decisive one.** Copyability was conditional, and the
   condition was not something a user could hold in their head. Measured on the tree as of this
   addendum, over one identical graph type (`subgraph_view<graph_ref_view<static_digraph>>`),
   `dijkstra`, `breadth_first_search` and `topological_sort` were copyable while
   `depth_first_search` and `strongly_connected_components` were not. Worse, the *same* algorithm
   over the *same* view shape flipped on the container underneath: `depth_first_search` over a
   subgraph of a `static_digraph` was not copyable, over a subgraph of a `mutable_digraph` it was —
   because `static_digraph` hands out `std::span` (a `borrowed_range`, so the cursor has no
   `_consumed` counter to reseek with) and `mutable_digraph` hands out intrusive views. A
   capability whose availability rule depends on a two-level interaction between the container's
   range flavour and the view stack above it is worse than no capability, and it made every generic
   `std::copyable<A>` query a research problem.

**What this does *not* remove.** Roughly half of the relocation machinery, and the harder half.
The `_consumed` / `_reseek` / `rebase` / `(range, consumed)` layer in `detail/consumable_view.hpp`
exists for **move**: the ASan finding that motivated Proposal 2 (REVIEW.md Tier 1, finding 2) is a
move defect, reproducible "with no copy anywhere in the program". The hand-written moves in
`depth_first_search`, `dinitz`, `strongly_connected_components`, `connected_components` and
`traversal_forest` all stay, `rebase()` is still called from all four of DFS/dinitz/SCC's special
members, and `frames_need_rebase` still feeds the move `noexcept` specifications. What disappears
is the copy-only half: the hand-written copies, the `requires`-clauses that kept `std::copyable`
honest, and the four rebase helpers that only copy paths ever called — BFS's `_rebase_cursors_from`,
both knapsacks' `_rebase_best_sol_from`, `connected_components`' `_rebase_cursors` and
`traversal_forest`'s `_rebase_sources`, each of which existed because a member pointed into another
member's heap buffer, a problem a move does not have.

**Net effect.** The 17 algorithm headers go from 5,162 to 4,759 lines — 403 net, after adding back
the comments that record the ruling. `detail/movable_box.hpp` and `detail/consumable_view.hpp` keep
their copy support: they serve the graph and mapping *views*, which stay copyable.

**One defect closed on the way.** `knapsack_bnb` and `unbounded_knapsack_bnb` were the last
hand-written copies in the library without the `requires`-clause of REVIEW.md finding 11 — for a
move-only `ItemRange`, `std::copyable` answered `true` and the copy hard-errored inside the
mem-initializer (verified). That is the fourth instance of the same defect found in a family that
had already been swept for it twice, which is the argument for closing the class rather than the
instance.

**Observable API break.** Real, and the largest of the four rulings in this document: any code that
copies an algorithm stops compiling. Migration is to construct a second algorithm, or to `reset()`
one — which reuses the state it has already allocated, and is what `reset()` is for. The one shape
that changes silently-looking code is `auto a = alg.run();`, since `run()` returns `*this` by
reference; it becomes `alg.run();` with the answer read through the accessors. Acceptable at pre-1.0,
and recorded in CHANGELOG.md under 1.0.0 breaking changes.

**Enforcement.** `test/api_review.cpp`'s value-semantics tests now relocate by move and still
destroy the source first; the relocation test moves twice mid-run (construction and assignment) and
compares against an undisturbed traversal. The six "copying a mutable lvalue must pick the copy
constructor" regressions become `!std::is_constructible_v<A, A &>` pins — the property `not_self`
actually guards, which without it would let the greedy single-argument constructor become viable
again where the deleted copy constructor should be chosen. Every algorithm test that pinned
`std::copyable` now pins `std::movable && !std::copyable`. 340/340 pass, including under
AddressSanitizer.
