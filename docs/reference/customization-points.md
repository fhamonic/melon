# Customization points

Every accessor in melon — `melon::vertices`, `melon::out_arcs`, `melon::arc_target`, … — is a **customization point object** (CPO): a constexpr function object in `namespace melon`, not an ordinary function. Three consequences:

1. **Two ways to provide one.** A CPO accepts a member function (`g.out_arcs(v)`) or a free function found by ADL (`out_arcs(g, v)`). The member is preferred. You never specialize anything inside `namespace melon`.
2. **They cannot be hijacked by ADL at the call site.** `vertices(g)` inside a `using namespace melon;` scope resolves to the CPO, which then does the lookup itself.
3. **Several of them fall back.** When a graph does not provide a function directly, the CPO synthesizes it from what the graph *does* provide — which is why a structure with three members is already a `graph`.
4. **They all return by value.** Every range-returning CPO decay-copies, so `vertices_range_t<G>`, `arcs_range_t<G>` and friends are never reference types. Return a *view*, not a reference to a container: if your graph stores its vertices in a `std::vector`, return `std::views::all(_vertices)` — a `std::ranges::ref_view`, which copies nothing. Returning `const std::vector<vertex> &` would be copied by the CPO on every call. The same rule is why `arc_targets_map()` returns a `mapping_ref_view` rather than a `const static_map &`.
5. **They demand an lvalue graph.** A range- or closure-returning CPO rejects a temporary at compile time: `melon::arcs_entries(build_graph())` would hand back a view into an object that dies at the end of the expression, exactly as `std::ranges::begin` refuses rvalue containers. Bind the graph to a name first. The one exception is a [borrowed](../views/ownership.md#borrowed-graphs) graph providing the protocol itself — its handed-out ranges outlive the object, so a temporary is admitted there.

## The full table

`✓` = must be provided directly (member or ADL) if you want it. Everything else lists what it is derived from when absent.

### Vertices and arcs

| CPO | Fallback when not provided |
| --- | --- |
| `vertices(g)` | — (primitive) |
| `num_vertices(g)` | `std::ranges::size(vertices(g))` when that range is sized |
| `arcs(g)` | join of `out_arcs` over all vertices, or of `in_arcs` — see [below](#choosing-between-fallbacks) |
| `num_arcs(g)` | `std::ranges::size(arcs(g))` when that range is sized |
| `arcs_entries(g)` | `arcs` + `arc_source` + `arc_target`, or a join of the out- or in-incidences |

### Incidence and adjacency

| CPO | Fallback when not provided |
| --- | --- |
| `out_arcs(g, v)` | — |
| `in_arcs(g, v)` | — |
| `arc_source(g, a)` | — |
| `arc_target(g, a)` | — |
| `out_degree(g, v)` | `std::ranges::size(out_arcs(g, v))` when sized |
| `in_degree(g, v)` | `std::ranges::size(in_arcs(g, v))` when sized |
| `out_neighbors(g, v)` | `out_arcs(g, v)` transformed by `arc_target` |
| `in_neighbors(g, v)` | `in_arcs(g, v)` transformed by `arc_source` |
| `arc_sources_map(g)` | `maps::map` over `arc_source` |
| `arc_targets_map(g)` | `maps::map` over `arc_target` |

### Data

| CPO | Fallback |
| --- | --- |
| `create_vertex_map<T>(g)` / `create_vertex_map<T>(g, d)` | — |
| `create_arc_map<T>(g)` / `create_arc_map<T>(g, d)` | — |

Both overloads must be provided, and the result must model `output_mapping_of<vertex_t<G>, T>` (respectively `arc_t<G>`).

### Mutation

None of these has a fallback; providing one is what makes the corresponding concept true.

| CPO | Concept |
| --- | --- |
| `create_vertex(g)` | `has_vertex_creation` |
| `remove_vertex(g, v)`, `is_valid_vertex(g, v)` | `has_vertex_removal`; `is_valid_vertex` alone satisfies `has_is_valid_vertex` |
| `create_arc(g, u, v)` | `has_arc_creation` |
| `remove_arc(g, a)`, `is_valid_arc(g, a)` | `has_arc_removal`; `is_valid_arc` alone satisfies `has_is_valid_arc` |
| `change_arc_source(g, a, s)` | `has_change_arc_source` |
| `change_arc_target(g, a, t)` | `has_change_arc_target` |

### Undirected

| CPO | Fallback |
| --- | --- |
| `edges(g)` | — |
| `num_edges(g)` | `std::ranges::size(edges(g))` when sized |
| `edge_endpoints(g, e)` | — |
| `incidence(g, v)` | — |
| `degree(g, v)` | `std::ranges::size(incidence(g, v))` when sized |
| `create_edge_map<T>(g)` / `create_edge_map<T>(g, d)` | — |

## Choosing between fallbacks

`arcs` and `arcs_entries` may have several routes available at once, and the CPO picks by **range category**, preferring the stronger one. The internal `detail::range_rank` ranks contiguous over random-access over bidirectional over forward over input.

For `arcs_entries` the priority is:

1. a member or ADL `arcs_entries` — always wins, *provided its entries have the documented shape*: tuple-likes of size 2 pairing the arc with a tuple-like `(source, target)` pair. A member with any other shape is not the protocol — the way `std::ranges::begin` ignores a member `begin()` that returns a non-iterator — and the CPO moves on to the routes below;
2. listing `arcs` and pairing each with `arc_source`/`arc_target`, when the `arcs` range ranks at least as high as the incidence ranges;
3. joining the out-incidences, when they rank above the in-incidences;
4. joining the in-incidences otherwise.

For `arcs`, when both incidence directions are available, whichever range ranks higher is joined.

The practical reading: a structure that stores an explicit arc list keeps its random-access arc iteration, and one that stores adjacency lists gets a correct forward-only arc range rather than nothing. If you can produce entries more directly than either route, define `arcs_entries` and the choice is skipped.

## `noexcept` propagation

Each CPO computes its own `noexcept` from the expression it will actually evaluate. A `noexcept` member accessor yields a `noexcept` CPO call; the synthesized fallbacks that build range adaptors are conservatively not `noexcept`. Nothing is asserted about your accessors — mark them `noexcept` when they are, and it propagates.

## Writing an adapter

```cpp
namespace their_lib {
struct their_graph { ... };

inline auto vertices(const their_graph & g) noexcept { ... }
inline auto out_arcs(const their_graph & g, unsigned int v) noexcept { ... }
inline unsigned int arc_target(const their_graph & g, unsigned int a) noexcept { ... }

template <typename T> auto create_vertex_map(const their_graph & g) { ... }
template <typename T> auto create_vertex_map(const their_graph & g, const T & d) { ... }
}  // namespace their_lib
```

Nothing is added to `namespace melon`; ADL from the argument type finds them. The map factories are called with an explicit template argument — `create_vertex_map<T>(g)` — and that works because on melon's side `create_vertex_map` is a *variable* template wrapping a CPO object, not a function template: the melon name is invisible to ADL, so the ADL probe inside the CPO can only ever find *your* `create_vertex_map`.

See [Bringing your own graph](../graphs/custom-graphs.md) for complete, compiling examples and the rules the ranges must respect. `graph_ref_view`, which every algorithm wraps its argument in, forwards every read-only accessor in the tables above — `arcs_entries` when the wrapped graph provides its own; otherwise the CPO fallback synthesizes it — so a type stays a `graph` once wrapped whichever protocol it provides. It does not forward the mutating CPOs — a view is read-only by construction — but it does forward `is_valid_vertex` and `is_valid_arc`: those are questions, not mutations, and the standalone concepts `has_is_valid_vertex<G>` / `has_is_valid_arc<G>` name a graph that answers them.
