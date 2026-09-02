# Mappings

A **mapping** is melon's abstraction for "data indexed by something" — lengths per arc, distances per vertex, a boolean filter per element. Like the [graph concepts](concepts.md), it is deliberately minimal: anything with an `operator[]` that can be read through a const access is a candidate, and the refinements say what more you can do with it. The concepts and the ownership views live in `melon/mapping.hpp`; the [ready-made maps](#the-ready-made-maps) beyond `maps::identity` each have their own header under `melon/maps/`.

## The concept

```cpp
template <typename Map, typename Key>
concept mapping = detail::subscriptable_with<Map, Key> &&
                  !std::same_as<mapped_value_t<Map, Key>, void>;
```

Two requirements: the map is subscriptable by the key — `detail::subscriptable_with` is the named helper for that clause, checking `m[k]` on an lvalue map — and there is actually a value to read. The second clause carries more than it looks like it does — `mapped_value_t` is defined through a **const access**, so a `mapping` is a map you can *read* from a const object. That consequence is easy to miss and is covered [below](#stdmap-is-not-a-mapping).

The associated aliases extract the value type; they are constrained on the subscript alone, so they stay usable on maps that fail the readability clause:

```cpp
template <typename Map, typename Key>
    requires detail::subscriptable_with<Map, Key>
using mapped_reference_t = decltype(std::declval<Map &>()[std::declval<Key>()]);

template <typename Map, typename Key>
    requires detail::subscriptable_with<Map, Key>
using mapped_const_reference_t =
    decltype(std::declval<const Map &>()[std::declval<Key>()]);

template <typename Map, typename Key>
    requires detail::subscriptable_with<Map, Key>
using mapped_value_t = std::decay_t<mapped_const_reference_t<Map, Key>>;
```

`mapped_reference_t` is what a mutable access yields, `mapped_const_reference_t` what a const access yields, and `mapped_value_t` the decayed value behind it.

## Writing, prefetching

```cpp
template <typename Map, typename Key>
concept output_mapping =
    mapping<Map, Key> &&
    requires(Map map, Key key, mapped_value_t<Map, Key> value) {
        { map[key] = value }
            -> std::same_as<std::add_lvalue_reference_t<mapped_reference_t<Map, Key>>>;
    };
```

`output_mapping` adds assignment. A mapping that does not satisfy it is read-only. The shape of the requirement — checking the *type of the assignment expression* rather than requiring a plain `T&` — is what lets proxy references qualify, so `std::vector<bool>` is a perfectly good output mapping alongside `std::vector<double>` and your own type.

The requirement also demands that a write actually *lands*: the subscript must return an lvalue reference into storage, or a proxy standing in for one. A map whose subscript returns a prvalue of the value type — a computed map, such as `maps::function` over a lambda returning by value — is readable but is not an `output_mapping`, because `m[k] = v` would assign into a temporary.

```cpp
template <typename Map, typename Key>
concept contiguous_mapping =
    mapping<Map, Key> && std::integral<Key> && requires(Map & m) {
        { m.data() } -> std::convertible_to<
                            std::add_pointer_t<std::add_const_t<mapped_value_t<Map, Key>>>>;
    };
```

`contiguous_mapping` requires integral keys and a `data()` pointer to a flat block. This is not a micro-optimization detail: it is what lets the shortest-path algorithms issue explicit prefetches for the values they are about to touch, which is a large part of why they are fast on big graphs.

## The `..._of` refinements

`mapping_of`, `output_mapping_of` and `contiguous_mapping_of` add one requirement: the mapped value is *exactly* the given type.

```cpp
template <typename Map, typename Key, typename Value>
concept mapping_of =
    mapping<Map, Key> && std::same_as<mapped_value_t<Map, Key>, Value>;
```

Use them when the value type is fixed by the problem rather than deduced from the map — `output_mapping_of<F, arc_t<G>, bool>` for an arc filter, for instance. Algorithms that infer their arithmetic from the map, such as Dijkstra, take the unrefined `mapping_view<arc_t<Graph>>` and let the length type follow.

## What qualifies

Verified against the concepts as written:

| Type | `mapping` | `output_mapping` | `contiguous_mapping` |
| --- | :-: | :-: | :-: |
| `std::vector<double>` | ✓ | ✓ | ✓ |
| `std::array<int, 4>` | ✓ | ✓ | ✓ |
| `std::vector<bool>` | ✓ | ✓ | |
| `static_map<K, V>` | ✓ | ✓ | ✓ |
| `static_filter_map<K>` | ✓ | ✓ | |
| `std::map` / `std::unordered_map` | | | |
| `maps::constant<V>` (`true_map`, `false_map`) | ✓ | | |
| `maps::identity`, `maps::element<I...>` | ✓ | | |
| `maps::function(callable)` | ✓ | | |
| `maps::transform(m, f)` | ✓ | | |

### `std::map` is not a `mapping`

`std::map::operator[]` is non-const — it inserts a default-constructed value when the key is absent, so it cannot be offered on a const map. A `mapping` must be readable through a *const* access, so `mapping<std::map<K, V>, K>` is **false**. The same holds for `std::unordered_map`.

That does not stop you passing one to an algorithm: map arguments are routed through [`maps::mapping_all`](#mapping-views), whose views dispatch each subscript to `m[k]`, then `m(k)`, then `m.at(k)` — with the constness the wrapped map carries.

```cpp
std::map<unsigned int, double> lengths = ...;

for(auto && [v, d] : dijkstra(graph, std::as_const(lengths), s)) { ... }   // reads via at()
```

The constness matters. Wrapping the non-const `lengths` directly also compiles, but the resulting `mapping_ref_view<std::map<...>>` holds a non-const reference — its subscript still finds the inserting `operator[]` first, so a missing key silently default-inserts a value. The `at()` branch, which throws on a missing key instead, engages only for a const-qualified wrapped map: a reference view of a const map, as above, or a `mapping_owning_view` read through const.

Where it does bite is a `static_assert`, or a template of your own constrained on `mapping`. Assert on the wrapped type — that is what the algorithm will actually hold:

```cpp
static_assert(!mapping<std::map<unsigned int, double>, unsigned int>);
static_assert(output_mapping_of<maps::mapping_all_t<std::map<unsigned int, double> &>,
                                unsigned int, double>);
```

## Maps that come from the graph

Algorithms rarely take their scratch space from you; they [ask the graph for it](concepts.md#attaching-data-to-vertices-and-arcs):

```cpp
auto dist   = create_vertex_map<double>(g, 0.0);
auto in_cut = create_arc_map<bool>(g, false);
```

The resulting types are `vertex_map_t<G, double>` and `arc_map_t<G, bool>`, chosen by the graph implementation. For melon's containers they are `static_map`s, which are `contiguous_mapping`s — so an algorithm that requires contiguity gets it, and one that does not still works when the graph hands back something else.

## Mapping views

Just as graphs are wrapped by [`views::graph_all`](../views/ownership.md), mappings passed to an algorithm go through `maps::mapping_all`, which yields:

- `mapping_ref_view` — a non-owning reference, when the argument is an lvalue;
- `mapping_owning_view` — takes ownership, when it is an rvalue.

That is what makes both of these safe:

```cpp
std::vector<double> length = ...;
dijkstra a(g, length, s);                       // ref view; `length` must outlive `a`
dijkstra b(g, std::move(length), s);            // owning view; `b` holds the vector
```

`mapping_owning_view` uses a `std::ranges`-style movable box, so an algorithm stays movable even when the mapping it owns is a capturing lambda.

### `maps::function`

`maps::function(f)` wraps any callable into a mapping. You do not need it to pass a lambda to an algorithm — that wrapping happens automatically — but you do need it wherever a `mapping` is required *as a type*: a member of your own class, a `static_assert`, an explicit template argument.

```cpp
// unit lengths, no storage
auto unit = maps::function([](auto &&) { return 1; });

// lengths derived from coordinates
auto euclidean = maps::function([&](arc_t<G> a) { return distance(pos[arc_source(g, a)],
                                                              pos[arc_target(g, a)]); });
```

!!! warning "A `mutable` lambda is not a `mapping`"

    A mutable lambda's `operator()` is non-const, so `maps::function` over one is
    not readable through a const access and fails `mapping` — the same
    const-readability requirement that rules out `std::map`. The map stays
    usable through its non-const subscript; it is only the const one that
    leaves the overload set. Spell a stateful map as a *const* lambda handing
    out a reference into storage it does not own:

    ```cpp
    std::vector<double> storage(n);
    auto m = maps::function([&storage](arc_t<G> a) -> double & { return storage[a]; });
    ```

    This spelling is also an `output_mapping` — the subscript returns a real
    lvalue reference — where the mutable-capture version never could be.

### The ready-made maps

`maps::identity` ships with `melon/mapping.hpp` itself; the rest live in [`melon/maps/`](../reference/headers.md#maps--melonmaps), one header each:

| Mapping | Header | `m[k]` yields |
| --- | --- | --- |
| `maps::constant<V>` | `melon/maps/constant.hpp` | `V` for every key — `maps::true_map` / `maps::false_map` are its `<true>` / `<false>` aliases |
| `maps::identity` | `melon/mapping.hpp` | the key itself |
| `maps::element<I...>` | `melon/maps/element.hpp` | `std::get<I>...` applied in sequence to the key |
| `maps::transform(m, f)` | `melon/maps/transform.hpp` | `f(m[k])` — the base's mapped value through a projection |

`maps::constant`'s value is an NTTP, so the map is an empty type — only structural types qualify; a runtime constant is `maps::function` over a capturing lambda. `maps::true_map` is the default filter of [`views::subgraph`](../views/graphs.md#subgraph) — and since it is empty and stored with `[[no_unique_address]]`, an unfiltered subgraph costs nothing and its `vertices()` is the underlying range itself rather than a `filter_view`. `maps::identity` and `maps::element` are how [`d_ary_heap`](../containers/data-structures.md#heaps) is told where to find an entry's priority and identifier: `maps::element<1>` reads `.second` of a `std::pair` entry, `maps::element<0>` its `.first`.

`maps::transform(m, f)` routes its base through `maps::mapping_all` — an lvalue is referenced, an rvalue owned, a view passed through — and applies `f` to the mapped *value*, never to the key. With a value-returning projection the result is read-only; a projection handing back a real lvalue reference keeps the base's writability, and the mutable-lambda rule above applies to `f` exactly as it does to `maps::function`. The returned class is `transform_map_view`, in `melon` itself like the other view classes — take it by `auto`; its exact type depends on the projection's.

## Next

- [Undirected graphs](undirected-graphs.md) — edges, and `create_edge_map`.
- [Ownership and mapping views](../views/ownership.md) — the full `mapping_all` story.
- [Maps, heaps and sets](../containers/data-structures.md) — `static_map` and `static_filter_map` in detail.
