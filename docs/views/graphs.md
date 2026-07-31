# Graph views

A view is a graph that computes its answers from another graph instead of storing them. It holds a reference (or ownership, see [Ownership and mapping views](ownership.md)), copies no adjacency, and satisfies the same [concepts](../graphs/concepts.md) as any container — so every algorithm accepts one, and views compose with each other.

```cpp
for(auto && [v, dist] : dijkstra(views::reverse(graph), length_map, t)) { ... }
```

Views follow the `std::ranges` shape: the *class* lives in `melon` (`reverse_view`, `subgraph_view`, `induced_subgraph_view`, `undirect_view`) and the *adaptor object* you actually spell lives in `namespace melon::views` (`views::reverse`, `views::subgraph`, …), one header each under `melon/views/`.

## Pipe syntax

Every adaptor supports `operator|`. For an argument-free adaptor the two spellings name exactly the same type, so the pipe costs nothing by construction; a *bound* adaptor stage differs from the direct call in one deliberate way, explained below:

```cpp
auto r1 = views::reverse(graph);
auto r2 = graph | views::reverse;               // same type as r1

auto sub = graph | views::subgraph(keep);       // bound closure
auto rsub = graph | views::reverse | views::subgraph();

auto adaptor = views::reverse | views::subgraph();  // closures compose
auto rsub2 = graph | adaptor;                   // same type as rsub
```

A multi-argument adaptor binds first, like `std::views::filter`: `views::subgraph(vf)` returns a self-contained closure holding a *copy* of the filter, so it is reusable and never dangles; `g | views::subgraph` without the parentheses is a compile error. Custom adaptors get the same behavior by deriving from `views::graph_adaptor_closure` (the melon analogue of `std::ranges::range_adaptor_closure`, whose `operator|` requires a `std::ranges::range` and therefore cannot serve graphs).

That copy is the one place the two spellings differ. An *lvalue* map passed to the **direct call** is stored **by reference** — the same [`maps::mapping_all`](ownership.md) rule every algorithm applies to its map arguments: reference for lvalues, ownership for rvalues — while the **bound closure** cannot hold a reference without dangling, so it always copies. With a writable filter the difference is observable:

```cpp
auto keep = create_vertex_map<bool>(graph, true);

auto s1 = views::subgraph(graph, keep);   // references keep
s1.disable_vertex(1u);                    // writes keep[1u]

auto s2 = graph | views::subgraph(keep);  // copies keep
s2.disable_vertex(1u);                    // writes s2's own copy, keep untouched
```

Either semantics is spellable in either form; only the lvalue default differs:

| you want | direct call | pipe |
|---|---|---|
| the view references your map | `views::subgraph(g, keep)` | `g \| views::subgraph(mapping_ref_view(keep))` |
| the view owns its own copy | `views::subgraph(g, auto(keep))` | `g \| views::subgraph(keep)` |

`mapping_ref_view(keep)` through the pipe is like piping a `std::ranges::ref_view`: you named the reference, so its lifetime is on you. The same rule splits `induced_subgraph`'s vertex range — ref-viewed by the direct call for an lvalue, copied by the closure.

## `reverse`

`views::reverse(g)` presents `g` with every arc turned around: what was an out-arc is an in-arc, sources become targets.

```cpp
#include "melon/views/reverse.hpp"

auto r = views::reverse(graph);

out_arcs(r, v);       // == in_arcs(graph, v)
arc_source(r, a);     // == arc_target(graph, a)
out_neighbors(r, v);  // == in_neighbors(graph, v)
```

Arc and vertex identifiers are unchanged, so a length map built for `graph` is a valid length map for `views::reverse(graph)` — which is what makes the one-liner above compute distances *to* `t`:

```cpp
// shortest distance from every vertex to t
for(auto && [v, dist] : dijkstra(views::reverse(graph), length_map, t)) { ... }
```

The view exposes an inward capability exactly when the underlying graph has the matching outward one, and vice versa. Reversing a `static_forward_digraph`, which has no in-arcs, yields a graph with no *out*-arcs — correct, and rejected at compile time by anything that needs to move forward.

## `subgraph`

`views::subgraph(g, vertex_filter, arc_filter)` restricts a graph to the elements its filters accept. Both filters are [mappings](../graphs/mappings.md) to `bool` and both default to `maps::true_map`:

```cpp
#include "melon/views/subgraph.hpp"

auto keep = create_vertex_map<bool>(graph, true);
keep[1u] = false;

auto sub = views::subgraph(graph, keep);
for(auto && v : vertices(sub)) { ... }   // 1 is gone
```

Filtering is **consistent, not merely lazy**: an arc is visible only if its own filter accepts it *and* both endpoints pass the vertex filter. `out_arcs(sub, v)` drops arcs pointing at a disabled vertex, `in_arcs` drops arcs coming from one, and `arcs(sub)` is filtered accordingly. You never see a dangling arc.

Because `true_map` is an empty type held with `[[no_unique_address]]`, the specializations matter:

- with both filters defaulted, every accessor forwards straight through — an unfiltered `subgraph` costs nothing and adds no `filter_view`. That includes the capabilities: `num_vertices`, `num_arcs`, `out_degree`/`in_degree` and the graph's own `arcs_entries` are forwarded, and the view is [borrowed](ownership.md#borrowed-graphs) exactly when the wrapped view is;
- with only an arc filter, only the arc ranges are wrapped; `arcs_entries` stays available, filtered — which also makes an arc-filtered subgraph of an entries-only graph a full graph;
- with a vertex filter, arc ranges also check the far endpoint, which is where the extra `arc_target` lookup per arc comes from.

