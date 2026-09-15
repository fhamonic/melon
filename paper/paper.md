---
title: 'MELON: a modern C++23 library for graph algorithms and network optimization'
tags:
  - C++
  - graph algorithms
  - network optimization
  - shortest paths
  - network flows
  - concepts
  - ranges
authors:
  - name: François Hamonic
    orcid: 0000-0002-3383-3100
    corresponding: true
    affiliation: 1
affiliations:
  - name: Institut Méditerranéen de Biodiversité et d'Écologie marine et continentale (IMBE), Aix Marseille Univ, Avignon Université, CNRS, IRD, Marseille, France
    index: 1
    ror: 0409c3995
date: 15 September 2026
bibliography: paper.bib
---

# Summary

Graphs describe anything made of objects and pairwise relations: road and
transit networks, habitat patches and the corridors between them, supply
chains, meshes, state machines. The classical questions asked of them (which
vertices are reachable, what is the shortest or most reliable route, how much
flow a network can carry, what the cheapest way to route demand is) have
well-known algorithms, and research code has to compute them quickly, on data
that already lives in whatever structure the rest of the program uses.

MELON (Modern and Efficient Library for Optimization in Networks) is a
header-only, dependency-free C++23 library of graph containers, lazy graph
views and algorithms, built on C++20 concepts and ranges. It ships traversals
(breadth-first and depth-first search, topological sort, strongly and weakly
connected components), shortest paths (Dijkstra, A\*, bidirectional Dijkstra,
bi-objective Dijkstra, Bellman–Ford, network Voronoi), flows and trees
(Edmonds–Karp, Dinitz, network simplex for minimum-cost flow, Kruskal),
knapsack branch-and-bound and Bentley–Ottmann segment intersection, together
with the heaps, flat maps and disjoint-set structures they are built on.
Algorithms are constrained by concepts rather than written against one graph
class, so they run on MELON's containers, on its zero-copy views, and on the
user's own data structure when it exposes a handful of member or free
functions.

# Statement of need

C++ is the implementation language of choice when a graph computation is the
inner loop of a larger optimization or simulation, but the two established
generic C++ graph libraries each force a compromise. The Boost Graph Library
[@Siek2002] is generic, but its genericity predates language support for
concepts: it is expressed through traits classes, tag dispatch and external
property maps, and a mistake yields template diagnostics that are notoriously
hard to read. LEMON [@Dezso2011] is fast and comfortable, but it is
unmaintained (its last release, 1.3.1, dates from 2014) and needs source
patches to build under C++17 and later. In practice many research codes fall back on
`std::vector<std::vector<int>>` and a hand-written Dijkstra, and pay for it
later in correctness, performance and reuse.

MELON was written to keep LEMON's speed and ergonomics while expressing
Boost.Graph's genericity with the tools C++20 and C++23 now provide. Its
intended users are researchers and engineers who write performance-sensitive
graph code in modern C++: operations research and computational ecology, where
the author's own work originated, but also routing, network analysis and any
domain where the same algorithm must run over a graph, its reverse and a
filtered subgraph without duplicating memory, or over a structure the program
already holds (a raster grid, a mesh, a compressed sparse row array coming out
of a solver) without first copying it into a "real" graph.

# State of the field

Boost.Graph and LEMON are the direct predecessors, and MELON's documentation
carries a translation table from both. Neither can adopt concepts and ranges
without breaking a decades-old interface, and LEMON is no longer developed,
which is the "build rather than contribute" argument for a new library. Two
C++20 efforts are closer in spirit. NWGraph [@Lumsdaine2022] rebuilt a generic
graph library on ranges of ranges, and the WG21 Graph Library proposal
[@Ratzloff2024] draws on it to define a graph container interface for a
future standard. Both define algorithms as functions that run to completion,
with visitors or output maps for intermediate results, and both target the
standardization process rather than a shipping toolkit; MELON explores a
different point in the design space (algorithms as steppable ranges, graph-owned
maps, compile-time storage traits) and already provides the network-flow and
minimum-cost-flow algorithms an optimization user needs. Analysis toolkits
such as NetworKit [@Staudt2016], igraph [@Csardi2006] and graph-tool
[@Peixoto2014] have compiled cores but are driven from Python or R over a
fixed internal representation; they are the right tool for interactive network
analysis, not for embedding a generic algorithm into a C++ program over the
program's own types.

# Software design

Five decisions shape the library, each visible at the call site.

**The interface is a set of concepts, not a base class.** `graph`,
`outward_incidence_graph`, `has_vertex_map<G, T>` and their relatives describe
what a type must *do*, and every algorithm states in its signature exactly the
capabilities it needs. Instantiating an algorithm on an unsuitable structure
fails at the call site with a diagnostic naming the missing requirement. The
accessors the concepts are written against (`vertices`, `out_arcs`,
`arc_target`, …) are customization point objects that accept a member function
or a free function found by argument-dependent lookup, so a type from another
library can be adapted without touching it. Several of them *synthesize* what
is missing: a structure exposing `vertices`, `out_arcs` and `arc_target`
automatically gains `out_neighbors`, `arcs`, `arcs_entries`, degrees and
counts, and the synthesized range is given the strongest iterator category the
primitives allow.

