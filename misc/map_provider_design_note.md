# Design note: caller-supplied algorithm maps through factory-enhancing views

Status: shape settled 2026-09-03, entirely additive, not scheduled. First
written 2026-09-02 around an algorithm-level provider parameter; rewritten
2026-09-03 after that shape was compiled against the headers and replaced,
then again the same day to state the projection **co-ownership** rule that
an ASan probe forced (see "Lifetime"). Everything marked *verified* below
was compiled and run with gcc-15 against `caa27db`; the interior
pieces are prototypes, not shipped code.

## Motivation

Two use cases, one mechanism:

1. **Graphs without map factories.** Today every algorithm requires the
   graph to provide `create_vertex_map` / `create_arc_map`. A caller who
   can supply the working maps themselves should be able to run an
   algorithm on a factory-less graph.
2. **Interior properties, Boost.Graph style.** The "created" map need not
   allocate: it can be a projection into storage that already exists — a
   field of a per-vertex record shared by several of the algorithm's maps,
   a span over a struct-of-arrays column. Zero allocation, better locality,
   state reuse across runs, without the graph concept knowing anything
   about it.

## Why the constraint sits on the class today (unchanged)

The constructor-level placement that 1.0.0-alpha carried looked like it
left this door open, but did not: the class-scope members
(`vertex_map_t<Graph, T> _map;`) and the default-traits heap aliases
force the factories at class-completion time, before any constructor
constraint is consulted — a factory-less graph could not even *name* the
specialization, and `std::constructible_from` probes hard-errored instead
of answering false. The class-level placement states what the
implementation does. Under the shape below it never needs to move: the
enhanced graph *is* a graph with factories, so `has_vertex_map<Graph>`
is simply true for it.

## Settled shape: the provider is a graph view

The provider plugs in **as a view over the graph**, not as an algorithm
parameter. Three adaptors, one concern each, pipe-composable like every
other melon view: `views::with_vertex_maps(g, fs...)`,
`views::with_arc_maps(g, fs...)` and `views::with_edge_maps(g, fs...)`
for undirected graphs. Each is a forwarding view that answers its kind of
map factory from caller-supplied lambdas and forwards everything else.
An algorithm then takes the view like any other graph. Separate adaptors
keep the lambda protocol at a single shape, leave the common vertex-only
case a one-liner, let an arc-only enhancement leave the vertex factories
alone, and match the storage split of stage two (vertex roles share one
record, arc roles another).

**The provider is one or more lambdas with an explicit template parameter
list**, and the protocol has exactly one form — the value type as an
explicit template argument, the role and the graph as arguments:

```cpp
f.template operator()<T>(Role{}, g)
```

The view derives the default-value factory itself: that call, then
`detail::fill` over the graph's vertices (arcs, edges), which fills
through a projection into interior storage as well as through a fresh
container. A graph with no factories then runs every algorithm from a
single lambda:

```cpp
auto vectors = []<typename T>(auto /*role*/, const auto & g) {
    return std::vector<T>(num_vertices(g));
};
for(auto && [v, dist] : dijkstra(views::with_vertex_maps(g, vectors), lengths, s))
    ...
```

**Several lambdas form an overload set.** The view stores
`overloaded<Fs...>` (a `using Fs::operator()...` aggregate) and makes the
call above on it, so ordinary partial ordering dispatches: a lambda that
names a role as its parameter type beats a generic `auto` one, and the
order the lambdas are given in does not matter.

```cpp
auto interior = views::with_vertex_maps(
    g,
    [p = slots.data()]<typename T>(dijkstra_roles::heap_index, const auto &)
        requires std::same_as<T, std::size_t> { return heap_index_field{p}; },
    []<typename T>(auto /*role*/, const auto & g) { return std::vector<T>(num_vertices(g)); });
```

Rules of the protocol, all *verified*:

- The view detects a served request with a requires-expression
  (`f.template operator()<T>(Role{}, g)`), never `std::invocable`, which
  cannot express an explicit template argument.
- The view **enhances**: a request no lambda serves falls through to the
  base graph's own factory when it has one, so a view carrying only
  role-specific lambdas over `static_digraph` interns those roles and
  leaves every other map to the container.
- An ambiguous set (two equally generic lambdas) makes the
  requires-expression false, `has_vertex_map` false, and the algorithm's
  class constraint refuses the graph.
- A lambda with only a default-value form
  (`<typename T>(auto, const auto & g, const T & d)`) serves nothing: the
  protocol has one form. A generic lambda **without** an explicit
  `<typename T>` is rejected too — the explicit argument would land on the
  invented parameter for `role`.
