# Contributing to melon

Thank you for considering contributing to melon! This document explains how to
build the project, run the tests, and what is expected from a pull request.

## Prerequisites

- **GCC 14** (C++23) or **Clang 18** (C++23, with libstdc++ ≥ 14) minimum;
  **GCC 15** (C++26) is recommended and what the main CI job uses. MinGW-w64
  GCC 15 and MSVC (VS 2022 17.11, toolset v14.41) are tested on Windows.
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
- Everything lives directly in `namespace melon` — algorithms, containers and
  concepts gain no umbrella namespace. The nested namespaces are few and
  specific: `views::` and `maps::` hold what users *spell at call sites* —
  adaptors, factories (`views::reverse`, `maps::mapping_all`,
  `maps::transform`) and factory-less leaf maps whose type name is the
  interface (`maps::constant`, `maps::element`) — while a class *returned* by
  a factory stays in `melon` itself (`subgraph_view`, `reverse_view`, the
  ownership views, `transform_map_view`): users meet those through `auto`,
  not by name. Moving a single class into its category namespace breaks this
  rule rather than serving it. `numeric::` holds the arithmetic value types
  and `experimental::` the unstable work.
- **No reserved identifiers.** Nothing may contain a double underscore, or a
  leading underscore followed by an uppercase letter — those are reserved to
  the implementation in every scope ([lex.name]/3), and melon is not a standard
  library implementation. Template parameters are plain `UpperCamel` (`Graph`,
  not `_Graph`), implementation details live in `melon/detail/` headers and the
  `melon::detail` namespace, CPO function objects are `vertices_fn`, and
  detection concepts are `has_member_x` / `has_adl_x` / `can_x`. The only
  double-underscore names in the tree are compiler- and platform-owned
  (`__builtin_prefetch`, `__GNUC__`, `__cpp_lib_*`) and must stay.
  - Watch the constructor-parameter trap: when a class template's constructor
    deduces its own parameter, it may not reuse the class template parameter's
    name (the leading underscore used to keep them apart). Pick a distinct
    descriptive name — `Rng` against a stored `R`, `BlueMap` against a stored
    `BLM`. GCC rejects the clash as `-Wtemplate-body` "shadows template
    parameter".
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
- Trait- or capability-gated map members use `detail::vertex_map_if` /
  `detail::arc_map_if` from `melon/detail/map_if.hpp`, declared
  `[[no_unique_address]]`. The disabled variant is an empty type whose
  constructors *mirror* the enabled signatures — never widen them back to a
  variadic, or a malformed construction only fails for the first user whose
  flags turn the map on. Two disabled maps in one class need distinct
  `DiscriminatingT` tags to overlap to zero bytes. Gated members that are
  not maps use a local named empty struct as the disabled alternative, not
  a shared detail type, for the same address-overlap reason. The machinery's
  contract is pinned in `test/map_if.cpp`.
- User-visible changes (features, fixes, deprecations) get a line in
  `CHANGELOG.md` under the upcoming release.

## Comments

melon's teaching documentation is prose, under `docs/`, and each piece of its
API contract is stated once, in the docs page that owns the topic (the
algorithm lifecycle in `docs/algorithms/index.md`, ownership in
`docs/views/ownership.md`, and so on). Header comments are not a
second copy of either: the code is meant to be readable on its own, and a
comment earns its place only by saying something the code cannot. Three kinds
qualify.

