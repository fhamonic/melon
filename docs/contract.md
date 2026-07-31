# The 1.0 contract

melon 1.0 is a frozen API: every header outside `melon/detail/` and
`melon/experimental/` is stable for the whole 1.x series. This chapter states
the **rulings** that stability rests on — the deliberate design decisions every
piece of user code has to follow. They are not conventions that happen to hold
today: each one is pinned by tests (`test/api_consistency.cpp`,
`test/api_review.cpp`) and will not be relitigated within 1.x. If your code
follows them, it keeps compiling and keeps meaning the same thing for every
1.x release.

Each ruling below says what it requires from you, and what happens if you
ignore it.

## Ruling 1 — Algorithms are move-only

`std::copyable<A>` is `false` for **every** algorithm object over **every**
graph; `std::movable<A>` is `true` for every one of them.

**What you must do.** Never pass an algorithm by value and never write
`auto a = alg.run();` — `run()` returns `*this` by reference, so that line
would be a copy, and it does not compile. Write:

```cpp
auto alg = dijkstra(g, length_map);   // construct (CTAD)
alg.add_source(s);
alg.run();                            // drains; returns alg itself
auto d = alg.dist(t);                 // results through accessors
```

To restart a computation, do not construct a copy — call `reset()`, which
reuses the maps and the heap the object has already allocated and restores
exactly the state the constructor left behind. To run the same problem twice
concurrently, construct two algorithms.

**Why it is a ruling and not a limitation.** Copying an algorithm was O(V+E)
— every vertex map, the heap, the cached cursors — behind syntax that
suggested a cheap handle. And its availability could not be stated: before
1.0, over one identical graph type, `dijkstra` was copyable while
`depth_first_search` was not, and `depth_first_search` flipped its answer
depending on the container underneath. A capability whose rule you cannot
hold in your head is worse than no capability, so it was removed.