- A role-catching lambda's captures must be the stable slot buffer
  pointer, per the lifetime rule below.
- The lambdas are stored `[[no_unique_address]]`; captureless ones cost
  nothing. (Every stacked melon view does carry 8 bytes over its base for
  an unrelated reason — the shared empty `graph_view_base` cannot overlap
  between a view and the view it stores.)

**Why one form, recorded because the two-form variant failed silently.**
With a bare form and a default-value form served by the same overload
set (a generic lambda taking `const auto &... d`, a role-specific lambda
taking only the bare form), the default-value request is viable only for
the generic lambda, overload resolution hands it there, and the interned
slot is silently bypassed — the probe's record stayed at its old value.
Overload resolution cannot see that the two calls are one request. The
single form removes the second overload set; its price is one extra pass
over a freshly created map, invisible next to a run. If native fills are
ever wanted, the variant that avoids the bypass is first-match ownership
over a tuple of lambdas, at the cost of order-dependence.

*Verified:* a ~50-line view deriving from
`detail::graph_forwarding_interface` and name-hiding the factories ran
`dijkstra`, `breadth_first_search` and `depth_first_search` (mid-run move
included) on a factory-less graph, stacked under `views::subgraph`, over
an lvalue and an owning rvalue base — with **zero algorithm edits**.
`dijkstra_default_traits<view, int>::heap` picked up the provider's
`std::vector<std::size_t>` as its index map by itself. The lambda
overload set, the base fall-through and the single-form fill were
verified in a second prototype over the role-aware CPO.

**Rejected: a provider template/constructor parameter on each algorithm**
(the 2026-09-02 shape). It would rewrite 17 algorithm headers, ~113
member and constructor sites and 6 default-traits structs, needed a
`map_if` variant taking the map type directly, and carried three semver
riders. The view shape needs none of it: `vertex_map_t<Graph, T>` already
computes every member type from whatever the graph view answers, and the
forwarding interface already hands factories through. The default
provider is literally the graph.

**Rejected, still: per-map constructor arguments** — the internal map set
varies with the traits, the heap's index map hides inside the traits'
heap type, and the arity would force callers to know melon's internals
positionally.

## Role keys ride on the CPOs as a defaulted template parameter

Requests must be keyed by **role**, not just value type:
`bidirectional_dijkstra` creates *two* `vertex_map<std::size_t>`s, one
per heap, and a provider keyed on `T` alone would hand the same interior
slot to both and silently corrupt the run. Each algorithm names its roles
(`dijkstra_roles::status`, `dijkstra_roles::heap_index`, ...).

The role is a **second, defaulted template parameter of the create-map
CPOs**, mirrored on the aliases and concepts:

```cpp
create_vertex_map<T, Role = std::monostate>(g);        // and (g, d)
create_arc_map<T, Role = std::monostate>(g);           // and the edge twin
vertex_map_t<G, T, Role = std::monostate>;
has_vertex_map<G, T = std::size_t, Role = std::monostate>;
```

Dispatch is two-tier, role-aware form first, legacy form second — member
`create_vertex_map<T, Role>()`, ADL `create_vertex_map<T, Role>(g)`,
then member `create_vertex_map<T>()`, ADL `create_vertex_map<T>(g)`.
Consequences, all *verified*:

- Every existing container answers **any** role with its standard map,
  whether its factories are members (`static_digraph`) or ADL free
  functions. No user graph changes.
- A role-aware type declares only the two-parameter form with a
  defaulted `Role`; it catches the roles it cares about with
  `if constexpr` and forwards the rest through the CPO to its base. The
  CPO needs no special case for the default role.
- A role never narrows satisfiability: `has_vertex_map<G, T, Role>`
  holds whenever `has_vertex_map<G, T>` does, so no algorithm constraint
  changes.
- Putting the role in the template argument list, not a function
  argument, avoids any ambiguity with the default-value overload.
- `detail::vertex_map_if`'s fourth parameter — today a discriminator
  keeping two disabled maps distinct empty types — becomes the role and is
  forwarded to the factory. `bidirectional_dijkstra`'s two local tags are
  its two heap-index roles.

`std::monostate` is the proposed default; a melon-owned empty tag would
read better in diagnostics and spare `graph.hpp` the `<variant>` include.
Either is fine; decide when landing.