The flip side: the moment any filter is attached, the sized capabilities go away — `has_out_degree`/`has_in_degree` (and `num_arcs`) are `false` for a filtered subgraph, since a filter can hide arcs a count cannot see. An algorithm constrained on them will reject the filtered view.

### Filters you can flip

When the filter maps are writable, the view forwards the mutation:

```cpp
auto sub = views::subgraph(graph, create_vertex_map<bool>(graph, true),
                                  create_arc_map<bool>(graph, true));

sub.disable_vertex(2u);
sub.enable_arc(7u);
```

`disable_vertex` / `enable_vertex` and `disable_arc` / `enable_arc` are constrained on the filter being an `output_mapping_of<..., bool>`, so they simply do not exist on a view built over `true_map` or a lambda. All four are non-`const` — the filter is part of the view's value, so flipping it through a `const subgraph_view &` does not compile. This is how you get a "graph with elements temporarily switched off" — the pattern flow and branch-and-bound codes want — without rebuilding anything.

*Whose* map they write depends on how the filter came in: built over an lvalue map in a direct call, the view references **your** map — `disable_vertex` writes it, and your own later writes show through the view — while a filter passed as an rvalue (as above) or through a piped closure is owned by the view, and your map is untouched. See [Pipe syntax](#pipe-syntax) for the full table.

A [`static_filter_map`](../containers/data-structures.md#static_filter_map) is a natural filter here: one bit per element, and `filter()` to enumerate what is on.

### `induced_subgraph`

When the subgraph is defined by a *list* of vertices rather than a predicate, `views::induced_subgraph(g, vertices_range)` is more direct: it builds the boolean filter once from the range and keeps the range itself as its `vertices()`, so iterating the subgraph iterates your list rather than scanning and filtering the whole vertex set.

```cpp
std::vector<vertex_t<static_digraph>> keep = {0u, 2u, 5u};
auto ind = views::induced_subgraph(graph, keep);

for(auto && v : vertices(ind)) { ... }   // 0, 2, 5 — in your order
for(auto && a : arcs(ind)) { ... }       // only arcs with both ends in the list
```

The range follows the same storage rule as a filter map: an lvalue range is ref-viewed — keep it alive, and unchanged, for the view's lifetime, since the boolean filter is built from it once at construction — while an rvalue, or the copy a piped closure holds, is owned by the view.

Unlike `views::subgraph`, an induced subgraph has no `enable_vertex` / `disable_vertex`: the filter and the vertex list are two spellings of one vertex set, and flipping a bit in the filter would desync them — `vertices()` would keep naming a vertex the graph no longer has.

## `undirect`

`views::undirect(g)` presents a digraph as an [undirected graph](../graphs/undirected-graphs.md): each arc becomes an edge with the same identifier, and the incidence of a vertex is the concatenation of its out- and in-incidences.

```cpp
#include "melon/views/undirect.hpp"

auto ugraph = views::undirect(graph);
for(auto && e : kruskal(ugraph, cost_map)) { ... }
```

It requires the underlying graph to be both an `outward_incidence_graph` and an `inward_incidence_graph`. Since edges keep the arc identifiers, arc maps double as edge maps.

## `complete_digraph`

`views::complete_digraph<V, A>` is a view over nothing at all: the complete digraph on `n` vertices, with all `n(n-1)` arcs computed arithmetically from their identifier.

```cpp
#include "melon/views/complete_digraph.hpp"

views::complete_digraph cd(4);

num_vertices(cd);   // 4
num_arcs(cd);       // 12
arc_source(cd, 5);  // 1
arc_target(cd, 5);  // 3
```

Arc `a` leaves vertex `a / (n - 1)`; self-loops are skipped, so the targets of vertex `u` are the other `n - 1` vertices in order. `out_degree` and `in_degree` are O(1) `noexcept` members answering the constant `n - 1` — for `in_degree` that member is the only reason the capability exists at all, since `in_arcs` is a concatenation and not sized — and the view is [borrowed](ownership.md#borrowed-graphs). There is no storage and no allocation, which makes it the right input for a dense problem — a TSP instance, a metric closure — where the arc data lives in a `maps::map` over the endpoint coordinates rather than in a container:

```cpp
auto dist = [&](auto a) {
    return euclidean(pos[arc_source(cd, a)], pos[arc_target(cd, a)]);
};

for(auto && [v, d] : dijkstra(cd, dist, 0u)) { ... }
```

The template parameters are the integer types for vertices and arcs, both `unsigned int` by default — worth widening for large `n`, since the arc count is quadratic.

## Composition

Views are graphs, so they nest, and the compiler tracks the capabilities through the stack:

```cpp
auto v = views::reverse(views::subgraph(graph, keep));
for(auto && [u, d] : dijkstra(v, length_map, t)) { ... }
```

Each layer is a thin object holding a pointer (or, for an rvalue, the graph itself) plus its filters, so the stack costs no allocation and no indirection beyond the accessor calls, which inline.

Two things to keep in mind:

- **Identifiers pass through unchanged.** `reverse`, `subgraph` and `undirect` never renumber, which is precisely why maps built on the base graph stay usable at every level.
- **Lifetime follows the reference.** `views::subgraph(graph, keep)` keeps pointers to `graph` and `keep`; both must outlive the view and any algorithm holding it. Passing a temporary graph makes the view own it instead — see [Ownership and mapping views](ownership.md).
