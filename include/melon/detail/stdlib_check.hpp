#pragma once

// <version> defines the __cpp_lib_* feature-test macros and, on libstdc++,
// _GLIBCXX_RELEASE. It has to be included before anything can be checked.
#include <version>

// A C++23-capable compiler does not imply a C++23 standard library. Clang 18
// against libstdc++ 12 accepts -std=c++23 and then fails deep inside melon
// with hundreds of lines of template diagnostics, because std::views::zip,
// std::views::enumerate and std::format are missing. These checks turn that
// into a single readable error.
//
// This cannot be expressed in the Conan recipe: its compiler.libcxx setting
// selects the ABI (libstdc++ vs libstdc++11), never the release, so for clang
// the standard library version is invisible to the package manager.

#if defined(__GLIBCXX__)

// The GCC 14 baseline melon documents is really a libstdc++ 14 baseline: the
// same requirement applies to clang built against libstdc++.
//
// To fix: use GCC 14 or newer, or point clang at a GCC 14+ toolchain --
// install libstdc++-14-dev, and if an older GCC is also present select it
// explicitly with --gcc-install-dir= (clang 18+) or --gcc-toolchain=.
#if !defined(_GLIBCXX_RELEASE) || _GLIBCXX_RELEASE < 14
#error "melon requires libstdc++ 14 or newer -- see stdlib_check.hpp"
#endif

#else

// melon is not tested against libc++ or the MSVC STL, so rather than invent a
// version number for them, require the features themselves.
#if !defined(__cpp_lib_format)
#error "melon requires std::format (__cpp_lib_format)."
#endif
#if !defined(__cpp_lib_ranges_zip)
#error "melon requires std::views::zip (__cpp_lib_ranges_zip)."
#endif
#if !defined(__cpp_lib_ranges_enumerate)
#error "melon requires std::views::enumerate (__cpp_lib_ranges_enumerate)."
#endif

#endif