**Hard obligation — every forwarding layer must carry the role.** A
roleless forwarding member silently drops it (*verified*), and
`graph_ref_view` / `graph_owning_view` are such layers today
(`detail::graph_forwarding_interface`'s factories take one template
parameter). Since `views::graph_all` wraps *every* algorithm's graph in one
of them, no algorithm could reach a role-aware view's interior maps until
the forwarding interface, `undirect_view` and `undirected_graph_view`
forward a defaulted `Role`. This is the first thing to land.

**Cost to accept knowingly:** naming roles makes an algorithm's internal
map set quasi-public — retiring a map becomes observable to
role-specialized providers. Document roles as extension points with
weaker stability guarantees than the main API.

## Traits keep naming a complete heap type

The default traits spell the heap through the role-aware alias, and
nothing else changes — the unary traits concepts, the documented traits
tables and every custom traits struct written today stay valid:

```cpp
template <has_vertex_map Graph, typename ValueType>
struct dijkstra_default_traits {
    using semiring = shortest_path_semiring<ValueType>;
    using heap = updatable_d_ary_heap<
        2, std::pair<vertex_t<Graph>, ValueType>, typename semiring::less_t,
        vertex_map_t<Graph, std::size_t, dijkstra_roles::heap_index>,
        maps::element<1>, maps::element<0>>;
    ...
};
```

*Verified* over a role-aware view: the heap is built directly on the
interior projection. (An alias-template `heap<IndexMap>` with a binary
traits concept was considered and dropped: not needed once the alias
carries the role.)

**Residue, and its guard.** A custom traits struct that hard-codes the
container — the `reliability_traits` example in
`docs/algorithms/shortest-paths.md` spells
`vertex_map_t<static_digraph, std::size_t>` — mismatches once the graph is
wrapped in a type-changing view. Today the hard-coding is harmless because
every view forwards the same map type. *Verified:* for a field projection
the mismatch is a hard error in the heap's mem-initializer; for a
span-shaped projection it **compiles and the heap writes to a private
copy**, because `static_map`'s range constructor accepts any random-access
range. Two small measures close it:

- `updatable_d_ary_heap` gains `using index_map_type = IndicesMap;`, and
  each heap-carrying algorithm asserts, next to its existing entry-type
  `static_assert`, that `heap::index_map_type` equals the map it creates —
  gated on the alias existing, so custom heaps stay free.
- The docs example is rewritten generically (inherit the default traits
  and override the semiring, or template over the graph), and the traits
  section says the index map is the graph's answer for the `heap_index`
  role.

## Lifetime: projections must reference *and* keep alive their storage

A projection owns nothing by itself, so an interior provider moves the
ownership of every map it serves out of the algorithm and into the view.
Two separate rules follow, and each was found by a failing probe.

**Rule 1, for moves: point at the slot buffer, never at the object.**
Algorithms are move-only and moving one moves the stored graph, view
included. A projection holding a pointer **to the view or graph object**
dangles on that move; one holding the address of the heap-allocated slot
buffer does not, because a move does not relocate that buffer. A record
field projection is then a plain
`{record_map * m; T & operator[](vertex) const;}` and no rebase machinery
is needed. *Verified:* moving an interleaving view leaves projections
taken before the move reading the same storage.

**Rule 2, for extraction: co-own the slot buffer.** Rule 1 alone silently
breaks a published contract. `docs/views/ownership.md` promises that an
expiring algorithm *moves its stored map out* so "the result outlives the
machinery that computed it", and that promise is inside the API-stability
scope. With a borrowing projection there is nothing to move: extraction
hands back a `mapping_owning_view` wrapping a pointer into storage the
view owns. *Verified under ASan:* `std::move(alg).dists_map()` over a view
handing out raw-pointer projections compiles, and reading the extracted
map after the view dies is a heap-use-after-free through
`mapping_owning_view::operator[]` → `detail::mapping_subscript` → the
projection. The failure is silent until someone reads the result.

So a projection holds a **shared pointer** to the slot buffer rather than
a raw one, and the extracted map keeps the storage alive by itself.
*Verified:* same probe, same extraction, view destroyed first, correct
distances read back, buffer freed exactly once when the last of the view
and the extracted maps dies. The cost is 8 extra bytes per map member and
one atomic increment per map *creation* — never per access, and creation
happens once per algorithm.

*Rejected alternative:* forbid interior roles for the maps that back
extractable results (distances, flows, potentials, reached) and allow them
only for internal state (status, heap index). It preserves the contract
without shared pointers, but it splits roles into two classes with
different rules and gives a caller who interns a distance role silence
rather than a diagnostic.

