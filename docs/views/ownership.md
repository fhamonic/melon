# Ownership and mapping views

Every melon algorithm stores the graph and the mappings it was given, and it must do so without either dangling on an lvalue or copying a temporary into oblivion. The mechanism is the same one `std::ranges` uses: a pair of *ref* / *owning* views and a factory that picks between them. You rarely name these types, but knowing the rule tells you when a lifetime is yours to manage.

## The rule

| You pass | melon stores | Lifetime |
| --- | --- | --- |
| an lvalue | a `graph_ref_view` / `mapping_ref_view` — a pointer | **yours**: the object must outlive the algorithm |
| an rvalue | a `graph_owning_view` / `mapping_owning_view` | the algorithm's |
| a view | the view itself, unchanged | the view's own |

```cpp
std::vector<double> length = ...;

dijkstra a(graph, length, s);              // ref views: graph and length must outlive a
dijkstra b(graph, std::move(length), s);   // b owns the vector
dijkstra c(views::reverse(graph), length, s);  // the reverse view owns nothing but a
                                               // pointer to graph — same rule applies
```

The classic mistake is returning an algorithm built on a local:

```cpp
auto make_search(const static_digraph & g) {
    auto length = compute_lengths(g);
    return dijkstra(g, length, 0u);   // ✗ length dies here
    return dijkstra(g, std::move(length), 0u);  // ✓ the algorithm owns it
}
```

## `views::graph_all`

`views::graph_all(g)` is the factory. Given any graph it yields the thing to store:

```cpp
static_assert(std::same_as<views::graph_all_t<static_digraph &>,
                           graph_ref_view<static_digraph>>);
static_assert(std::same_as<views::graph_all_t<const static_digraph &>,
                           graph_ref_view<const static_digraph>>);
static_assert(std::same_as<views::graph_all_t<static_digraph>,
                           graph_owning_view<static_digraph>>);
static_assert(std::same_as<views::graph_all_t<views::complete_digraph<>>,
                           views::complete_digraph<>>);   // already a view
```

The last line is why views compose without stacking wrappers: a type that already satisfies `graph_view` — that is, a movable `graph` deriving from `graph_view_base` — passes straight through, provided the value category allows it. An **lvalue** `graph_owning_view` fails that last test (its copy constructor is deleted) and becomes a `graph_ref_view` of the owning view instead of passing through.

This is what every algorithm and every view does with its graph argument, which is why they all take it by forwarding reference — and the constructors are constrained on exactly that wrap, so `std::is_constructible` answers honestly:

```cpp
template <graph_for<Graph> G, mapping_for<LengthMap> LM>
constexpr dijkstra(G && g, LM && lm)
    : _graph(views::graph_all(std::forward<G>(g)))
    , _length_map(maps::mapping_all(std::forward<LM>(lm)))
    ...
```

Their deduction guides are written in terms of `views::graph_all_t<Graph>`, and the class heads require view types for the stored members (`graph_view Graph`, `mapping_view<arc_t<Graph>> LengthMap`) — **stored members are always views**, the `std::ranges::transform_view` precedent: a raw-container member spelling like `dijkstra<static_digraph, static_map<…>>` is ill-formed, and value ownership is spelled `graph_owning_view` / `mapping_owning_view`. If you need to name an algorithm's type, use `decltype` on the CTAD spelling or spell the ownership explicitly — CTAD always produces exactly the type you could have spelled.

### Marking your own type as a view

If you write a graph adaptor of your own and want it to pass through rather than be wrapped, derive it from `graph_view_base`:

```cpp
class my_adaptor : public graph_view_base { ... };
```

`enable_graph_view<T>` is `std::derived_from<T, graph_view_base>`, and `graph_view<T>` additionally requires `graph<T>` and `std::movable<T>`. The undirected side has the same pair: `undirected_graph_view_base` and `views::undirected_graph_all`.

### Getting the graph back out: `base()`

Every view exposes `base()`, in whichever of the three `std::ranges` shapes matches what it is:

| Type | Shape | `base()` returns |
| --- | --- | --- |
| `graph_ref_view` | `std::ranges::ref_view` | `G &`, from a `const` object — constness is shallow |
| `graph_owning_view` | `std::ranges::owning_view` | four ref-qualified overloads: `G &`, `const G &`, `G &&`, `const G &&` |
| `reverse`, `subgraph`, `undirect` | `std::ranges::filter_view` | a **copy** of the adapted view: `Graph base() const &` (only when that copy is possible) and `Graph base() &&` |

The split matters when the adapted view is move-only. `reverse_view<graph_owning_view<G>>` owns its graph, so it has no `base() const &` — the copy cannot be made — and you reach it with `std::move(r).base()`, exactly as with a `std::ranges::filter_view` over an `owning_view`:

```cpp
auto r = views::reverse(static_digraph{...});   // owns the graph
auto g = std::move(r).base();                   // move it back out
```

Algorithms differ deliberately: an algorithm *owns* its graph view rather than adapting it, so its `base()` is the `owning_view` shape and returns references. See [Algorithms](../algorithms/index.md).

### Getting a result map out: the `*s_map()` accessors

The same ref-or-owning split applies on the way *out*. Every map accessor backed by a stored map — `flows_map()`, `dists_map()`, `reached_map()`, `component_ids_map()`, … — is a ref-qualified pair: an lvalue algorithm hands out a `mapping_ref_view` (valid while the algorithm lives and stays put), and an expiring one **moves the stored map** into a `mapping_owning_view`, so the result outlives the machinery that computed it:

```cpp
auto flows = std::move(dinitz(graph, capacity, s, t).run()).flows_map();
// owning: the algorithm is gone, the flow map lives on
```

Extraction is terminal, like `std::move(alg).base()`: the member left behind is valid but empty, so extract last and call nothing else afterwards. Mind also that a map handed out by an *lvalue* algorithm references the algorithm object — moving the algorithm afterwards invalidates it, the same contract `std::ranges` adaptors have over a moved container. When the map must outlive or outlast the algorithm, extract it from an expiring one. The handful of *computed* maps (`dijkstra`'s, `network_voronoi`'s, `strongly_connected_components`' and `biobjective_dijkstra`'s `reached_map()`, derived from richer state rather than stored as a bool map) extract too — their expiring overload moves the backing map (status enums, component indices, Pareto fronts) into the lambda of the returned computed map, so it is self-contained and outlives the algorithm just the same.

## Pipe closures own their arguments

A bound adaptor stage — `views::subgraph(filter)`, `views::induced_subgraph(vertex_range)` — stores *copies* of its arguments, and each application hands a copy (or, when the closure is a temporary, a move) into the view it builds. So the closure is reusable, the view it builds never points back into it, and the type built does not depend on how the closure was held:

```cpp
auto adaptor = views::subgraph(filter);   // owns a copy of filter
auto s1 = g | adaptor;                    // s1 owns its own copy
auto s2 = g | views::subgraph(filter);    // same type as s1

auto s3 = views::subgraph(g, filter);     // direct call: references filter
```

This is the one place the pipe and call spellings differ: the direct call keeps the reference-for-lvalues rule above, the pipe stage is self-contained, exactly like a `std::views::filter(pred)` closure.

## Mapping views

`maps::mapping_all(m)` is the mapping counterpart, and `maps::mapping_all_t<M>` the resulting type. Mind the namespaces: the factories live in `melon::maps`, while `mapping_ref_view`, `mapping_owning_view`, the `mapping_view_base` opt-in marker and the mapping concepts live in `melon::` itself — symmetric with `graph_ref_view` / `graph_owning_view`.

Two features are worth knowing beyond the ownership rule.

### They accept more than `operator[]`

A mapping view subscripts its target through the first of these that works:

1. `m[k]`
2. `m(k)`
3. `m.at(k)`

That is what makes a **callable** usable as a mapping, and what rescues `std::map`:

```cpp
// a lambda becomes a mapping
auto unit = maps::function([](auto &&) { return 1; });

// std::map is not a mapping on its own (operator[] is non-const);
// wrapped const, the view reads through at()
const std::map<unsigned int, double> lengths = ...;
auto length_map = maps::mapping_all(lengths);
```

