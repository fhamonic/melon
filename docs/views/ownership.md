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

The last line is why views compose without stacking wrappers: a type that already satisfies `graph_view` — that is, a movable `graph` deriving from `graph_view_base` — passes straight through.

This is what every algorithm and every view does with its graph argument, which is why they all take it by forwarding reference:

```cpp
template <typename G, typename M>
constexpr dijkstra(G && g, M && l)
    : _graph(views::graph_all(std::forward<G>(g)))
    , _length_map(maps::mapping_all(std::forward<M>(l)))
    ...
```

and why their deduction guides are written in terms of `views::graph_all_t<Graph>`.

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

`maps::mapping_all(m)` is the mapping counterpart, with `mapping_ref_view` and `mapping_owning_view`, `mapping_view_base` as the opt-in marker, and `mapping_all_t<M>` as the resulting type.

Two features are worth knowing beyond the ownership rule.

### They accept more than `operator[]`

A mapping view subscripts its target through the first of these that works:

1. `m[k]`
2. `m(k)`
3. `m.at(k)`

That is what makes a **callable** usable as a mapping, and what rescues `std::map`:

```cpp
// a lambda becomes a mapping
auto unit = maps::map([](auto &&) { return 1; });

// std::map is not a mapping on its own (operator[] is non-const),
// but the view falls back to at()
std::map<unsigned int, double> lengths = ...;
auto length_map = maps::mapping_all(lengths);
```

`maps::map(f)` is a shorthand for `mapping_owning_view<F>(f)`. Use `maps::mapping_all` when you have an lvalue container you want referenced, `maps::map` when you have a callable to own.

!!! note

    You do not need either of them to *call* an algorithm — `dijkstra(g, lambda, s)`
    and `dijkstra(g, std_map, s)` both work, because the algorithm applies
    `maps::mapping_all` itself. Reach for them when you need the mapping as a
    **type**: storing one in a class of your own, asserting a concept on it, or
    naming it in a template argument. A bare lambda does not satisfy
    `mapping`; its wrapped form does.

### They stay movable

`mapping_owning_view` stores its target in a `std::ranges`-style movable box, so it remains `std::movable` even when what it owns is not assignable — a capturing lambda, typically. Without that, an algorithm holding a lambda-based length map would not be movable, and could not be returned from a function or stored in a container.

## Copying an algorithm: `enable_borrowed_graph`

`depth_first_search`, `strongly_connected_components`, `connected_components`
and `dinitz` keep a *cursor* over an incidence range for each stack frame or
vertex, so a copy of the algorithm copies ranges the graph handed out. Whether
that is sound depends on what those ranges point at.

`graph_ref_view` is a bare pointer, so `out_arcs(v)` names storage that lives
outside the view — copying the algorithm, which relocates the stored view, does
not disturb it. `views::subgraph` is different: its filtered ranges capture
`this`, so a range obtained from a subgraph the algorithm stores *by value*
points back at that member, and a copy would leave the new object's cursors
aimed at the original's graph.

`melon::enable_borrowed_graph<G>` draws that line, mirroring
`std::ranges::enable_borrowed_range`. It is `true` for `graph_ref_view`,
`undirected_graph_ref_view` and `views::complete_digraph`, propagates through
`views::reverse` and `views::undirect`, and is `false` by default — including
for `views::subgraph` and `graph_owning_view`.

The four algorithms above constrain their copy constructor and copy assignment
on it, so copying over a subgraph is a compile error rather than a
use-after-free, and `std::copyable` reports the truth:

```cpp
auto dfs = melon::depth_first_search(graph, 0u);           // copyable
auto sub = melon::depth_first_search(subgraph_view, 0u);   // move-only
```

Moving is always available, and always sound. If you write a graph view whose
ranges do not refer to the view object, specialise the trait:

```cpp
template <>
inline constexpr bool melon::enable_borrowed_graph<my_view> = true;
```

## Constness

`graph_ref_view<const G>` is a distinct type from `graph_ref_view<G>`, and the const one only forwards const-callable accessors — which is all of them, since every graph concept is written against `const T &`. Passing a `const` graph is therefore free of surprises.

The mapping side is where constness bites: an algorithm that writes through a mapping needs `output_mapping`, and a `mapping_ref_view<const std::vector<double>>` is read-only. That is a compile error at the constructor, naming the concept.

## Summary

- Pass an lvalue when you own the storage and it outlives the algorithm; pass an rvalue, or `std::move`, when it does not.
- Views hold references too — `views::subgraph(graph, filter)` keeps both alive only if *you* do.
- Callables and non-const-subscriptable containers need no wrapping to be *passed* to an algorithm; wrap them with `maps::map` / `maps::mapping_all` when you need the mapping as a type.
- Derive from `graph_view_base` / `mapping_view_base` to make your own adaptors pass through unwrapped.
- Algorithms that cache incidence ranges are copyable only over a graph satisfying `melon::borrowed_graph`; everything is movable.
