# Installation

melon is header-only and dependency-free: adding its `include/` directory to your include path and compiling with C++23 is enough. The sections below cover the packaged routes, which additionally give you an imported CMake target.

## Requirements

**C++23** is the baseline; GCC 15 / C++26 is recommended. Every configuration below is exercised by CI on every commit.

| Compiler | Minimum version | CI configuration |
| --- | --- | --- |
| GCC | 14 | GCC 14 / C++23, GCC 15 / C++26 |
| Clang | 18 | Clang 18 / C++23 (with libstdc++ 14) |
| MinGW-w64 GCC | 15 | MinGW GCC 15 / C++26 (Windows) |
| MSVC | — | not supported |

CMake 3.24 or later is required for the CMake integration, and Conan 2.0 or later for the Conan one. Asking CMake for a **C++26** build needs **3.30**: `CXX_STANDARD 26` became a valid value in 3.25, but no `-std=c++26` mapping exists for GCC before 3.30, where the `cxx_std_26` compile feature was finally implemented. Older CMake is not an error — the `melon::melon` target requires only `cxx_std_23`, so you get a working C++23 build with the range shapes noted below.

!!! note "C++23 versus C++26"

    melon uses `std::ranges::concat_view`, which is a C++26 addition. When the
    standard library does not advertise it through `__cpp_lib_ranges_concat`,
    melon falls back to a bundled implementation — this needs no flag, and is
    why C++23 works. Nothing else in the library is conditional.

    The fallback is a forward range ending at a sentinel; std's is *also*
    sized, common and random-access whenever its bases are. Three members are
    built on it, and only one of them — over a graph whose own incidence
    ranges are richer than forward — can tell the difference:

    | expression | C++23 | GCC 15 / C++26 |
    | --- | --- | --- |
    | `incidence(ug, v)`, `ug` undirecting a `static_digraph` | forward | forward, bidirectional, random-access, sized, common |
    | `incidence(ug, v)`, `ug` undirecting a `mutable_digraph` | forward | forward |
    | `in_arcs(g, v)` on a `complete_digraph` | forward | forward |
    | `bidirectional_dijkstra::path()` | forward | forward |

    So **C++23 is the shape melon guarantees**: a range documented only by
    concept is a `forward_range` you walk to its end, and a C++26 standard
    library may hand you a stronger one but never a weaker one. Write against
    that floor — ask `degree(ug, v)` for a cardinality rather than sizing the
    range — and the same code compiles on every configuration in the table
    above. Code that requires `sized_range`, `common_range` or
    `random_access_range` of one of those ranges compiles on GCC 15 / C++26
    and fails on the GCC 14 minimum.

    To catch that from a C++26 build without installing GCC 14, configure with
    `-DMELON_PORTABLE_RANGE_SHAPES=ON` (or define the macro of the same name):
    melon then uses the fallback everywhere and reproduces the C++23 shapes.
    The setting must be the same for every translation unit of a program — it
    changes the return type of `incidence()` — which is why the CMake option
    puts it on the `melon::melon` interface target, and it is also why objects
    compiled as C++23 and as C++26 must not be linked into one program.

## As a Conan package

**From the repository** (the tagged 1.0.0 release — the recommended route):

```console
$ git clone https://github.com/fhamonic/melon
$ cd melon
$ conan create . -u -b=missing -pr=<your_conan_profile>
```

Then declare the dependency in your own `conanfile.txt`:

```ini
[requires]
melon/1.0.0

[generators]
CMakeDeps
CMakeToolchain
```

**From Conan Center** (latest published release):

```ini
[requires]
melon/1.0.0-alpha.1
```

!!! warning

    The `1.0.0-alpha.1` package on Conan Center is a pre-1.0 alpha that
    predates two breaking changes: it still depends on range-v3, and its
    symbols live in the `fhamonic::melon` namespace rather than `melon`.
    Prefer the repository / tagged release until the 1.0.0 package lands on
    Conan Center.

Either way, consume it from CMake with:

```cmake
find_package(melon CONFIG REQUIRED)
target_link_libraries(<your_target> PRIVATE melon::melon)
```

