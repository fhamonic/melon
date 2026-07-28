# Changelog

All notable changes to this project are documented in this file.
The project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html);
see the [API stability](README.md#api-stability) section of the README for the
exact scope of the guarantees.

## 1.0.0 — unreleased

First stable release.

### Breaking changes

- **Removed the `fhamonic` umbrella namespace.** Everything now lives directly
  in `namespace melon`: replace `fhamonic::melon::` with `melon::`.
- **Compiler baseline is GCC 14 / C++23 or Clang 18 / C++23** (GCC 15 / C++26
  is recommended; GCC 14/15, Clang 18 and MinGW GCC 15 are all tested in CI). When the standard library does not provide
  `std::ranges::concat_view` (`__cpp_lib_ranges_concat`), melon transparently
  falls back to a bundled implementation; the former `MELON_ENABLE_GCC14_SUPPORT`
  (aka `gcc14_compat`) CMake option was removed in favor of this automatic
  feature-test-macro detection. Earlier pre-releases required GCC 15 / C++26.
- **Dropped the range-v3 dependency.** melon is now dependency-free; the
  `melon/1.0.0-alpha.1` package on Conan Center still predates this change.

### Fixed

- `melon/all.hpp` compiles again (it referenced two non-existent headers) and
  now includes the entire public API; a dedicated test translation unit keeps
  it from rotting.
- All headers use `#pragma once`, fixing a duplicated include guard that made
  `algorithm/unbounded_knapsack_bnb.hpp` a silent no-op when included after
  `algorithm/knapsack_bnb.hpp`, as well as collision-prone unprefixed guards
  in `utility/rational.hpp` and `utility/geometry.hpp`.
- `mapping_owning_view` (and therefore every algorithm taking a mapping by
  value) is now `std::movable` even when it owns a non-assignable functor such
  as a capturing lambda, using a `std::ranges`-style movable-box.
- Two GCC-only constructs that Clang rightfully rejects: an ill-formed
  deduction guide in the `concat_view` fallback (redundant, removed) and
  missing `template` keywords on dependent names in `bentley_ottmann.hpp`.
  melon now compiles with Clang 18.

### Added

- `melon/version.hpp` with `MELON_VERSION_MAJOR` / `MELON_VERSION_MINOR` /
  `MELON_VERSION_PATCH` and a combined `MELON_VERSION` macro for feature
  testing. It is the single source of truth for the version number, parsed by
  both `CMakeLists.txt` and `conanfile.py`.
- CMake install and export rules: `cmake --install` now ships the headers
  together with `melonConfig.cmake` / `melonConfigVersion.cmake`
  (`SameMajorVersion`, architecture-independent), so non-Conan consumers can
  use `find_package(melon CONFIG)`. The library target is also exported under
  the conventional namespaced alias `melon::melon`, and carries the
  `cxx_std_23` compile feature.
- `MELON_SANITIZE` CMake option (e.g. `-DMELON_SANITIZE=address,undefined`)
  to build the test suite with sanitizers.

### Build system

- `MELON_BUILD_TESTS` now defaults to `PROJECT_IS_TOP_LEVEL`: consuming melon
  via `add_subdirectory` no longer builds the test suite nor downloads GTest.
- Warning flags are applied privately to the test executable instead of being
  attached to the exported `melon` target; consumers no longer inherit
  `-Wall -Wextra …`.
- AddressSanitizer is no longer silently enabled whenever `libasan` is found;
  use `MELON_SANITIZE` instead. The GCC-specific
  `-fconcepts-diagnostics-depth` flag is now gated on the compiler, unblocking
  MSVC/MinGW configuration.
- `cmake_minimum_required` unified to 3.24 (was an inconsistent 3.12/3.13).
- Conan: `package_type = "header-library"`, and the recipe now configures the
  top-level `CMakeLists.txt` (like a regular consumer) instead of a duplicate
  library target defined under `test/`.

### `experimental/`

- Everything under `melon/experimental/` moved from `namespace melon` to
  **`namespace melon::experimental`**, so the code that carries no stability
  guarantee can no longer be reached from the stable namespace by accident.
- `planar_map.hpp` and `dual.hpp` were repaired (two missing semicolons, a
  stale `#include "melon/planar_map.hpp"` path, and the `has_arc_twin` /
  `has_arc_face` concepts that `dual.hpp` used but nobody had written) and
  are now covered by `test/experimental.cpp`, which keeps them compiling.
- `scapegoat_tree.hpp` and `doubly_connected_digraph.hpp` are still
  unfinished — they do not compile once instantiated — so they are **no
  longer shipped**: both the CMake install rules and the Conan package now
  exclude them. They remain in the repository, each with a file-level note
  describing what is missing.

### CI

- New jobs: Clang 18, ASan + UBSan (via `MELON_SANITIZE`, through the pure
  CMake + FetchContent path), and a `clang-format --dry-run -Werror` check
  pinned to clang-format 18 — the whole tree has been reformatted with it.
- `actions/checkout` bumped from the deprecated v3 to v7; Conan profiles moved
  from `.github/workflows/` to `.github/conan-profiles/`.
