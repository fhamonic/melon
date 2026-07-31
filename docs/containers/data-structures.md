# Maps, heaps and sets

These are the containers the algorithms are built on. They are not graph-specific, and nothing stops you from using them on their own — but their design is driven by what a graph algorithm needs: flat storage indexed by an integer identifier, a priority queue whose entries can be re-prioritized, and a union-find.

## `static_map`

```cpp
template <std::integral K = std::size_t, typename V = std::size_t>
class static_map;
```

A fixed-size array indexed by an integral key: one allocation, no capacity, no growth amortization. This is what `create_vertex_map` and `create_arc_map` return for melon's containers, and it is a [`contiguous_mapping`](../graphs/mappings.md#writing-prefetching) — which is what makes prefetching possible in the shortest-path algorithms.

```cpp
static_map<unsigned int, double> m(5, 0.0);   // size 5, all zero
m[2u] = 1.5;
m.fill(0.0);
m.resize(8);                                  // keeps the first 5, new tail indeterminate
m.reset(3);                                   // keeps nothing
```

`resize(n)` reallocates and keeps the elements that still fit; `reset(n)` reallocates and keeps nothing. Neither initialises — after a growing `resize` the new tail is indeterminate, unlike `std::vector::resize` — and both are a no-op at the current size. Checked access is `at()` (const and non-const, throwing `std::out_of_range`); `empty()`, `swap` and iterator-pair / forward-range constructors round out the std-container surface, and the single-size constructor is `explicit`. A moved-from `static_map` is a valid empty map — the moves are hand-written so the source never reports a stale `size()` over a null buffer.

It is also a `random_access_range`, so `std::ranges` algorithms apply to its values directly. `static_map<K, bool>` is a plain array of `bool` — one byte per entry, unlike `std::vector<bool>` — and therefore stays contiguous and gives out real `bool&` references.

## `static_filter_map`

```cpp
template <std::integral K>
class static_filter_map;
```

The bit-packed counterpart: one *bit* per key, with a proxy reference. It is a container for *storing and enumerating sparse key sets* — not a drop-in replacement for `static_map<K, bool>`, which remains the right default for the random-access "visited map" pattern. What earns `static_filter_map` its place is `filter()`: a bit scan that skips 64 keys per word via `countr_zero`, enumerating the set keys many times faster than any per-key loop when they are sparse — something neither `static_map<K, bool>` nor `std::vector<bool>` (which hides its words) can express. The eight-fold memory saving and word-wise `fill()` come on top.

```cpp
static_filter_map<unsigned int> keep(8, false);
keep[3u] = true;
keep[5u] = true;

for(auto && k : keep.filter(std::views::iota(0u, 8u)))
    std::print(" {}", k);          //  3 5
```

`filter()` takes its bit-scan path for any *common* `std::views::iota` over an integral type — the key range's value type does not have to match `K`, and `views::take`/`views::drop` of an iota collapse back to an iota, so clipped ranges qualify too (an unbounded `std::views::iota(0u)` is not common and falls back to the generic branch). Bounds are clamped into `[0, size())`. The returned range is a `std::ranges::subrange` of a named, storable forward iterator: multipass and borrowed, ended by `std::default_sentinel` (pipe through `std::views::common` if an iterator pair is required — the sentinel keeps two compares per set key out of the scan's hot loop). Any other key range still works, filtered key by key.

Like `static_map` it offers `at()`, `empty()` and `swap`, its single-size constructor is `explicit`, and a moved-from object is a valid empty map; unlike `static_map` it has no `resize` — only the content-discarding `reset(n)`.