**Moving is fully supported, mid-run included.** An algorithm that caches
incidence cursors re-aims every cursor at its new graph member when moved, so
relocation is sound even for an algorithm over an owned, filtered subgraph.
One caveat: maps handed out by an *lvalue* algorithm (`dists_map()`,
`reached_map()`, …) reference the algorithm object — moving the algorithm
invalidates previously handed-out maps, the same contract `std::ranges`
adaptors have over a moved container. To keep a result map beyond the
algorithm, extract it from an *expiring* one — `std::move(alg).flows_map()`
moves the stored map out into an owning view; extraction is terminal. See
[Ownership](views/ownership.md#getting-a-result-map-out-the-s_map-accessors).

## Ruling 2 — One lifecycle, spelled as a concept

The algorithm-object lifecycle is a named contract,
`melon::traversal_algorithm` (and `rooted_traversal_algorithm` for the
`add_source` half), defined in `melon/utility/algorithmic_generator.hpp` and
statically asserted for every algorithm in the library:

- **`reset()` restores exactly the state the constructor leaves behind** —
  blank for an algorithm whose sources are added afterwards, re-seeded and
  immediately runnable for one whose constructor seeds (`topological_sort`,
  `traversal_forest`). `alg.reset()` is always equivalent to constructing a
  fresh object from the same arguments, minus the allocations.
- **`run()` drains and returns `*this`.** It is idempotent: `finished()`
  holds afterwards and a second call is a no-op. Results stay readable
  through the accessors. This holds for every algorithm, including
  `bidirectional_dijkstra`, whose answer is read as `alg.run().dist()`.
- **`current()` and `advance()` require `!finished()`** — asserted in debug
  builds, undefined in release builds.
- **`add_source(s)` requires that `s` is untouched** — not reached, not in
  the heap. Re-adding a settled vertex would silently corrupt stored paths
  and distances, so the strict precondition is asserted family-wide.
- **There is no post-construction, pre-iteration step.** A constructed (and,
  for rooted algorithms, sourced) object is ready to iterate. The old
  `competing_dijkstras::init()` is gone; nothing replaces it.

Traits are checked by concepts (`dijkstra_traits`,
`breadth_first_search_traits`, …, all plural), so a misspelled traits flag
fails the constraint instead of silently defaulting. The path-storing flag is
`store_paths`, plural, everywhere.

**Accessor naming is part of the contract**: traits-gated results are
`dist(v)` / `dists_map()`, `pred_arc(v)` / `pred_arcs_map()`, `path_to(v)`,
`reached(v)` / `reached_map()`. A per-key accessor's map view pluralises the
noun and appends `_map` — the `component_ids_map()` precedent: `flow(a)` /
`flows_map()`, `depth(v)` / `depths_map()`, `cluster(v)` / `clusters_map()`.
If a ninth algorithm ever drifts from any of this, that is a bug — report
it.

## Ruling 3 — Algorithms and views store *views*: lvalues are referenced, rvalues are owned

Every algorithm and every graph adaptor routes its arguments through
`views::graph_all` / `maps::mapping_all` — the melon analogues of
`std::views::all`:

- **Pass an lvalue** graph or map and the object stores a *reference view* of
  it. You must keep the graph and the maps alive for as long as the algorithm
  lives, and mutations you make through the original are visible to it.
- **Pass an rvalue** and the object *owns* it (`graph_owning_view` /
  `mapping_owning_view` — the same bytes a raw member would have).

```cpp
auto alg1 = dijkstra(g, lengths);             // references g and lengths
auto alg2 = dijkstra(std::move(g), lengths);  // owns the graph, references the map
```

**What you must not write.** Explicitly spelling a raw container as a member
type — `dijkstra<static_digraph, static_map<unsigned, int>>` — is
**ill-formed**: class heads require `graph_view` / `mapping_view` members
(the `std::ranges::transform_view` precedent). If you need to name the type,
either use `decltype` on the CTAD spelling, or spell ownership explicitly:
`dijkstra<graph_owning_view<static_digraph>, mapping_owning_view<…>>`. CTAD
always produces exactly the type you could have spelled.

Constructor constraints tell the truth in both directions: the concepts
`graph_for<G, Graph>`, `undirected_graph_for<UG, UGraph>` and
`mapping_for<M, Map>` state exactly "wraps through `graph_all` /
`mapping_all` into the member", so `std::is_constructible` never answers
`true` for a construction that would hard-error.

## Ruling 4 — A mapping is readable through const access

The `mapping<M, K>` concept requires that `m[k]` works on a **const** map.
This is deliberate: an algorithm reads your length map through const access,
so a map whose subscript cannot be offered const is not a mapping.

The canonical casualty is `std::map`: its `operator[]` inserts, so
`mapping<std::map<K, V>, K>` is **false** — by design, not by accident. To
use one, go through the wrapping layer, which reads via `at()` **on a const
base**:

```cpp
const std::map<vertex, double> lengths = …;
auto alg = dijkstra(g, maps::mapping_all(lengths));  // reads through at()
```

Mind the `const`: wrapping a *non-const* `std::map` lvalue hands the view a
non-const reference, and the subscript dispatch (`m[k]` → `m(k)` → `m.at(k)`)
then finds the inserting `operator[]` first — a lookup of a missing key
default-inserts instead of throwing. Pass the map through `std::as_const`
unless insertion is what you want.

Anything subscriptable-const works directly: `static_map`, `std::vector`
(keyed by an integral handle), a lambda through `maps::map`, a
`mapping_ref_view`. Write access is a separate concept, `output_mapping`; a
mapping stored by view is `mapping_view`. There is no `input_mapping` layer
anymore — `mapping` *is* the readable concept.

## Ruling 5 — Direct call and pipe differ for lvalue filters, on purpose

`views::subgraph(g, filter)` and `g | views::subgraph(filter)` are the same
view type but treat an **lvalue** filter map differently:

- the **direct call** follows Ruling 3: an lvalue filter is stored by
  *reference*, so `disable_vertex` / `enable_vertex` on the view write into
  your map, and your later writes into the map are visible to the view;
- the **piped closure** is self-contained like std's: `views::subgraph(vf)`
  *copies* the filter into the closure, and each application copies (from an
  rvalue closure, moves) it into the view — a closure is reusable and never
  dangles, and the view's filter is independent of your map.

Both semantics are spellable in both forms; only the lvalue *default*
differs:

```cpp
// view references your map:
auto s1 = views::subgraph(g, vf);                             // direct default
auto s2 = g | views::subgraph(mapping_ref_view(vf));          // pipe override

// view owns a copy:
auto s3 = views::subgraph(g, auto(vf));                       // direct override
auto s4 = g | views::subgraph(vf);                            // pipe default
```

The same rule applies to `views::induced_subgraph`'s vertex range. See
[Graph views — pipe syntax](views/graphs.md) for the worked examples.

## Ruling 6 — Algorithms are single-pass ranges, not `std::ranges` views

Every lazy algorithm is a `std::ranges::input_range` — you can range-`for`
it, `take_while` it, early-exit it — but it is deliberately **not** a
`std::ranges::view` (`std::ranges::enable_view` is `false`):

- iteration **consumes**: `begin()` is cheap and idempotent, but each
  `++it` is an `advance()` on the algorithm itself. After a full pass,
  `finished()` holds; call `reset()` to go again;
- piping an lvalue algorithm into a standard adaptor wraps a `ref_view`
  around it — `alg | std::views::take(3)` advances *your* object, it does
  not copy the search state (before 1.0 it deep-copied the whole algorithm);
- do not require `forward_range` of an algorithm, and do not expect a second
  iteration without `reset()`.

## Ruling 7 — Handles, names and namespaces

- **Vertex and arc handles are spelled `vertex_t<G>` and `arc_t<G>`.** The
  member typedefs are private on every graph type; the alias templates are
  the supported spelling and work for every graph, view and user type.
- **Graph views follow the class/adaptor split**: the *types* are
  `melon::reverse_view<G>`, `melon::subgraph_view<…>`,
  `melon::induced_subgraph_view<…>`, `melon::undirect_view<G>`; the *adaptor
  objects* live in `melon::views` and support pipe syntax. Naming a view
  type means naming the `…_view` class.
- **`melon::maps`** holds the mapping views and factories (`maps::map`,
  `maps::mapping_all`, `maps::true_map`, `maps::identity_map`, …);
  `melon::views` holds graph views only. `mapping_ref_view`,
  `mapping_owning_view` and the mapping concepts live in `melon::` itself.
- **`melon::numeric`** holds `rational`, `make_rational`, `bounded_value`
  and `const_value`, under `melon/numeric/`.
- **`melon::experimental`** carries no stability guarantee at all.

## Ruling 8 — Preconditions are asserted, not checked

melon does not throw on contract violations. Every stated precondition —
`add_source` on an untouched vertex, `current()`/`advance()` on an
unfinished algorithm, valid handles into `create_arc`, a fitting vertex
count — is an `assert` in debug builds and undefined behaviour in release
builds. Build and test with assertions (and, ideally,
`-DMELON_SANITIZE=address,undefined`) before shipping with `NDEBUG`.

The exceptions that *are* thrown are the STL-shaped ones: `at()` on the map
containers throws `std::out_of_range`, allocation failures propagate
`std::bad_alloc`. `noexcept` specifications are honest — a member is
`noexcept` exactly when it cannot throw, conditional specifications measure
the operations they forward — so you may rely on them.

## Ruling 9 — Borrowedness is an opt-in promise you make

`melon::enable_borrowed_graph<G>` mirrors
`std::ranges::enable_borrowed_range`: specialise it `true` for your own
graph type only if the ranges it hands out remain valid **independently of
the graph object** (a non-owning view over external storage, for example).
Algorithms use it to skip the cursor-rebase work on moves. Claiming it
falsely turns every algorithm move over your graph into a use-after-free;
not claiming it merely costs the rebase loop. When unsure, leave it false.

## Ruling 10 — The scope of the guarantee

Semantic versioning covers every documented name in `namespace melon`,
`melon::views`, `melon::maps`, `melon::numeric` and `melon::cpo`, plus the
behavioural contracts on this page. It does **not** cover:

- `melon/detail/` and anything in a `detail` namespace;
- `melon/experimental/` and `namespace melon::experimental`;
- the exact type of members documented only by concept (e.g. the concrete
  range type an algorithm's iteration yields, beyond what
  `traversal_algorithm` promises) — spell such types with `auto` or
  `decltype`.

If a 1.x release ever breaks code that follows the rulings on this page,
that is a bug in melon.
