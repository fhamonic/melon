# Bringing your own graph

The point of describing graphs with [concepts](concepts.md) is that melon's algorithms are not tied to melon's containers. If your program already holds its topology in a structure of its own — a mesh, a state machine, a CSR array from a solver, a grid with implicit neighbors — you can run melon's algorithms on it directly, with no adapter class and no copy.

This page shows the two ways to do that: implementing the interface as members on a type you own, and adapting a type you do not own through ADL.

## The minimum

Provide three member functions and melon derives the rest.

```cpp
#include <ranges>
#include <vector>

#include "melon/container/static_map.hpp"
#include "melon/graph.hpp"

using namespace melon;

// A compressed adjacency structure: the arcs of vertex v are the identifiers
// in [_begin[v], _begin[v + 1]).
class csr_digraph {
public:
    using vertex = unsigned int;
    using arc = unsigned int;

private:
    std::vector<arc> _begin;      // size num_vertices + 1
    std::vector<vertex> _target;  // size num_arcs

public:
    csr_digraph(std::vector<arc> begin, std::vector<vertex> target)
        : _begin(std::move(begin)), _target(std::move(target)) {}

    auto vertices() const noexcept {
        return std::views::iota(vertex(0),
                                static_cast<vertex>(_begin.size() - 1));
    }
    auto out_arcs(const vertex v) const noexcept {
        return std::views::iota(_begin[v], _begin[v + 1]);
    }
    vertex arc_target(const arc a) const noexcept { return _target[a]; }
};
```

That is already a graph:

```cpp
static_assert(graph<csr_digraph>);
static_assert(outward_incidence_graph<csr_digraph>);
static_assert(outward_adjacency_graph<csr_digraph>);
static_assert(has_num_vertices<csr_digraph>);
static_assert(has_out_degree<csr_digraph>);
```

Only `vertices`, `out_arcs` and `arc_target` were written. `melon::arcs`, `melon::arcs_entries` and `melon::out_neighbors` are [synthesized](../reference/customization-points.md) from them, and `num_vertices` and `out_degree` come from the ranges being sized. The capabilities that genuinely are not there are correctly reported missing:

```cpp
static_assert(!has_arc_source<csr_digraph>);
static_assert(!inward_incidence_graph<csr_digraph>);
```

### What is still missing

Almost every algorithm needs per-vertex scratch space, so it requires `has_vertex_map`. Add the four map factories — two per element kind, with and without an initial value:

```cpp
    template <typename T>
    auto create_vertex_map() const {
        return static_map<vertex, T>(_begin.size() - 1);
    }
    template <typename T>
    auto create_vertex_map(const T & d) const {
        return static_map<vertex, T>(_begin.size() - 1, d);
    }
    template <typename T>
    auto create_arc_map() const {
        return static_map<arc, T>(_target.size());
    }
    template <typename T>
    auto create_arc_map(const T & d) const {
        return static_map<arc, T>(_target.size(), d);
    }
```

`static_map<K, V>` is melon's flat array for integral keys; it satisfies [`contiguous_mapping`](mappings.md#writing-prefetching), so algorithms that prefetch will. For non-integral identifiers, return a hash map — anything modelling `output_mapping_of<key, T>` is accepted.

Both overloads of each factory are required by the concept, and the value-initialized one must actually honour its argument: melon relies on `create_vertex_map<bool>(g, false)` producing a map that reads `false` everywhere.

With those in place, the structure runs the algorithms:

```cpp
csr_digraph g({0, 3, 4, 6, 7, 7, 8}, {1, 2, 5, 3, 3, 5, 4, 4});
std::vector<double> length = {7.0, 9.0, 14.0, 15.0, 12.0, 2.0, 6.0, 9.0};

for(auto && [v, dist] : dijkstra(g, length, 0u))
    std::println("vertex {} at distance {}", v, dist);
```

### One thing that is not free

```cpp
static_assert(!has_num_arcs<csr_digraph>);
```

`melon::arcs(g)` was synthesized by joining the per-vertex arc ranges, and a `join_view` is not a `sized_range` — so `num_arcs` has nothing to fall back on. Where the count is known, say so, and the concept flips to true:

```cpp
    std::size_t num_arcs() const noexcept { return _target.size(); }
```

The same applies to `num_vertices`, `out_degree` and `in_degree`: melon only derives them from a sized range, and defining the member is both cheaper and more honest than letting a caller pay `std::ranges::distance`.

## Adapting a type you do not own

When the type comes from another library, define the same functions as **free functions in the type's own namespace**. Every melon accessor is a customization point object that looks for a member first and an ADL free function second, so nothing needs to be declared inside `melon`.

The example below also shows a graph that is *adjacent but not incident*: it holds a flat arc list and per-vertex neighbor lists, so it can name all its arcs and answer `out_neighbors`, but there is no range of "the arcs leaving `v`".