## As a CMake subdirectory

Clone melon into your project, or add it as a submodule:

```console
$ git submodule add https://github.com/fhamonic/melon dependencies/melon
```

then:

```cmake
add_subdirectory(dependencies/melon)
target_link_libraries(<your_target> PRIVATE melon::melon)
```

The `melon` target is an `INTERFACE` library carrying the `cxx_std_23` compile feature, so linking against it raises your target's standard if needed. It exports no warning flags of its own.

`MELON_BUILD_TESTS` defaults to `PROJECT_IS_TOP_LEVEL`, so consuming melon this way builds no tests and downloads no GTest.

## Installed system-wide

```console
$ cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMELON_BUILD_TESTS=OFF
$ cmake --install build --prefix /usr/local
```

This installs the headers together with `melonConfig.cmake` and
`melonConfigVersion.cmake` (`SameMajorVersion`, architecture-independent), so consumers can `find_package(melon CONFIG)` exactly as with the Conan package.

## Headers

Every component is a separate header under `melon/`, and you include what you use:

```cpp
#include "melon/container/static_digraph.hpp"
#include "melon/algorithm/dijkstra.hpp"
```

`melon/all.hpp` pulls in the entire public API — convenient for a scratch program, wasteful in a build you care about. The [header map](../reference/headers.md) lists every header and what it declares.

Everything lives in `namespace melon`. Prior releases nested it inside an umbrella `fhamonic` namespace; that was removed, so `fhamonic::melon::dijkstra` is now `melon::dijkstra`.

Four sub-namespaces carve out the parts that would otherwise take very generic names at top level: `melon::views` for graph views, `melon::maps` for mapping views, `melon::numeric` for `rational` / `integer` / `bounded_value`, and `melon::experimental`. See [Headers](../reference/headers.md).

## API stability

Starting with 1.0.0, melon follows [semantic versioning](https://semver.org): every header is frozen API for the whole 1.x series, with two explicit exceptions that carry **no stability guarantee** and may change or disappear in any release:

- `melon/detail/` — implementation details, as well as any symbol in a `detail` namespace;
- `melon/experimental/` — work-in-progress data structures. These live in `namespace melon::experimental`, so nothing reaches the stable `melon` namespace by accident.

The guarantee covers every documented name in `namespace melon`, `melon::views`, `melon::maps` and `melon::numeric` — the customization point objects (`vertices`, `out_arcs`, …) are reachable as `melon::` names and covered as such, while `melon::cpo`, which holds only their implementation types, stays out per the `detail` rule above — plus the behavioural contracts stated throughout this documentation — the [algorithm lifecycle](../algorithms/index.md#the-lifecycle-contract), the [ownership rules](../views/ownership.md), the [mapping concepts](../graphs/mappings.md). Those contracts are deliberate design decisions, each pinned by tests (`test/api_consistency.cpp`, `test/api_review.cpp`), and will not be relitigated within 1.x. If a 1.x release ever breaks code that follows them, that is a bug in melon.

!!! note "Types documented only by concept are not frozen"

    The exact type of a member documented only by concept — the concrete range
    type an algorithm's iteration yields, for instance, beyond what
    `traversal_algorithm` promises — is not covered. Spell such types with
    `auto` or `decltype`.

`melon/version.hpp` is the single source of truth for the version number and lets you feature-test:

```cpp
#include "melon/version.hpp"

#if MELON_VERSION >= 10000  // MAJOR * 10000 + MINOR * 100 + PATCH
    ...
#endif
```

## Building the test suite

From a checkout, with Conan:

```console
$ conan build . -b=missing -pr=<your_conan_profile>
```

or with plain CMake, which falls back to fetching GTest if it is not installed:

```console
$ cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
$ cmake --build build -j
$ ctest --test-dir build --output-on-failure
```

Set `MELON_SANITIZE` to build the suite with sanitizers, e.g. `-DMELON_SANITIZE=address,undefined`.

## Next steps

[A first graph](first-graph.md) walks through building a graph and running an algorithm on it.
