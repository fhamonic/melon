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

CMake 3.24 or later is required for the CMake integration, and Conan 2.0 or later for the Conan one.

!!! note "C++23 versus C++26"

    melon uses `std::ranges::concat_view`, which is a C++26 addition. When the
    standard library does not advertise it through `__cpp_lib_ranges_concat`,
    melon transparently falls back to a bundled implementation — this is
    detected per translation unit, needs no flag, and is why C++23 works.
    Nothing else in the library is conditional.

## As a Conan package

**From the repository** (latest commit — the recommended route while 1.0.0 is unreleased):

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

    The `1.0.0-alpha.1` package on Conan Center predates two breaking changes:
    it still depends on range-v3, and its symbols live in the
    `fhamonic::melon` namespace rather than `melon`. Prefer the repository
    route until 1.0.0 is published.

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

- `melon/detail/` — implementation details, as well as any symbol in a `detail` / `detail` namespace or prefixed with `__`;
- `melon/experimental/` — work-in-progress data structures. These live in `namespace melon::experimental`, so nothing reaches the stable `melon` namespace by accident.

Until 1.0.0 is tagged, treat the whole API as unstable and pin a commit.

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