```cpp
namespace their_lib {

struct their_graph {
    std::vector<std::pair<unsigned int, unsigned int>> arc_list;
    std::vector<std::vector<unsigned int>> adjacency;
};

inline auto vertices(const their_graph & g) noexcept {
    return std::views::iota(0u, static_cast<unsigned int>(g.adjacency.size()));
}
inline auto arcs(const their_graph & g) noexcept {
    return std::views::iota(0u, static_cast<unsigned int>(g.arc_list.size()));
}
inline unsigned int arc_source(const their_graph & g, unsigned int a) noexcept {
    return g.arc_list[a].first;
}
inline unsigned int arc_target(const their_graph & g, unsigned int a) noexcept {
    return g.arc_list[a].second;
}
inline auto out_neighbors(const their_graph & g, unsigned int v) noexcept {
    return std::views::all(g.adjacency[v]);
}

template <typename T>
auto create_vertex_map(const their_graph & g) {
    return melon::static_map<unsigned int, T>(g.adjacency.size());
}
template <typename T>
auto create_vertex_map(const their_graph & g, const T & d) {
    return melon::static_map<unsigned int, T>(g.adjacency.size(), d);
}

}  // namespace their_lib
```

```cpp
static_assert(graph<their_lib::their_graph>);
static_assert(outward_adjacency_graph<their_lib::their_graph>);
static_assert(!has_out_arcs<their_lib::their_graph>);
static_assert(!outward_incidence_graph<their_lib::their_graph>);

their_lib::their_graph tg{{{0, 1}, {0, 2}, {1, 2}}, {{1, 2}, {2}, {}}};
for(auto && v : breadth_first_search(tg, 0u)) std::println("bfs {}", v);
```

BFS runs, because it only requires adjacency. Dijkstra would not compile on this type, because it requires `outward_incidence_graph` to read a length per arc — which is the right answer, delivered at the call site.

## Rules to respect

**Ranges may be lazy, but they must be re-iterable.** melon walks `vertices(g)` and the incidence ranges more than once. Return a view over stable storage, or an `iota`; do not return a one-shot input range.

**Identifiers must be unique and stable.** No duplicates in `vertices(g)` or `arcs(g)`, and an identifier must keep meaning the same element for as long as the algorithm runs.

**Accessors must be const-callable.** Every concept is written against `const T &`. A non-const `out_arcs` is invisible to melon.

**Return by value or by reference, but not to a temporary.** `out_arcs(v)` returning a view over a member container is fine; returning a view over a local vector is a dangling range.

**`arcs_entries` is worth defining when you can.** It is synthesized from `arcs` + `arc_source` + `arc_target`, or by joining incidences — and melon [picks whichever route has the stronger range category](../reference/customization-points.md#choosing-between-fallbacks). If your structure can produce entries directly, a member `arcs_entries()` beats both. Its elements must have the `(a, (s, t))` shape [the `graph` concept pins](concepts.md) — `std::pair<arc, std::pair<vertex, vertex>>` is the usual spelling; a member yielding any other shape is ignored in favor of the synthesized routes.

**Views forward the whole read-only interface.** When an algorithm takes your graph, it wraps it in [`graph_ref_view`](../views/ownership.md), which forwards `vertices`, `arcs`, `num_vertices`, `num_arcs`, `arcs_entries`, `arc_source`, `arc_target`, `out_arcs`, `in_arcs`, `out_neighbors`, `in_neighbors`, `out_degree`, `in_degree`, `is_valid_vertex`, `is_valid_arc`, the endpoint maps and the map factories. So a graph whose *only* route to `arcs_entries` is a member of its own keeps satisfying `graph` once wrapped, and a container's own `arcs_entries` is not quietly replaced by the synthesized one.

What a view does **not** forward is the mutating half — `create_vertex`, `create_arc`, `remove_vertex`, `remove_arc`, `change_arc_source`, `change_arc_target`. A view is read-only, so `has_vertex_removal<graph_ref_view<G>>` is false even when `has_vertex_removal<G>` is true. The validity *question* is forwarded, which is what lets [`views::subgraph`](../views/graphs.md) tell a vertex you filtered out from one the graph underneath has removed.

## Checking your work

Put the static assertions next to the type. They cost nothing at runtime, they document the intent, and they turn a future regression into a one-line error rather than a template avalanche at some call site:

```cpp
static_assert(outward_incidence_graph<my_graph>);
static_assert(has_vertex_map<my_graph, double>);
static_assert(has_arc_map<my_graph, bool>);
```

For a worked reference implementation with every operation including mutation, see `test/dumb_digraph.hpp` in the repository — a deliberately naive structure kept in the test suite precisely to prove the concepts do not smuggle in assumptions about efficiency.