Buffer reallocation (`mutable_digraph::create_vertex`) still invalidates
projections the way it invalidates `std::vector` iterators; document it.

## Constness (unchanged, restated)

Interior slots need mutable access, but algorithms hold the graph
const-ly. The view is constructed by the caller before the algorithm
exists and owns the slot buffer through a smart pointer, from which a
const view can both read the address and copy a new owning handle, so it
hands out mutable, co-owning slots bound up front; the algorithm's const
view of the graph never grants writes to anything.
Two live algorithms sharing one interior view share its slots: an interior
view backs at most one live algorithm per role. Documentation-enforced;
no debug-mode check (the `#ifndef NDEBUG` layout/ODR rule pinned in
dinitz).

## Prerequisites already shipped (2026-09-02)

- `detail::fill` resets maps through per-key writes when no member
  `fill` exists, so a bare projection modeling only `output_mapping` runs
  and resets every algorithm — no `.fill` obligation on providers.
- Factory constraints are honest (`default_initializable` +
  fill-assignability), so `has_vertex_map` answers false rather than
  hard-erroring for unholdable value types.

## Stage two: interior storage lives in a view, not in the graph

Recommended (unruled, measure first): `views::interleave_maps<Roles...>(g)`.

**It is sugar over `with_vertex_maps`.** Stage one already buys interior
storage by hand: declare a record type, allocate an array of it, and hand
`with_vertex_maps` a role-catching lambda that returns a projection into
one field. `interleave_maps` writes exactly that boilerplate from a list
of roles:

```cpp
auto iv = views::interleave_maps<dijkstra_roles::status,
                                 dijkstra_roles::heap_index>(g);
```

The view generates one record type with one field per listed role
(`{char status; std::size_t heap_index;}`), owns one array of it, and
answers `create_vertex_map<char, status>` with a projection reading
`record.status`, `create_vertex_map<std::size_t, heap_index>` with one
reading `record.heap_index`. A projection is a co-owning handle on the
record array plus a compile-time field index, so the algorithm's map
members become projections instead of separately allocated arrays.

**What it buys.** Dijkstra settling a vertex touches its status, its heap
index and its predecessor: three arrays and up to three cache lines per
vertex, versus one record and one line interleaved. *Verified* on a
two-role prototype:

| Measurement | Value |
| --- | --- |
| Record size for status + heap index (`std::tuple<char, std::size_t>`) | 16 bytes |
| Distance between the two fields | 8 bytes, same record |
| Two distinct `std::size_t` roles written independently | stay distinct, no aliasing |

That last row is the collision the roles exist for:
`bidirectional_dijkstra`'s two heap-index maps are both `std::size_t`, and
as two roles they become two fields rather than one shared slot.

**Three properties worth stating.** *(1)* The view does not allocate the
array itself: it calls `create_vertex_map<record>` on the base graph, so
the record array is whatever the graph's own map is — which is what makes
it work over id holes, non-integral ids and a custom allocator. *(2)* A
role no listed role matches falls through to the base graph's own factory,
so a view over `static_digraph` interns the two or three maps that matter
and leaves the rest as `static_map`s (*verified* by static assertion).
*(3)* Projections co-own their storage per Rule 2 above, so extraction
from an expiring algorithm still outlives it.

**Caveats found while prototyping, to settle when it is picked up.**

- **Roles must declare a `value_type`** for the view to derive the record,
  since the caller names roles only
  (`struct heap_index { using value_type = std::size_t; };`, and
  distance-like roles are templates over the length type). Stage one does
  not need that — its lambdas receive `T` explicitly — so either every
  role declares it or `interleave_maps` takes role/type pairs.
- **Generate a struct, not a `std::tuple`.** Libstdc++ lays tuple members
  out in reverse and the prototype's two fields cost 7 bytes of padding.
  Order the generated fields by alignment or part of the locality win is
  given straight back.
- **The role pack must precede the deduced graph parameter**
  (`template <typename... Roles, typename G>`), or an explicit role list
  binds the graph parameter. The pipe-composable spelling is an adaptor
  object parameterized on the roles.
- **It pays off only after stage one.** Roles have to reach the
  algorithms first; today an algorithm over such a view compiles and runs
  correctly but takes the roleless path and uses the base graph's maps
  (*verified*).
- **A projection is not a `contiguous_mapping`.** That costs nothing in
  the current algorithms: the prefetch calls target arc-keyed maps
  (`arc_targets_map`, the length map), not vertex-keyed ones.