The dispatch runs with the constness the wrapped map carries, so the `const` above matters: wrapping a **non-const** `std::map` lvalue finds its inserting `operator[]` at step 1 — a lookup of a missing key default-inserts. The throwing `at()` path is reached only through a const base (`std::as_const(lengths)`, or an owning view read through a const algorithm). See the [mappings chapter](../graphs/mappings.md#stdmap-is-not-a-mapping) for the full mechanism.

`maps::function(f)` is a shorthand for `mapping_owning_view<F>(f)`. Use `maps::mapping_all` when you have an lvalue container you want referenced, `maps::function` when you have a callable to own.

!!! note

    You do not need either of them to *call* an algorithm — `dijkstra(g, lambda, s)`
    and `dijkstra(g, std_map, s)` both work, because the algorithm applies
    `maps::mapping_all` itself. Reach for them when you need the mapping as a
    **type**: storing one in a class of your own, asserting a concept on it, or
    naming it in a template argument. A bare lambda does not satisfy
    `mapping`; its wrapped form does.

### They stay movable

`mapping_owning_view` stores its target in a `std::ranges`-style movable box, so it remains `std::movable` even when what it owns is not assignable — a capturing lambda, typically. Without that, an algorithm holding a lambda-based length map would not be movable, and could not be returned from a function or stored in a container.

## Relocating an algorithm: move-only, always sound

**Every algorithm is move-only.** `std::copyable<A>` is `false` for every
algorithm `A`, over every graph, and `std::movable<A>` is `true` for every one
of them. An algorithm carries the whole search state — each vertex map, the
heap, the cached cursors — so copying it is never the cheap operation the
syntax suggests; passing one by value is a compile error rather than a silent
O(V+E) duplication. The `melon::traversal_algorithm` concept — the
[lifecycle contract](../algorithms/index.md#the-lifecycle-contract) every
algorithm models — requires the movability.

If you want a second search, construct a second algorithm. If you want to
re-run one, `reset()` reuses the state it has already allocated. And do not
write `auto a = alg.run();` — `run()` returns `*this` by reference, so that
line would be a copy, and it does not compile; call `alg.run();` and read the
results through the accessors.

Moving, on the other hand, is always available and always sound — including
mid-traversal. Getting that right is what `enable_borrowed_graph` is for.

### Borrowed graphs

`depth_first_search`, `strongly_connected_components`, `connected_components`
and `dinitz` keep a *cursor* over an incidence range for each stack frame or
vertex, so relocating the algorithm relocates ranges the graph handed out.
Whether a memberwise move would suffice depends on what those ranges point at.

`graph_ref_view` is a bare pointer, so `out_arcs(v)` names storage that lives
outside the view — moving the algorithm, which relocates the stored view, does
not disturb it. A *filtered* `views::subgraph` is different: its filtered
ranges capture `this`, so a range obtained from a subgraph the algorithm
stores *by value* points back at that member, and a memberwise move would
leave the new object's cursors aimed at the moved-from object's graph.

`melon::enable_borrowed_graph<G>` draws that line, mirroring
`std::ranges::enable_borrowed_range`. It is `true` for `graph_ref_view`,
`undirected_graph_ref_view` and `views::complete_digraph`, and `false` by
default — including for `graph_owning_view`. The adaptors compute it from
what they wrap: `views::reverse` propagates it unchanged; a `subgraph_view`
is borrowed exactly when **both filters are `maps::true_map` and the wrapped
view is borrowed** (with any real filter present, the captured `this` makes
it non-borrowed); and `undirect_view` propagates it only for a
copy-constructible wrapped view — its incidence lambdas capture a *copy* of
the view in that case, which is what makes the promise true.

Where it is `false`, those four hand-write their move to *re-ask the new graph*
for each cached range; the cursor's consumed counter puts it back where it was.
You never see the difference:

```cpp
auto dfs = melon::depth_first_search(subgraph_view, 0u);
dfs.advance();
auto relocated = std::move(dfs);   // sound mid-run: the cursors are rebased
```

If you write a graph view whose ranges do not refer to the view object,
specialise the trait and that rebasing compiles away entirely:

```cpp
#include "melon/borrowed_graph.hpp"

template <>
inline constexpr bool melon::enable_borrowed_graph<my_view> = true;
```

!!! warning "Borrowedness is a promise you make"

    Specialise it `true` only if the ranges your graph hands out remain valid
    **independently of the graph object** — a non-owning view over external
    storage, for example. Claiming it falsely turns every algorithm move over
    your graph into a use-after-free; not claiming it merely costs the rebase
    loop. When unsure, leave it `false`.

## Constness

`graph_ref_view<const G>` is a distinct type from `graph_ref_view<G>`, and the const one only forwards const-callable accessors — which is all of them, since every graph concept is written against `const T &`. Passing a `const` graph is therefore free of surprises.

The mapping side is where constness bites: an algorithm that writes through a mapping needs `output_mapping`, and a `mapping_ref_view<const std::vector<double>>` is read-only. That is a compile error at the constructor, naming the concept.

## Summary

- Pass an lvalue when you own the storage and it outlives the algorithm; pass an rvalue, or `std::move`, when it does not.
- Views hold references too — `views::subgraph(graph, filter)` keeps both alive only if *you* do.
- Callables and non-const-subscriptable containers need no wrapping to be *passed* to an algorithm; wrap them with `maps::function` / `maps::mapping_all` when you need the mapping as a type.
- Derive from `graph_view_base` / `mapping_view_base` to make your own adaptors pass through unwrapped.
- Algorithms are move-only, uniformly. Those that cache incidence ranges rebase them on the move, so relocating one is always sound — mid-traversal included.