It is an `output_mapping` but **not** a `contiguous_mapping` — bits have no address — so an algorithm that requires contiguity will reject it. Being a bool `output_mapping` is also what makes it a natural filter map for [`views::subgraph`](../views/graphs.md#subgraph) — the view reads the filter through its subscript; `filter()` is for your own enumeration of the key set.

## Heaps

The heaps are described by two concepts in `melon/utility/priority_queue.hpp`:

```cpp
template <typename Q>
concept priority_queue = std::semiregular<Q> &&
    requires(Q q, typename Q::value_type v) {
    q.push(v);
    { q.top() } -> std::convertible_to<typename Q::value_type>;
    q.pop();
    { q.size() } -> std::same_as<typename Q::size_type>;
    { q.empty() } -> std::convertible_to<bool>;
    q.clear();
};

template <typename Q>
concept updatable_priority_queue = priority_queue<Q> &&
    requires(Q q, typename Q::id_type i, typename Q::priority_type p) {
    { q.contains(i) } -> std::convertible_to<bool>;
    { q.priority(i) } -> std::same_as<typename Q::priority_type>;
    q.promote(i, p);
    q.demote(i, p);
};
```

Anything satisfying them can be substituted into an algorithm through its [traits](../algorithms/shortest-paths.md#traits) — a bucket queue of your own for integer priorities, or any heap with the `std::priority_queue` shape: `top()` may return by value or by `const` reference, whichever suits it. (`std::priority_queue` itself falls one member short: it has no `clear()`, which the algorithms' `reset()` relies on.)

### `d_ary_heap`

```cpp
template <std::size_t D, typename Entry,
          typename PriorityComparator = std::greater<Entry>,
          mapping<Entry> EntryPriorityMap = maps::identity_map>
class d_ary_heap;
```

| Parameter | Meaning |
| --- | --- |
| `D` | children per node — 2 is a binary heap, 4 tends to win on large workloads |
| `Entry` | the element type stored |
| `PriorityComparator` | strict weak order on priorities; the element that compares *before* all others is on top |
| `EntryPriorityMap` | how to get an entry's priority; the default is the entry itself |

```cpp
d_ary_heap<2, int> max_heap;                     // std::greater  -> largest on top
for(int e : {0, 7, 3, 5, 6, 11}) max_heap.push(e);
max_heap.top();                                  // 11

d_ary_heap<4, int, std::less<int>> min_heap;     // std::less     -> smallest on top
d_ary_heap<2, int, decltype(cmp)> h(cmp);        // stateful comparator, explicit ctor
```

Watch the direction: with the default `std::greater` the maximum is on top. Dijkstra's default traits use `std::less` (from `shortest_path_semiring`), which puts the minimum on top.

`top()` returns `const value_type &` — the `std::priority_queue` shape. The reference points into the heap array, so it is invalidated by `push()`, `pop()`, `promote()` and `demote()`; copy first when the entry has to survive one of those (the in-tree algorithms do). The single-comparator constructor is `explicit`, so `d_ary_heap<2, int, Cmp> h = cmp;` does not compile.

### `updatable_d_ary_heap`

```cpp
template <std::size_t D, typename Entry,
          typename PriorityComparator = std::greater<Entry>,
          typename IndicesMap = mapping_owning_view<std::unordered_map<Entry, std::size_t>>,
          mapping<Entry> EntryPriorityMap = maps::identity_map,
          mapping<Entry> EntryIdMap = maps::identity_map>
class updatable_d_ary_heap;
```

Adds `contains(id)`, `priority(id)`, `promote(id, p)` and `demote(id, p)`, by tracking where each entry lives. The three extra parameters say how:

- `IndicesMap` maps an entry's identifier to its index in the heap array. The default is a hash map; for integral identifiers, pass a `static_map` and the lookup becomes an array access — this is exactly what `dijkstra_default_traits` does with `vertex_map_t<Graph, std::size_t>`.
- `EntryPriorityMap` extracts the priority from an entry.
- `EntryIdMap` extracts the identifier from an entry.

Stateful maps are passed through the 2-, 3- and 4-argument constructors — `(cmp, indices_map)`, `(cmp, indices_map, entry_priority_map)`, `(cmp, indices_map, entry_priority_map, entry_id_map)`; omitted maps are default-constructed.

For a `std::pair<vertex, distance>` entry, the last two are `maps::element_map<1>` and `maps::element_map<0>`:

```cpp
using entry = std::pair<unsigned int, double>;

updatable_d_ary_heap<2, entry, std::less<double>,
                     static_map<unsigned int, std::size_t>,
                     maps::element_map<1>,   // priority is entry.second
                     maps::element_map<0>>   // id is entry.first
    heap(std::less<double>{}, static_map<unsigned int, std::size_t>(num_vertices));

heap.push({0u, 3.0});
heap.push({1u, 1.0});
heap.push({2u, 5.0});

heap.contains(1u);      // true
heap.contains(2u);      // true -- any live entry, not only the one on top
heap.priority(1u);      // 1.0
heap.promote(2u, 0.5);  // now on top
```

`promote` asserts that the new priority is an improvement in the comparator's sense, and `demote` the opposite. With `std::less` — minimum on top — promoting means *decreasing*. Calling the wrong one is an assertion failure in a debug build and silent corruption in a release build.

Two more contract points:

- `promote` and `demote` exist only when `EntryPriorityMap` hands back a non-const *lvalue reference* to the priority inside the entry (the `mutable_entry_priority_map` concept) — a map yielding a copy or a detached proxy would make the write land on a temporary and vanish. In particular, with the default `maps::identity_map` — which returns a copy — the heap has **no** `promote`/`demote` at all; `maps::element_map<I>` into a pair or tuple entry qualifies.
- `contains(id)`, `priority(id)`, `promote` and `demote` require the id to have been *pushed at least once*: the index map is caller-supplied and not initialised by the heap, so looking up a never-pushed key reads indeterminate memory.

## `disjoint_sets`

```cpp
template <typename K,
          output_mapping<K> M = mapping_owning_view<std::unordered_map<K, unsigned int>>>
    requires std::integral<mapped_value_t<M, K>>
class disjoint_sets;
```

Union-find with union-by-size and path halving, over arbitrary key types. The second parameter is how a key is mapped to its internal component index; as with the heap, the default is a hash map, and passing a `static_map` makes it an array access for integral keys.

```cpp
disjoint_sets<int> sets;
for(int e : {11, 20, 3, 14}) sets.push(e);

sets.merge_keys(11, 20);
sets.find(11) == sets.find(20);   // true
```

| Member | Effect |
| --- | --- |
| `push(k)` | adds `k` as a new singleton component |
| `find(k)` | the component index of `k`, compressing the path |
| `merge(c1, c2)` | merges two component indices, returns the survivor |
| `merge_keys(k1, k2)` | `merge(find(k1), find(k2))` |
| `size()`, `empty()`, `clear()` | on the number of pushed elements |

Every key must be `push`ed before it is looked up. `clear()` drops the components but **not** the key-to-component map: a key pushed before a `clear()` and not pushed again still maps to a component index the object no longer has, and `find()` on it is out of range — push every key you intend to query after each `clear()`. This is the structure [`kruskal`](../algorithms/flows-and-trees.md#kruskal) runs on.

## Other utilities

| Header | What it provides |
| --- | --- |
| `utility/semiring.hpp` | `shortest_path_semiring`, `most_reliable_path_semiring`, `max_capacity_path_semiring`, `minimum_spanning_tree_semiring` — see [Shortest paths](../algorithms/shortest-paths.md#semirings) |
| `numeric/rational.hpp` | `numeric::rational<NumT, DenT>` exact arithmetic, used by the geometric algorithms |
| `utility/geometry.hpp` | the `cartesian_point`, `cartesian_segment` and `cartesian_line` concepts and the `cartesian` traits |
| `numeric/bounded_value.hpp` | `numeric::bounded_value` — integer types that widen automatically instead of narrowing; unary `-` is deleted where the negated *range* would not fit (unsigned, or a signed range pinned at the type's minimum) |
| `utility/alias_method_sampler.hpp` | O(1) sampling from a discrete distribution |
| `utility/algorithmic_generator.hpp` | the [`algorithmic_generator` concept](../algorithms/index.md) and the range adaptor built on it |
