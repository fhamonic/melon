# Design note: caller-supplied algorithm maps through a map provider

Status: direction settled, not scheduled. Written 2026-09-02, after the
pre-1.0.0 concept audit moved `has_vertex_map` / `has_arc_map` from the
algorithms' constructors onto their class-level requires-clauses.

## Motivation

Two use cases, one mechanism:

1. **Graphs without map factories.** Today every algorithm requires the
   graph to provide `create_vertex_map` / `create_arc_map`. A caller who
   can supply the working maps themselves should be able to run an
   algorithm on a factory-less graph.
2. **Interior properties, Boost.Graph style.** The "created" map need not
   allocate: it can be a projection into storage that already lives with
   the graph — a span over a struct-of-arrays column, a member projection
   over a vertex record. Zero allocation, better locality, state reuse
   across runs, without the graph concept knowing anything about it.

## Why the constraint sits on the class today

The constructor-level placement that 1.0.0-alpha carried looked like it
left this door open, but did not: the class-scope members
(`vertex_map_t<Graph, T> _map;`) and the default-traits heap aliases
force the factories at class-completion time, before any constructor
constraint is consulted — a factory-less graph could not even *name* the
specialization, and `std::constructible_from` probes hard-errored instead
of answering false. The class-level placement states what the 1.x
implementation actually does, and relaxing a class constraint later is
the non-breaking direction under semver. This evolution therefore costs
nothing by waiting.

## Settled shape: one provider object, not per-map arguments

Per-map constructor arguments (`dijkstra(g, lengths, status_map, ...)`)
are rejected: the internal map set varies with the traits (flag-gated
`map_if` members), the heap's index map hides inside the traits' heap
type, and the resulting arity explosion would force callers to know
melon's internals positionally.

Instead: one allocator-like **provider** parameter, defaulted to a
graph-backed provider that calls today's factories. Sketch:

```cpp
template <typename P, typename G, typename T, typename Role>
concept vertex_map_provider_for =
    requires(const P & p, const G & g, const T & d) {
        {
            p.template create_vertex_map<T>(g, Role{})
        } -> output_mapping_of<vertex_t<G>, T>;
        {
            p.template create_vertex_map<T>(g, Role{}, d)
        } -> output_mapping_of<vertex_t<G>, T>;
    };

// The default provider: exactly today's behavior and requirements.
template <typename G>
struct graph_map_provider {
    template <typename T, typename Role>
    auto create_vertex_map(const G & g, Role) const {
        return melon::create_vertex_map<T>(g);
    }
    // + default-value form, + arc twin
};
```

`has_vertex_map<Graph>` then migrates from the algorithm classes into
`graph_map_provider`'s constraints; the algorithms constrain on the
provider concept instead. Members stop being `vertex_map_t<Graph, T>`
and become the provider's return types (a `map_if` variant taking the
map type directly is needed).

## Role keys are load-bearing, not decoration

Requests must be keyed by **role**, not just value type. Algorithms
create several maps and some collide on value type —
`bidirectional_dijkstra` creates *two* `vertex_map<std::size_t>`s, one
per heap. A provider keyed only on `T` would hand the same interior slot
to both and silently corrupt the run. Each algorithm names its roles
(`dijkstra_roles::status`, `dijkstra_roles::heap_index`, ...); the
default provider ignores them; interior-storage providers specialize
exactly the roles they care about. This is Boost.Graph's property-tag
insight relocated to the provider boundary, with the graph concept left
untouched.

**Cost to accept knowingly:** naming roles makes an algorithm's internal
map set quasi-public — retiring a map becomes observable to
role-specialized providers. Document roles as extension points with
weaker stability guarantees than the main API.

## Lifetime rule for projection maps

Melon algorithms are move-only and moving one moves the stored graph. A
projection holding a pointer **to the graph object** (member-pointer +
object pointer) dangles on that move — the cursor-rebasing problem
`depth_first_search` / `dinitz` hand-write their moves for. The rule,
following the same distinction the DFS move policy draws for
std-borrowed ranges: **projections must reference the stable slot
buffer** (heap storage the graph owns, which a move does not relocate),
never the graph object itself. Then no rebase machinery is needed.

## Constness

Interior slots need mutable access, but algorithms hold the graph
const-ly. The provider is constructed by the caller before the algorithm
exists, so it binds mutable slot access up front; the algorithm's const
view of the graph never has to grant writes to interior storage. (This
sidesteps the const-graph / non-const-property-map friction Boost.Graph
is known for.)

## Prerequisites already shipped (2026-09-02)

- `detail::fill` resets maps through per-key writes when no member
  `fill` exists, so a bare projection modeling only `output_mapping`
  runs and resets every algorithm — no `.fill` obligation on providers.
