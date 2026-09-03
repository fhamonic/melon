# Design note: caller-supplied algorithm maps through factory-enhancing views

Status: shape settled 2026-09-03, entirely additive, not scheduled. First
written 2026-09-02 around an algorithm-level provider parameter; rewritten
2026-09-03 after that shape was compiled against the headers and replaced.
Everything marked *verified* below was compiled and run with gcc-15 against
HEAD `caa27db`.

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
parameter. `views::with_maps(g, provider)` (name open) is a forwarding
view that answers the map factories from a caller-supplied provider and
forwards everything else. An algorithm then takes it like any other graph:

```cpp
struct vector_provider {
    template <typename T, typename G>
    auto create_vertex_map(const G & g) const {
        return std::vector<T>(num_vertices(g));
    }
    template <typename T, typename G>
    auto create_vertex_map(const G & g, const T & d) const {
        return std::vector<T>(num_vertices(g), d);
    }
};

for(auto && [v, d] : dijkstra(views::with_maps(g, vector_provider{}), lengths, s))
    ...
```

*Verified:* a ~50-line view deriving from
`detail::graph_forwarding_interface` and name-hiding the four factories
ran `dijkstra`, `breadth_first_search` and `depth_first_search` (mid-run
move included) on a factory-less graph, stacked under `views::subgraph`,
over an lvalue and an owning rvalue base — with **zero algorithm edits**.
`dijkstra_default_traits<view, int>::heap` picked up the provider's
`std::vector<std::size_t>` as its index map by itself.

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

## Lifetime rule for projection maps (unchanged, restated)

Algorithms are move-only and moving one moves the stored graph, view
included. A projection holding a pointer **to the view or graph object**
dangles on that move. Projections must reference the **stable slot
buffer** — heap storage the view or graph owns, which a move does not
relocate — never the object. A record field projection is then a plain
`{record * p; T & operator[](vertex) const;}` and no rebase machinery is
needed. Buffer reallocation (`mutable_digraph::create_vertex`) invalidates
projections the way it invalidates `std::vector` iterators; document it.

## Constness (unchanged, restated)

Interior slots need mutable access, but algorithms hold the graph
const-ly. The view is constructed by the caller before the algorithm
exists and owns the record buffer through a smart pointer whose `get()`
is const, so a const view hands out mutable slots bound up front; the
algorithm's const view of the graph never grants writes to anything.
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

Recommended (unruled, measure first): `views::interleave_maps<Roles...>(g)`
— a view that owns **one** record array sized through the base's own
factory (so it works on every graph with factories, holes and non-integral
ids included) and answers each listed role with a field projection,
falling back to the base for every other role. Roles carry their value
type (`struct heap_index { using value_type = std::size_t; };`;
distance-like roles are templates over the length type) so the caller
names roles only.

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
2. `views::with_maps` against `dijkstra` and `bidirectional_dijkstra`
   (the same-value-type collision case), including the DFS mid-run move.
3. `views::interleave_maps` over the same two, then measure against the
   status + heap-index baseline before naming any container-level work.

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