- **Contracts** — preconditions, complexity, and semantic requirements no
  concept can check (Dijkstra's "an arc length must never improve a distance
  when combined"). Users read these through jump-to-definition, so state them
  in the header; keep it to the requirement, the consequence of violating it,
  and the bound. The motivation and the worked examples belong in `docs/`.
- **Traps** — decisions that *look* wrong and would be "fixed" by a competent
  maintainer: a `mutable` member, an `optional` that only exists for
  default-constructibility, a counter that looks redundant. These must name the
  invariant *and* the concrete failure that follows from breaking it, or
  someone will simplify them away. Terse is wrong here; "`// mutable:
  filter_view end()`" does not survive contact with a maintainer's
  simplification instinct.
- **Non-obvious mechanics** — why an overload is constrained the way it is, why
  a `noexcept` measures what it measures. One or two sentences.

A comment explaining why a specifier is **absent** is a keeper when the reason
is not visible in the body — `// Not noexcept: emplace_back may reallocate and
throw. It also sifts through the user's comparator and priority map.` earns its
place on the second sentence, because a maintainer reading the body sees only
the first throw source. Nothing distinguishes a deliberate omission from an
oversight, so these are worth repeating at each site they guard. But an absence
whose reason *is* right there (`// Not noexcept: it allocates.` over a body that
plainly allocates) is restatement like any other.

Everything else goes. In particular, **do not narrate history**: "the old
unconstrained template said yes to…", "this used to admit a settled vertex",
"this was the one `current()` carrying no specification". That justifies a past
change to a reviewer, which is what the commit message and `CHANGELOG.md` are
for, and it rots the moment the old code is gone. State the invariant in the
present tense instead.

Three more things go, and none is caught by looking at length — these are
usually one or two tidy, accurate sentences:

- **Restatement.** The comment says what the code says. `// Private, like every
  other container's` sitting under a `private:` label; a comment naming the
  type, the parameter, or what `= delete` does; `// Rebuilds the in-degrees and
  re-seeds the queue` over a call to `push_start_vertices()`. If a reader
  learns it by looking down one line, it is not documentation.
- **Reassurance.** A comment that explains why the code as written is correct,
  without warning against a specific wrong alternative, protects nothing. "No
  `Traits` parameter: the class template's own default computes it, so the
  deduced type and the explicitly written `dijkstra<G, LM>` agree" reads as a
  trap comment but names no failure, so a maintainer minded to add that
  parameter reads it and proceeds. Either name the failure or drop it.
- **Anything a test already pins.** The CTAD-agreement property above is
  enforced by `api_review.deduced_type_equals_the_spelled_out_one`, whose own
  comment *does* explain the failure. A `static_assert` that fires on the next
  build is stronger than a paragraph, and it cannot go stale.

Prefer references to compiler-checked things (`// Move-only; see the
melon::traversal_algorithm concept`) over references to sibling code (`// See
competing_dijkstras::current()`), which drift silently.

One discriminator covers all of the above: *does the comment name a **mistake**
— something a reader could do, or wrongly conclude, that the code alone would
not prevent?* If it only describes what is there, or reassures that what is
there is right, delete it. A maintainer re-simplifies and breaks an invariant —
keep it, with the failure named. A user misuses the API — one-line contract
here, full prose in `docs/`. When genuinely torn, keep it: a borderline keeper
costs two lines, deleting a real trap costs a bug.
[`algorithm/dijkstra.hpp`](include/melon/algorithm/dijkstra.hpp) is the
reference for the resulting density.

### Structural dividers

The discriminator above governs comments that make claims. Headers under
`include/melon/algorithm/` additionally carry a fixed set of *structural
dividers* — typography, not documentation, in the same sense as a blank line
or an access-specifier label:

```cpp
// ---- Construction -------------------------------------------------------
```

The dash fill extends the line to the 80-column limit, so a section boundary
is visible at any scroll speed.

The vocabulary is fixed, in this order:

- `Construction` — constructors and the copy/move operations,
- `Base access` — the `base()` quartet,
- `Setup` — the methods that seed or re-arm a run (`reset()`,
  `add_source()`, `set_source()`, …),
- `Execution` — the methods that drive it (`finished()` / `current()` /
  `advance()`, or `run()`),
- `Queries` — result accessors and the maps and ranges over them.

A divider marks where each group starts — or resumes, when a private
implementation block interrupts it; the private blocks themselves stay
unlabeled. A class omits the sections it lacks. Do not invent section names or
spread dividers beyond `include/melon/algorithm/`: a divider that can say
anything, anywhere, is no longer navigation but a comment, and then the
discriminator above applies to it.

## Versioning

The version number has a single source of truth:
`include/melon/version.hpp` (`MELON_VERSION_MAJOR` / `MINOR` / `PATCH`).
`CMakeLists.txt` and `conanfile.py` parse it — never edit a version number
anywhere else.

## Releasing

The checklist for cutting a release, in order — a step lives here rather than
in a private file precisely so it cannot be lost between releases:

1. Bump `include/melon/version.hpp` and move the `Unreleased` section of
   `CHANGELOG.md` under the new version number, dated.
2. Tag the release commit `v<MAJOR>.<MINOR>.<PATCH>` and push the tag. Both
   workflows trigger on `v*` tags: wait for the full CI matrix to pass **on
   the tag** — it may not be the commit the last `main` push validated — and
   for the docs deployment from the tagged commit.
3. Publish the Conan package: `conan create` from the tag, then submit the
   recipe bump to Conan Center.
4. Once the package is live on Conan Center, delete the stale
   pre-1.0 fallback instructions if any remain: the `melon/1.0.0-alpha.1`
   block in `docs/getting-started/installation.md` and the matching README
   paragraph were kept only until 1.0.0 landed there.
5. Create the GitHub release from the tag, with the changelog entry as its
   notes.

## License

melon is distributed under the [Boost Software License 1.0](LICENSE). By
submitting a contribution you agree to license it under the same terms.
