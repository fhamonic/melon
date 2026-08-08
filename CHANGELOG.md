# Changelog

Notable changes to melon. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[semantic versioning](https://semver.org) starting at 1.0.0.

## [Unreleased]

## [1.0.0] - 2026-08-09

First stable release. Every header outside `melon/detail/` and
`melon/experimental/` is frozen API for the 1.x series — see
[API stability](https://fhamonic.github.io/melon/getting-started/installation/#api-stability)
for the exact scope of the guarantee.

### Changed since `1.0.0-alpha.1` (the last package published on Conan Center)

- The library namespace is `melon`, no longer `fhamonic::melon`, and the
  CMake target is `melon::melon`.
- The range-v3 dependency is gone: melon is dependency-free and builds on
  C++23 standard library ranges (GCC 14 / Clang 18 with libstdc++ 14
  minimum; GCC 15 / C++26 recommended).
- Algorithms are move-only steppable ranges with a single lifecycle
  contract; preconditions are asserted, not thrown.
- Mappings are read through const access (`mapping` requires
  const-readability), and stored members are always views.
- The customization-point layer synthesizes missing graph operations
  (`out_neighbors`, `arcs`, degrees, counts) from the primitives a graph
  type exposes.

[Unreleased]: https://github.com/fhamonic/melon/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/fhamonic/melon/releases/tag/v1.0.0
