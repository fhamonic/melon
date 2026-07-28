# Contributing to melon

Thank you for considering contributing to melon! This document explains how to
build the project, run the tests, and what is expected from a pull request.

## Prerequisites

- **GCC 14** (C++23) or **Clang 18** (C++23, with libstdc++ ≥ 14) minimum;
  **GCC 15** (C++26) is recommended and what the main CI job uses. MinGW-w64
  GCC 15 is tested on Windows. MSVC is not supported.
- **CMake ≥ 3.24** (when using Conan, a suitable CMake is provisioned
  automatically through `tool_requires`).
- **Conan 2** for the standard workflow (optional — see the CMake-only
  workflow below).

## Building and running the tests

### With Conan (recommended)

```sh
conan profile detect          # once, if you have no default profile
make                          # = conan build . -b=missing -pr=<profile>
```

The `Makefile` is a thin wrapper around `conan build`, which configures the
top-level `CMakeLists.txt` with `MELON_BUILD_TESTS=ON`, builds `melon_test`
and runs the whole suite through CTest. Select a profile with
`make CONAN_PROFILE=<profile>`; the profiles used by CI live in
`.github/conan-profiles/`.

### With CMake only

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

`MELON_BUILD_TESTS` defaults to `ON` when melon is the top-level project. If
GTest is not found on your system it is fetched automatically with
FetchContent.

Useful configuration options:

| Option | Effect |
| ------ | ------ |
| `MELON_BUILD_TESTS` | Build the test suite (default: `PROJECT_IS_TOP_LEVEL`) |
| `MELON_SANITIZE` | Build the tests with sanitizers, e.g. `-DMELON_SANITIZE=address` or `address,undefined` (GCC/Clang) |
| `WARNINGS_AS_ERRORS` | Promote compiler warnings to errors |

Before submitting, please run the suite at least once with
`-DMELON_SANITIZE=address`.

## Test expectations

Every pull request must keep `melon_test` green on GCC 14, GCC 15 and
Clang 18 (CI enforces this, including a MinGW build on Windows and an
ASan+UBSan run). CI also rejects code that does not match `.clang-format`
(pinned to clang-format 18).

- **New features need tests.** Add a `test/<feature>.cpp` translation unit,
  register it in the `add_executable(melon_test …)` list in
  `test/CMakeLists.txt`, and write `GTEST_TEST(<suite>, <name>)` cases with
  real assertions. The helpers in `test/ranges_test_helper.hpp` (range
  equality/multiset comparison) and `test/dumb_digraph.hpp` (a minimal
  archetype graph) are there to be reused.
- **New public headers** must be self-contained and must be added to
  `include/melon/all.hpp` (kept in alphabetical order per section);
  `test/all.cpp` compiles the whole public surface, so a missing or broken
  header fails the build.
- **Bug fixes** should come with a regression test that fails without the fix.

## Formatting

Formatting is defined by the `.clang-format` file at the repository root
(Google style, 4-space indent, `Type & name` pointer/reference alignment,
no space before parentheses). Format anything you touch:

```sh
clang-format -i <files you changed>
```

## Code conventions

- Headers use `#pragma once`.
- Everything lives directly in `namespace melon`; nothing may be added to a
  nested umbrella namespace.
- Implementation details go in `melon/detail/` headers and/or `__detail`
  namespaces with `__`-prefixed names; public headers use `_UpperCamel`
  template parameters and `__`-prefixed function parameters in the
  libstdc++ tradition. Match the style of the file you are editing.
- `melon/detail/` and `melon/experimental/` carry no API stability
  guarantee; everything else is frozen for the 1.x series (see
  [API stability](README.md#api-stability)). Breaking changes to public
  headers must wait for a major release and be recorded in
  [CHANGELOG.md](CHANGELOG.md).
- Everything under `melon/experimental/` belongs in
  `namespace melon::experimental`. A header there is only shipped once it
  compiles: add it to `test/experimental.cpp` and to the install rules
  (`CMakeLists.txt` and the `package()` exclusion list in `conanfile.py`)
  in the same change that makes it work.
- User-visible changes (features, fixes, deprecations) get a line in
  `CHANGELOG.md` under the upcoming release.

## Versioning

The version number has a single source of truth:
`include/melon/version.hpp` (`MELON_VERSION_MAJOR` / `MINOR` / `PATCH`).
`CMakeLists.txt` and `conanfile.py` parse it — never edit a version number
anywhere else.

## License

melon is distributed under the [Boost Software License 1.0](LICENSE). By
submitting a contribution you agree to license it under the same terms.