**The graph owns its data maps.** Instead of external property maps handed in
by the caller, an algorithm asks the graph for scratch storage through
`create_vertex_map<T>(g)`, and states that need as a constraint. The storage
type is the graph's choice: MELON's containers hand back a flat array, a graph
with non-integral identifiers may hand back a hash map, and a view can hand
out storage that already exists. Maps are requested under a *role*, so two
maps of the same value type are never confused.

**Algorithms are steppable ranges.** An algorithm object exposes
`finished()`, `current()` and `advance()`, which makes it a standard input
range: the loop body is the visitor, `break` is the early exit, and
`std::views::take` or `std::ranges::find_if` compose with it directly. Two
searches can be advanced in lockstep, which is exactly how the bidirectional
and competing Dijkstra variants are implemented. Algorithm objects are
move-only, since copying a search state is never the cheap operation the
syntax suggests, and `reset()` reuses the allocated state across queries.

**You do not pay for what you do not use.** Each algorithm takes a traits type
selecting its heap, its semiring and what it records. Optional state lives in
`[[no_unique_address]]` members that collapse to zero bytes when a flag is
off, and the accessors that would read them are removed from the overload set
by a `requires` clause, so a misuse is a compile error rather than a runtime
check. Swapping the semiring turns Dijkstra into a most-reliable-path or a
widest-path search without touching the traversal.

**Views instead of copies.** `views::reverse`, `views::subgraph`,
`views::undirect` and the map-providing views are lazy adaptors that satisfy
the same concepts as containers, compose with a pipe operator, and follow the
`std::ranges` ownership rules: an lvalue is referenced, an rvalue is owned, a
view passes through unchanged.

The cost of these choices is a C++23 toolchain (GCC 14, Clang 18, Apple Clang
21 or MSVC 17.11 at minimum), longer compile times than a non-template
library, and single-threaded algorithms. Preconditions are asserted rather
than thrown, and the API outside `melon/detail/` and `melon/experimental/`
is frozen for the 1.x series under semantic versioning.

Performance comes from the same design: no virtual dispatch or type erasure
anywhere, a compressed adjacency container whose per-vertex arc ranges are
consecutive integers, contiguous maps that let the shortest-path family issue
explicit prefetches, array-indexed heap positions instead of hash lookups, and
a branchless breadth-first search selected by `if constexpr` when the traits
allow it. \autoref{fig:bench} shows two of the measurements published in the
companion benchmark repository [@melon_benchmark], where every library is
built with the same compiler and every timed configuration must produce the
same result digest before a chart is drawn.

![Runtime on the largest USA road network of the 9th DIMACS Implementation Challenge [@Demetrescu2009] (`USA-road-t.NE`, 1.52 M vertices, 3.9 M arcs), each library in its fastest container, GCC 14.1, `-O3`, AMD Ryzen 7 7800X3D, median of five repetitions. (a) Full runs of six algorithms; Boost.Graph has no correct weak-components algorithm for directed graphs. (b) Cost of a query that consumes the k nearest vertices from a source and stops, with every library in the leanest configuration its API allows: because a MELON algorithm is a range that allocates only what its traits request, an early-exit query costs one to two orders of magnitude less than through a run-to-completion interface, and the advantage amortizes as the search grows to cover the graph.\label{fig:bench}](figure.png)

# Research impact statement

MELON grew out of the author's doctoral work on landscape connectivity
optimization [@Hamonic2023thesis; @Hamonic2023; @Hamonic2023b], where the
inner loop is a large number of shortest-path computations over habitat
graphs, and it is the graph layer of GECOT, the Graph-based Ecological
Connectivity Optimization Tool published in Methods in Ecology and Evolution
[@Hamonic2025]: GECOT declares `melon/1.0.0` as a dependency and builds its landscape
graphs, per-species maps and connectivity computations on MELON's containers,
maps and shortest-path algorithms. A companion library by the same author for linear
and mixed-integer programming [@mippp] shares its design and is used alongside
it. The repository has been public since December 2021 with more than a
thousand commits, is packaged on Conan Center, and has received issues and
pull requests from external users, including a compiler-compatibility fix and
tests for the topological-sort algorithm; the 1.0.0 release froze the public
API after a redesign that removed every third-party dependency. The benchmark
repository [@melon_benchmark] also serves as an independently reproducible
comparison of LEMON, Boost.Graph and CGAL [@CGAL2024] on DIMACS
[@Demetrescu2009] and SNAP [@Leskovec2014] instances, with cross-library
result validation, which is useful beyond MELON itself.

# AI usage disclosure

<!-- TODO(author): edit this section so that it is accurate before submission. -->
This manuscript was drafted with the assistance of a generative AI coding
assistant (Claude, Anthropic), working from the repository, its documentation
and the benchmark data; the author reviewed, corrected and takes
responsibility for every statement. [State here whether generative AI tools
were used in the development of the software itself or of its documentation,
and if so how their output was verified: for example, through the test suite,
the sanitizer runs and the cross-library benchmark validation.]

# Acknowledgements

This work is grounded in the PhD thesis and postdoctoral positions of the
author, funded by Région Sud Provence-Alpes-Côte d'Azur, Natural Solutions,
the European Research Council grant SCALED to Cécile H. Albert (ERC-STG
no. 949812), the ANR project RESILIENCE (ANR-24-PEVD-0002) and the OASIS
project of Aix-Marseille University's ITEM institute. The author thanks the
external contributors to the repository, in particular Simon Spoorendonk for
the GCC 14 build fix and GitHub user pradkrish for the topological-sort tests.

# References