- The map-creation constraints live on the algorithm classes and default
  traits, where relaxing them into the default provider is a pure
  widening.
- Factory constraints are honest (`default_initializable` +
  fill-assignability), giving the provider concept a clean probe story
  to mirror.

## Stage two: arena-backed providers from the graph itself

Settled direction (2026-09-02, same discussion): a graph container may be
parameterized with a per-vertex byte count — untyped storage reserved in
each vertex record — and expose a CPO,
`create_map_provider<Roles...>(g)`, returning a provider that carves the
requested roles' mapped values out of that arena and **falls back to the
standard factories for every unspecified role**. The graph still knows
nothing about what roles mean; they are opaque keys. It owns the
*where*, the provider computes the *what*. This is the interior-property
payoff: status bytes and heap indices living on the same cache line as
the vertex's adjacency data.

Five decision points, settled in principle:

1. **Type restriction.** Arena-backed roles are restricted to
   implicit-lifetime types (at minimum trivially copyable + trivially
   destructible), handled through `std::start_lifetime_as` — no
   constructor/destructor bookkeeping in raw bytes. This costs nothing
   real: the hot-loop state worth interring (status enums, heap indices,
   distances) qualifies; `optional<arc>` pred maps do not and take the
   factory fallback, which the design provides anyway. The restriction
   belongs in the CPO's requires-clause so a non-qualifying role is a
   named rejection, not UB.
2. **Alignment and capacity are compile-time-honest.** The arena is
   `alignas(std::max_align_t)` (or the byte count gains an alignment
   companion), and the offset computation ends in a friendly
   static_assert — "roles {status, heap_index} need 12 bytes, this graph
   reserves 8". Silently spilling an interior request to the fallback
   would be worse than failing: a caller who asked for interior storage
   gets it or hears why not.
3. **Exclusivity preconditions, two layers, documentation-enforced.**
   (a) An interior role's slots back at most one live algorithm at a
   time. (b) Offsets are computed from the requested role *pack*, so the
   arena is a single resource: one live interior provider per graph.
   No debug-mode enforcement — a checking member would have to exist
   unconditionally (the `#ifndef NDEBUG` layout/ODR rule pinned in
   dinitz), and roles-to-bits registration is not worth it.
4. **`N = 0` is genuinely zero-cost.** `std::byte[0]` is not a legal
   member, so the arena member gets the flag-gated-member treatment
   (local empty struct, as `map_if` holders do), keeping the default
   layout byte-identical to today's containers.
5. **The CPO is born MSVC-safe.** A template-parameterized member/ADL
   CPO is exactly the shape that armed MSVC's instantiation-time-lookup
   trap on the create-map CPOs; build `create_map_provider` in the
   `melon_create_map_cpo` mold (public name a variable template outside
   `namespace melon` — see the comment in undirected_graph.hpp) from day
   one.

**Rejected sibling:** parameterizing the graph by a bundled *type*
(Boost.Graph bundled properties). More transparent, but it pushes
roles-to-members glue onto the user and couples the graph parameter to
specific algorithms' state; the byte arena keeps the coupling in the
provider and stays automatic.

## Semantic versioning: this lands as a minor (1.x.0)

Every piece can be landed in the widening direction, so the feature is a
minor bump — under three riders:

- **Cleanly minor as-is:** relaxing the class-level `has_vertex_map`
  into the default provider (capability widening; SFINAE-observable in
  the false-to-true direction, conventionally minor); the new roles,
  provider concept and CPO (additive names); behavior (the default
  provider *is* today's factories — no change without opt-in).
- **Rider 1 — containers via alias, never in place.** `mutable_digraph`
  (and any other container gaining an arena) is a plain class; adding a
  template parameter to the existing name breaks every bare mention.
  Introduce the template under a new name and keep the current one as
  the `N = 0` alias (`using mutable_digraph = basic_mutable_digraph<0>;`).
  With the alias: minor. Without: major, full stop.
- **Rider 2 — provider constructor overloads are concept-constrained**
  so they can never make an existing call (e.g. the source-vertex form)
  ambiguous. Design obligation, not version arithmetic.
- **Rider 3 — the API-stability page reserves the maneuver.** Appending
  a trailing defaulted template parameter breaks only user forward
  declarations and template-template matches; add one line to the
  stability doc — users may not forward-declare melon types, and
  defaulted template parameters may be appended in minor versions —
  ideally before the v1.0.0 retag, so the minor-ness is defensible by
  published contract rather than arguable. (Still pending as of this
  note.)

## First step when picked up

Settle the role-key surface first — it shapes every algorithm's
provider-facing API. Prototype against `dijkstra` (status map + heap
index map) and `bidirectional_dijkstra` (the same-value-type collision
case) before generalizing; bring the arena CPO up only after the
graph-backed provider round-trips those two.
