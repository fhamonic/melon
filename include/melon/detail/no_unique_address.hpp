#pragma once

// MSVC -- every version, VS 2026 included -- parses [[no_unique_address]] but
// ignores it, so empty members keep their bytes; only the
// [[msvc::no_unique_address]] spelling is honored. Writing the attribute
// directly instead of this macro silently loses the layout on MSVC, which the
// consumable_view size pins turn into a build failure.
#if defined(_MSC_VER) && !defined(__clang__)
#define MELON_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define MELON_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
