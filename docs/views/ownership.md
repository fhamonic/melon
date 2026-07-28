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
template <typename _G, typename _M>
constexpr dijkstra(_G && g, _M && l)
    : _graph(views::graph_all(std::forward<_G>(g)))
    , _length_map(views::mapping_all(std::forward<_M>(l)))
    ...
```

and why their deduction guides are written in terms of `views::graph_all_t<_Graph>`.

### Marking your own type as a view

If you write a graph adaptor of your own and want it to pass through rather than be wrapped, derive it from `graph_view_base`:

```cpp
class my_adaptor : public graph_view_base { ... };
```

`enable_graph_view<T>` is `std::derived_from<T, graph_view_base>`, and `graph_view<T>` additionally requires `graph<T>` and `std::movable<T>`. The undirected side has the same pair: `undirected_graph_view_base` and `views::undirected_graph_all`.

## Mapping views

`views::mapping_all(m)` is the mapping counterpart, with `mapping_ref_view` and `mapping_owning_view`, `mapping_view_base` as the opt-in marker, and `mapping_all_t<M>` as the resulting type.

Two features are worth knowing beyond the ownership rule.

### They accept more than `operator[]`

A mapping view subscripts its target through the first of these that works:

1. `m[k]`
2. `m(k)`
3. `m.at(k)`

That is what makes a **callable** usable as a mapping, and what rescues `std::map`:

```cpp
// a lambda becomes a mapping
auto unit = views::map([](auto &&) { return 1; });

// std::map is not an input_mapping on its own (operator[] is non-const),
// but the view falls back to at()
std::map<unsigned int, double> lengths = ...;
auto length_map = views::mapping_all(lengths);
```

`views::map(f)` is a shorthand for `mapping_owning_view<F>(f)`. Use `views::mapping_all` when you have an lvalue container you want referenced, `views::map` when you have a callable to own.

!!! note

    You do not need either of them to *call* an algorithm — `dijkstra(g, lambda, s)`
    and `dijkstra(g, std_map, s)` both work, because the algorithm applies
    `views::mapping_all` itself. Reach for them when you need the mapping as a
    **type**: storing one in a class of your own, asserting a concept on it, or
    naming it in a template argument. A bare lambda satisfies neither `mapping`
    nor `input_mapping`; its wrapped form does.

### They stay movable

`mapping_owning_view` stores its target in a `std::ranges`-style movable box, so it remains `std::movable` even when what it owns is not assignable — a capturing lambda, typically. Without that, an algorithm holding a lambda-based length map would not be movable, and could not be returned from a function or stored in a container.

## Constness

`graph_ref_view<const G>` is a distinct type from `graph_ref_view<G>`, and the const one only forwards const-callable accessors — which is all of them, since every graph concept is written against `const _Tp &`. Passing a `const` graph is therefore free of surprises.

The mapping side is where constness bites: an algorithm that writes through a mapping needs `output_mapping`, and a `mapping_ref_view<const std::vector<double>>` is read-only. That is a compile error at the constructor, naming the concept.

## Summary

- Pass an lvalue when you own the storage and it outlives the algorithm; pass an rvalue, or `std::move`, when it does not.
- Views hold references too — `views::subgraph(graph, filter)` keeps both alive only if *you* do.
- Callables and non-const-subscriptable containers need no wrapping to be *passed* to an algorithm; wrap them with `views::map` / `views::mapping_all` when you need the mapping as a type.
- Derive from `graph_view_base` / `mapping_view_base` to make your own adaptors pass through unwrapped.