Why not the 2026-09-02 graph-embedded byte arena:

- **The arena's unique payoff is bounded and unmeasured.** Per-arc work in
  both shipped containers never touches vertex-keyed graph storage:
  `static_digraph::out_arcs` reads `_out_arc_begin` once per pop,
  `mutable_digraph::out_arcs` reads `_vertices[v]` once per pop, and the
  targets and lengths are arc-keyed. Dijkstra's per-arc misses come from
  the vertex-keyed *maps* — status, heap index, predecessor. A view-owned
  record collapses those to one line and captures the per-arc win; the
  arena adds only a once-per-vertex saving on top. If that residue ever
  matters, benchmark it before templatizing a container.
- **A typed record admits everything.** `std::optional<arc>` predecessor
  maps, dinitz's cursor maps and biobjective's label sets are not
  implicit-lifetime types and could never live in a byte arena; they
  interleave in a typed record without restriction. No
  `std::start_lifetime_as`, no alignment/capacity `static_assert`s.
- **No graph API change.** No container template parameter, no
  `basic_*` + alias rename, no `create_map_provider` CPO, no per-graph
  single-arena resource — exclusivity shrinks to ordinary ownership of a
  view instance.

If a byte-carving variant is ever wanted for caller-side ignorance of value
types, it can be the same view carving a per-vertex byte record from
role-declared budgets; the arena decision points on implicit-lifetime
restriction and compile-time-honest capacity then apply to that view, not
to the containers. Rejected sibling, unchanged: bundled property *types* on
the graph.

## Semantic versioning: everything is additive

- Defaulted template parameters appended to the CPO variable templates,
  the aliases and the concepts; a defaulted `Role` on the forwarding
  views' factory members; new view classes and adaptor objects; a new
  `index_map_type` alias and `static_assert`s that only fire on code that
  is already wrong. No algorithm signature, no traits protocol, no
  container changes. The v1.0.0 retag needs nothing from this note.
- The API-stability line reserving appended defaulted template parameters
  and forbidding user forward declarations of melon types is no longer a
  prerequisite; still worth adding as general hygiene.

## First steps when picked up

1. `Role` on the create-map CPOs, aliases and concepts; forwarded through
   `graph_forwarding_interface`, `undirect_view`, `undirected_graph_view`;
   `vertex_map_if`'s fourth parameter becomes the role; roles named in the
   six heap-carrying algorithms' default traits; `index_map_type` guard;
   docs example made generic. Pin with `static_assert`s that a legacy
   container answers a role with its standard map and that a role survives
   `graph_ref_view` and `views::subgraph`.
2. `views::with_vertex_maps` / `with_arc_maps` / `with_edge_maps` against
   `dijkstra`, `dinitz` and `bidirectional_dijkstra` (the same-value-type
   collision case), including the DFS mid-run move; pin the single-form
   fill through a projection and the ambiguous-set rejection.
3. Pin the extraction contract before any interior view ships: extract
   `dists_map()` / `flows_map()` from an expiring algorithm over a
   projection-serving view, destroy the view, read the map — under ASan,
   which is what caught the borrowing version.
4. `views::interleave_maps` over the same algorithms, then measure against
   the status + heap-index baseline before naming any container-level work.

## Traps met while probing this design

- `detail::not_self` must be the first conjunct of a view's
  single-argument constructor constraint, or wrapping the view in another
  view recurses through `views::graph_all` ("satisfaction of atomic
  constraint depends on itself"). Already pinned for the shipped views;
  it bit the prototypes immediately.
- The create-map CPO structs stay outside `namespace melon`
  (`melon_create_map_cpo`), with the public names as variable templates in
  `melon::cust` — the MSVC and ADL-self-dependency rulings in `graph.hpp`
  apply unchanged to the two-parameter form.
- A borrowing projection makes every ownership failure *silent*: the map
  still models `output_mapping`, extraction still compiles, and
  `mapping_owning_view` still wraps it — only a read after the view's
  death shows the bug, and only under a sanitizer. Any prototype in this
  space should be probed with `-fsanitize=address` from the start.
- Every stacked melon view is 8 bytes larger than the view it stores,
  because the shared empty `graph_view_base` cannot overlap between the
  two. It is pre-existing and unrelated to providers, but it is what a
  `sizeof` check on a prototype view will show first; do not chase it as
  a `[[no_unique_address]]` failure of the stored lambdas.
